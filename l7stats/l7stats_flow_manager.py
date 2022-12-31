# openwrt Engine
# Copyright (C) 2022 IPSquared, Inc. <https://www.ipsquared.com>
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
# Written by:
#
# mfoxworthy
# gchadda
#
################


from threading import RLock
from l7stats_collectd_uds import Collectd
import socket
from syslog import \
    openlog, syslog, LOG_PID, LOG_PERROR, LOG_DAEMON, \
    LOG_DEBUG, LOG_ERR, LOG_WARNING, LOG_INFO, LOG_CRIT


class CollectdFlowMan:

    def __init__(self):
        self._flow = {}
        self._map = {}
        self._app = {}
        self._cat = {}
        self._csocket = Collectd()
        self._lock = RLock()

    def addflow(self, dig, app, cat, iface):
        app_int_name = app + "_" + iface
        cat_int_name = cat + "_" + iface
        if app_int_name not in self._app.keys():
            with self._lock:
                self._app.update({app_int_name: {"bytes_tx": 0, "bytes_rx": 0}})
        if cat_int_name not in self._cat.keys():
            with self._lock:
                self._cat.update({cat_int_name: {"bytes_tx": 0, "bytes_rx": 0}})
        with self._lock:
            print("Updating flow data", dig)
            self._map.update({dig: {"app_name": app, "app_cat": cat, "iface_name": iface}})

    def _delflow(self, dig):
        if dig not in self._flow.keys():
            syslog(LOG_WARNING, "Digest not found...\n", dig)
        else:
            _ = self._flow.pop(dig, None)
            _ = self._map.pop(dig, None)

    def purgeflow(self, dig):
        has_dig = dig in self._flow.keys()
        if has_dig:
            with self._lock:
                self._flow[dig]["purge"] = 1

    def updateflow(self, dig, iface, tx_bytes, rx_bytes):
        has_dig = dig in self._flow.keys()
        if has_dig:
            with self._lock:
                self._flow[dig]["bytes_tx"] += tx_bytes
                self._flow[dig]["bytes_rx"] += rx_bytes
        else:
            self._flow.update({dig: {"iface_name": iface, "bytes_tx": tx_bytes, "bytes_rx": rx_bytes, "purge": 0}})

    def sendappdata(self, interval):
        interval = {"interval": interval}
        for i in list(self._flow):
            if i in self._map.keys():
                flow_id = str(self._flow[i])
                c_app = self._map[i]["app_name"] + "_" + self._map[i]["iface_name"]
                c_cat = self._map[i]["app_cat"] + "_" + self._map[i]["iface_name"]
                with self._lock:

                    b_tx = self._flow[i]['bytes_tx']
                    b_rx = self._flow[i]['bytes_rx']
                    self._app[c_app]['bytes_tx'] += b_tx
                    self._app[c_app]['bytes_rx'] += b_rx
                    self._cat[c_cat]['bytes_tx'] += b_tx
                    self._cat[c_cat]['bytes_rx'] += b_rx
                    if self._flow[i]['purge'] == 1 and flow_id not in self._map.keys():
                        app = "unknown" + "_" + self._flow[i]["iface_name"]
                        self.addflow(flow_id, "unknown", "unknown", self._flow[i]["iface_name"])
                        self._app[app]['bytes_tx'] += b_tx
                        self._app[app]['bytes_rx'] += b_rx
                        self._cat[app]['bytes_tx'] += b_tx
                        self._cat[app]['bytes_rx'] += b_rx
                        self._delflow(i)
                    elif self._flow[i]['purge'] == 1:
                        self._delflow(i)
                    else:
                        if b_tx != self._flow[i]['bytes_tx']:
                            print("Data came in on TX")
                        elif b_rx != self._flow[i]['bytes_rx']:
                            print("Data came in on RX")
                        self._flow[i]['bytes_tx'] = 0
                        self._flow[i]['bytes_rx'] = 0
        try:
            hostname = socket.gethostname()
        except Exception as e:
            hostname = "default_sturdynet"
            syslog(LOG_CRIT, "Please set the hostname")
        with self._lock:
            for i in list(self._app):
                x = i.split("_")
                app_name = x[0].replace("-", "_")
                i_name = x[1].replace("-", "_")
                app_id_rxtx = hostname + "/application-" + app_name + "/if_octets-" + i_name
                app_txbytes = self._app[i]['bytes_tx']
                app_rxbytes = self._app[i]['bytes_rx']
                app_cd_if = ["N", app_rxbytes, app_txbytes]
                self._csocket.putval(app_id_rxtx, app_cd_if, interval)

            for i in list(self._cat):
                x = i.split("_")
                cat_name = x[0].replace("-", "_")
                i_name = x[1].replace("-", "_")
                cat_id_rxtx = hostname + "/category-" + cat_name + "/if_octets-" + i_name
                cat_txbytes = self._cat[i]['bytes_tx']
                cat_rxbytes = self._cat[i]['bytes_rx']
                cat_cd_if = ["N", cat_rxbytes, cat_txbytes]
                self._csocket.putval(cat_id_rxtx, cat_cd_if, interval)

    def printdict(self):
        print(self._flow)
        print("\n")
