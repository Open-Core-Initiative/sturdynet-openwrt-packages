import json
import socket
import errno
import os
from queue import Queue
from threading import Thread
import hashlib
from syslog import \
    openlog, syslog, LOG_PID, LOG_PERROR, LOG_DAEMON, \
    LOG_DEBUG, LOG_ERR, LOG_WARNING, LOG_INFO
import ubus
from uci import Uci
import time


def get_config():
    u_iface = u.get("flow_broker", "main", "ext_iface")
    print(u_iface)
    d = gen_int_dict(u_iface)
    syslog(LOG_INFO, f"Using interfaces and IPs: {d}")
    return d


def gen_int_dict(u_iface):
    iface_dict = {}
    ubus.connect("/var/run/ubus/ubus.sock")
    for i in u_iface:
        iface = "network.interface." + i
        counter = 1
        while counter < 3:
            try:
                i_list = ubus.call(iface, "status", {})
            except Exception as e:
                syslog(LOG_ERR, f"Interface {i} not not found.")
                break
            if_dict = i_list[0]
            if "l3_device" in if_dict.keys():
                iface_dict.update({if_dict["l3_device"]: if_dict["ipv4-address"][0]["address"]})
                break
            elif "l3_device" not in if_dict.keys():
                syslog(LOG_WARNING, f"Interface {i} does not have an IP address. Try {counter}.")
                counter += 1
                time.sleep(5)
                continue
            elif counter == 3:
                syslog(LOG_ERR, f"l3_device for {i} not available.")
    try:
        if u.get("flow_broker", "main", "464xlat") == "1":
            iface_dict.update({u.get("flow_broker", "main", "464iface"): u.get("flow_broker", "main", "464ip")})
    except Exception as e:
        syslog(LOG_ERR, f"Failed to get xlat configuration.")
    return iface_dict


def print_pkt(pkt):
    #print_pkt_data = (str(pkt["src_ip"]) + " " + str(pkt["src_port"]) + " " +
    #                  str(pkt["dest_ip"]) + " " + str(pkt["dest_port"]) + " " + str(pkt["ip.totlen"]))
    print_pkt_data = str(pkt)
    syslog(LOG_DEBUG, print_pkt_data)


def server(sq):
    path = "/var/run/l7stats.sock"
    try:
        os.unlink(path)
    except OSError as e:
        syslog(LOG_ERR, f"Cannot unlink file: {path}")
        if os.path.exists(path):
            raise
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    syslog(LOG_INFO, "Starting up stats socket")
    s.bind(path)
    s.listen()
    disconn = True
    while True:
        if disconn:
            syslog(LOG_INFO, f"Waiting for a connection on {s}")
            conn, addr = s.accept()
            syslog(LOG_INFO, f"Accepted connection on {s}")
            disconn = False
        while True:
            msg = sq.get()
            if not len(msg):
                continue
            try:
                conn.sendall(msg)
            except Exception as e:
                syslog(LOG_ERR, f"Could not send data on {conn}")
                disconn = True
                break
        conn.close()


def pkt_thread(sq, int):
    disconn = True
    p = socket.socket()

    syslog(LOG_INFO, "Starting up on pkt socket")
    p.bind(('127.0.0.1', 6001))
    rbytes = 0
    tbytes = 0
    rh_data = ""
    th_data = ""
    rret = 0
    tret = 0
    p.listen()

    while True:
        if disconn:
            syslog(LOG_INFO, f"Waiting for a connection on {p}")
            p_conn, ulog_addr = p.accept()
            syslog(LOG_INFO, f"Accepted connection from: {p}")
            try:
                p_fh = p_conn.makefile()
            except Exception as e:
                syslog(LOG_ERR, f"Cannot read socket: Restarting {p_conn}")
            disconn = True
        while True:
            try:
                p_data = p_fh.readline()
            except Exception as e:
                syslog(LOG_ERR, f"Lost connection to packet socket: {e}")
                p_conn.close()
                disconn = True

            if not p_data:
                break

            p_jd = json.loads(p_data)
            #print_pkt(p_jd)
            if p_jd["ip.protocol"] == 1:
                continue
            elif "src_port" not in p_jd:
                continue
            try:
                if p_jd["oob.out"] != "":
                    p_jd["src_ip"] = int[p_jd["oob.out"]]
                    if debug == 1:
                        print_pkt(p_jd)
                    h_data = (str(p_jd["src_ip"]) + str(p_jd["src_port"]) +
                              str(p_jd["dest_ip"]) + str(p_jd["dest_port"])).replace(".", "")
                    if th_data == h_data:
                        tbytes += p_jd["ip.totlen"]
                        tret = 1
                        continue
                    elif th_data != h_data and tret == 1:
                        th_data = hashlib.sha1(th_data.encode())
                        th_data = str(th_data.hexdigest())
                        q_data = {"type": "flow_update_tx", "flow": {"digest": th_data,
                                                                     "iface": p_jd["oob.out"], "t_bytes": tbytes}}
                        tbytes = p_jd["ip.totlen"]
                        th_data = h_data
                        tret = 1
                    else:
                        tbytes = p_jd["ip.totlen"]
                        th_data = h_data
                        tret = 1
                        continue

                else:
                    if debug == 1:
                        print_pkt(p_jd)
                    h_data = (str(p_jd["dest_ip"]) + str(p_jd["dest_port"]) +
                              str(p_jd["src_ip"]) + str(p_jd["src_port"])).replace(".", "")
                    if rh_data == h_data:
                        rbytes += p_jd["ip.totlen"]
                        rret = 1
                        continue
                    elif rh_data != h_data and rret == 1:
                        rh_data = hashlib.sha1(rh_data.encode())
                        rh_data = str(rh_data.hexdigest())
                        q_data = {"type": "flow_update_rx", "flow": {"digest": rh_data,
                                                                     "iface": p_jd["oob.in"], "r_bytes": rbytes}}
                        rbytes = p_jd["ip.totlen"]
                        rh_data = h_data
                        rret = 1
                    else:
                        rbytes = p_jd["ip.totlen"]
                        rh_data = h_data
                        rret = 1
                        continue
            except Exception as e:
                p_data = {"KeyError": e}
                syslog(LOG_ERR, f"Must have a KeyError in packet thread: {e}")
                continue
            q_data = json.dumps(q_data)
            q_data = q_data + "\n"
            try:
                sq.put((str(q_data).encode("utf-8")))
            except IOError as e:
                if e.errno == errno.EPIPE:
                    break


def flow_thread(sq):
    disconn = True
    f = socket.socket()

    syslog(LOG_INFO, "Starting up flow socket")
    f.bind(('127.0.0.1', 6000))

    f.listen()

    while True:
        if disconn:
            syslog(LOG_INFO, f"Waiting for a connection on {f}")
            f_conn, ulog_addr = f.accept()
            syslog(LOG_INFO, f"Accepted connection from: {f}")
            try:
                f_fh = f_conn.makefile()
            except Exception as e:
                syslog(LOG_ERR, f"Cannot read socket: Restarting {f_conn}")
            disconn = True
        while True:
            try:
                f_data = f_fh.readline()
            except Exception as e:
                syslog(LOG_ERR, f"Lost connection to flow socket: {e}")
                f_conn.close()
                disconn = True

            if not f_data:
                break

            f_jd = json.loads(f_data)
            if f_jd is None:
                syslog(LOG_ERR, "We have no data on f_jd")
                continue
            if "orig.ip.protocol" not in f_jd.keys():
                syslog(LOG_ERR, "Still no data in f_jd")
                continue
            if f_jd["orig.ip.protocol"] == 1 or f_jd["orig.ip.protocol"] == 58:
                continue
            else:
                try:
                    h_data = (str(f_jd["reply.ip.daddr.str"]) + str(f_jd["reply.l4.dport"])
                              + str(f_jd["reply.ip.saddr.str"]) + str(f_jd["reply.l4.sport"])).replace(".", "")
                    h_data = hashlib.sha1(h_data.encode())
                    h_data = h_data.hexdigest()
                    f_data = {"type": "purge", "flow": {"digest": str(h_data)}}
                except Exception as e:
                    f_data = {"KeyError": e}
                    syslog(LOG_ERR, f"Must have a KeyError in flow thread: {e}")
                    print_flow = str(f_jd)
                    syslog(LOG_ERR, print_flow)
                    continue
                f_data = json.dumps(f_data)
                f_data = f_data + "\n"
                try:
                    sq.put((str(f_data).encode("utf-8")))
                except IOError as e:
                    if e.errno == errno.EPIPE:
                        break


if __name__ == "__main__":
    time.sleep(10)
    q = Queue(maxsize=0)
    u = Uci()

    i_dict = get_config()
    debug = u.get("flow_broker", "main", "debug")

    s_proc = Thread(target=server, args=(q,), daemon=True)
    p_proc = Thread(target=pkt_thread, args=(q, i_dict,))
    f_proc = Thread(target=flow_thread, args=(q,))

    syslog(LOG_INFO, "Flow Broker Starting")

    s_proc.start()
    p_proc.start()
    f_proc.start()

    os.system("/etc/init.d/firewall restart")
    syslog(LOG_INFO, "Firewall restarted")
    os.system("/etc/init.d/ulogd restart")
    syslog(LOG_INFO, "Ulogd  restarted")