import json
import select
import socket
from syslog import \
    openlog, syslog, LOG_PID, LOG_PERROR, LOG_DAEMON, \
    LOG_DEBUG, LOG_ERR, LOG_WARNING


class BrokerUds:
    sd = None
    fh = None
    path = None

    def connect(self, path):
        self.path = path

        try:
            syslog("Connecting to: %s" % (path))

            self.sd = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

            self.sd.connect(path)
        except socket.error as e:
            syslog(LOG_ERR, "Error connecting to: %s: %s" % (path, e.strerror))
            return None

        syslog("Connected to: %s" % (path))

        self.fh = self.sd.makefile()

        return self.fh

    def read(self):

        jd = {"type": "noop"}

        fd_read = [self.sd]
        fd_write = []

        rd, wr, ex = select.select(fd_read, fd_write, fd_read, 1.0)

        if not len(rd):
            return jd

        try:
            data = self.fh.readline()
        except:
            return None

        if not data:
            return None

        jd = json.loads(data)

        return jd

    def close(self):
        if self.sd is not None:
            self.sd.close()