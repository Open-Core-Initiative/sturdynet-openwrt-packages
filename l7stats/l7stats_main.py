# openwrt Engine
# Copyright (C) 2022 IPSquared, Inc. <https://www.ipsquared.com)>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

###############
#
# Written by:
#
# gchadda
# mfoxworthy
#
###############
import hashlib
import os
import threading
import signal
import ast
import time

from l7stats_netifyd_uds import netifyd
from l7stats_broker_uds import BrokerUds
from random import randint
from l7stats_flow_manager import CollectdFlowMan
from syslog import \
    openlog, syslog, LOG_PID, LOG_PERROR, LOG_DAEMON, \
    LOG_DEBUG, LOG_ERR, LOG_WARNING, LOG_INFO


def round_float_by_precision(inval, p=-1):
    if -1 == p:
        p = TIMING_PRECISION
    return round(float(inval), p)


def update_data(e, t, fl):
    b = -1
    while not e.isSet():
        timing_bias = 0
        if b >= 0:
            timing_bias -= round_float_by_precision(time.time())
            fl.sendappdata(t)
            timing_bias += round_float_by_precision(time.time())
        else:
            b += 1

        if timing_bias < t and timing_bias > 0:
            t_delta = round_float_by_precision(t - timing_bias)
            if LOGGING == 1:
                syslog(LOG_INFO, f"normalized sleep to {t_delta}")
            time.sleep(t_delta)
        else:
            if LOGGING == 1:
                syslog(LOG_INFO, f"using default sleep timer, timing_bias is: {timing_bias}")
            time.sleep(t)


def cleanup():
    global nd
    global bd
    global eh
    nd.close()
    bd.close()
    eh.set()


def sig_handler(s, f):
    if LOGGING == 1:
        syslog(LOG_ERR, f"Received stack {repr(s)} on frame {repr(f)}")
    cleanup()
    exit(0)


def netify_thread():
    global nd
    fh = nd.connect(SOCKET_ENDPOINT)
    global fl

    for fp in (APP_PROTO_FILE, APP_CAT_FILE):
        with open(fp, mode="r") as f:
            s = f.read()

        if isinstance(s, str):
            app_to_cat[fp] = ast.literal_eval(s)
            assert isinstance(app_to_cat[fp], dict) == True
        else:
            raise RuntimeError("app mapping failure..")

    while True:

        try:

            jd = nd.read()
            if jd is None:
                nd.close()
                fh = None
                if LOGGING == 1:
                    syslog(LOG_INFO, f"backing off for {SLEEP_PERIOD}..")
                time.sleep(SLEEP_PERIOD)
                nd = netifyd()
                fh = nd.connect(SOCKET_ENDPOINT)
                continue

            if jd['type'] == 'noop':
                if LOGGING == 1:
                    syslog(LOG_DEBUG, "detected noop, continuing...")
                continue

            if all(['flow_count' in jd.keys(), 'flow_count_prev' in jd.keys()]):
                if LOGGING == 1:
                    syslog(LOG_INFO,
                           f"flow count == {jd['flow_count']}{os.linesep}previous flow count == {jd['flow_count_prev']}" \
                           + f"{os.linesep}delta == {-nd.flows_delta}")
                continue

            if jd['type'] == 'flow':
                if jd['flow']['other_type'] != 'remote': continue

                if jd['internal']: continue

                if jd['flow']['ip_protocol'] != 6 and \
                        jd['flow']['ip_protocol'] != 17 and \
                        jd['flow']['ip_protocol'] != 132 and \
                        jd['flow']['ip_protocol'] != 136: continue

                if jd['flow']['detected_protocol'] == 5 or \
                        jd['flow']['detected_protocol'] == 8: continue

                app_name_str = jd['flow']['detected_application_name']
                app_int, *app_trail = app_name_str.split("netify")
                if 1 == len(app_trail):
                    app_int = app_int.rstrip(".")
                    app_name = app_trail[0].lstrip(".")
                    if LOGGING == 1:
                        syslog(LOG_INFO, f"app_int == {app_int}, app_name = {app_name}")
                    app_cat = app_to_cat[APP_CAT_FILE]['applications'][str(app_int)]
                    app_cat_name = app_to_cat[APP_PROTO_FILE]['application_category'][str(app_cat)]['tag']
                else:
                    if LOGGING == 1:
                        syslog(LOG_INFO, f"failure.... read in {app_name_str}, unable to parse further")
                    app_name = "unknown"
                    app_cat = 0
                    app_cat_name = "unknown"
                if LOGGING == 1:
                    syslog(LOG_INFO, f"app_cat = {app_cat}, app_cat_name = {app_cat_name}")

                iface_name = jd['interface']

                detected_protocol = jd['flow']['detected_protocol']
                detected_protocol_mapping = app_to_cat[APP_CAT_FILE]['protocols'][str(detected_protocol)]
                protocol_mapping_name = app_to_cat[APP_PROTO_FILE]['protocol_category'][str(detected_protocol_mapping)][
                    'tag']

                if LOGGING == 1:
                    syslog(LOG_INFO,
                           f"detected_protocol_mapping={detected_protocol_mapping}, protocol_mapping_name  = {protocol_mapping_name}")
                digest = (str(jd['flow']['local_ip']) + str(jd['flow']['local_port']) + str(jd['flow']['other_ip'])
                          + str(jd['flow']['other_port'])).replace(".", "")
                digest = hashlib.sha1(digest.encode())
                digest = str(digest.hexdigest())

                if digest:
                    fl.addflow(digest, app_name, app_cat_name, iface_name)

            if jd['type'] == 'agent_status':
                """ we explicitly ignore agent_status ; not implemented """
                pass

        except KeyError as ke:
            syslog(LOG_ERR, f"hit key error for : {ke}")
            syslog(LOG_ERR, str(jd))
            continue
        except Exception as e:
            syslog(LOG_ERR, f"hit general exception: {e}")
            continue


def broker_thread():
    global bd
    fhb = bd.connect(BROKER_SOCKET_ENDPOINT)
    global fl

    while True:
        jd = bd.read()

        if jd is None:
            bd.close()
            fhb = None
            if LOGGING == 1:
                syslog(LOG_INFO, f"backing off for {SLEEP_PERIOD}..")
            time.sleep(SLEEP_PERIOD)
            bd = BrokerUds()
            fhb = bd.connect(BROKER_SOCKET_ENDPOINT)
            continue

        if jd['type'] == 'purge':
            try:
                fl.purgeflow(jd['flow']['digest'])
            except Exception as e:
                syslog(LOG_ERR, f"Failed to purge flow: {jd['flow']['digest']}")
                continue

        elif jd['type'] == 'flow_update_rx':
            try:
                fl.updateflow(jd['flow']['digest'], jd['flow']['iface'], 0, jd['flow']['r_bytes'])
            except Exception as e:
                syslog(LOG_ERR, f"Failed to update flow with rx bytes: {jd['flow']['digest']}")
                continue

        elif jd['type'] == 'flow_update_tx':
            try:
                fl.updateflow(jd['flow']['digest'], jd['flow']['iface'], jd['flow']['t_bytes'], 0)
            except Exception as e:
                syslog(LOG_ERR, f"Failed to update flow with tx bytes: {jd['flow']['digest']}")
                continue
        else:
            continue


# start off a thread to report data every APP_UPDATE_ITVL secs
if __name__ == "__main__":
    SOCKET_ENDPOINT = "unix:///var/run/netifyd/netifyd.sock"
    BROKER_SOCKET_ENDPOINT = "/var/run/l7stats.sock"
    SLEEP_PERIOD = randint(1, 5)
    APP_UPDATE_ITVL = 10
    app_to_cat = dict()
    APP_PROTO_FILE = "/etc/netify-fwa/app-proto-data.json"
    APP_CAT_FILE = "/etc/netify-fwa/netify-categories.json"
    TIMING_PRECISION = 1
    LOGGING = 0
    fl = CollectdFlowMan()
    nd = netifyd()
    bd = BrokerUds()
    eh = threading.Event()

    lock = threading.Lock()
    threading.Thread(target=netify_thread, daemon=True).start()
    threading.Thread(target=broker_thread, daemon=True).start()

    threading.Thread(target=update_data, args=(eh, APP_UPDATE_ITVL, fl)).start()

    signal.signal(signal.SIGHUP, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)
    signal.signal(signal.SIGINT, sig_handler)
