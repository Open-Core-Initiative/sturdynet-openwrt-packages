#-*- coding: ISO-8859-1 -*-
# collect.py: the python collectd-unixsock module.
#
# Requires collectd to be configured with the unixsock plugin, like so:
#
# LoadPlugin unixsock
# <Plugin unixsock>
#   SocketFile "/var/run/collectd-unixsock"
#   SocketPerms "0775"
# </Plugin>
#
# Copyright (C) 2008 Clay Loveless <clay@killersoft.com>
#
# This software is provided 'as-is', without any express or implied
# warranty.  In no event will the author be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.

import socket
import sys
import os
import time

class Collectd():

    def __init__(self, path='/var/run/collectd-unixsock', noisy=False):
        self.noisy = noisy
        self.path = path
        self._sock = self._connect()

    def flush(self, timeout=None, plugins=[], identifiers=[]):
        """Send a FLUSH command.

        Full documentation:
            http://collectd.org/wiki/index.php/Plain_text_protocol#FLUSH

        """
        # have to pass at least one plugin or identifier
        if not plugins and not identifiers:
            return None
        args = []
        if timeout:
            args.append("timeout=%s" % timeout)
        if plugins:
            plugin_args = ["plugin=%s" % x for x in plugins]
            args.extend(plugin_args)
        if identifiers:
            identifier_args = ["identifier=%s" % x for x in identifiers]
            args.extend(identifier_args)
        return self._cmd('FLUSH %s' % ' '.join(args))

    def getthreshold(self, identifier):
        """Send a GETTHRESHOLD command.

        Full documentation:
            http://collectd.org/wiki/index.php/Plain_text_protocol#GETTHRESHOLD

        """
        numvalues = self._cmd('GETTHRESHOLD "%s"' % identifier)
        lines = []
        if not numvalues or numvalues < 0:
            raise KeyError("Identifier '%s' not found" % identifier)
        lines = self._readlines(numvalues)
        return lines

    def getval(self, identifier, flush_after=True):
        """Send a GETVAL command.

        Also flushes the identifier if flush_after is True.

        Full documentation:
            http://collectd.org/wiki/index.php/Plain_text_protocol#GETVAL

        """
        numvalues = self._cmd('GETVAL "%s"' % identifier)
        lines = []
        if not numvalues or numvalues < 0:
            raise KeyError("Identifier '%s' not found" % identifier)
        lines = self._readlines(numvalues)
        if flush_after:
            self.flush(identifiers=[identifier])
        return lines

    def listval(self):
        """Send a LISTVAL command.

        Full documentation:
            http://collectd.org/wiki/index.php/Plain_text_protocol#LISTVAL

        """
        numvalues = self._cmd('LISTVAL')
        lines = []
        if numvalues:
            lines = self._readlines(numvalues)
        return lines

    def putnotif(self, message, options={}):
        """Send a PUTNOTIF command.

        Options must be passed as a Python dictionary. Example:
          options={'severity': 'failure', 'host': 'example.com'}

        Full documentation:
            http://collectd.org/wiki/index.php/Plain_text_protocol#PUTNOTIF

        """
        args = []
        if options:
            options_args = ["%s=%s" % (x, options[x]) for x in options]
            args.extend(options_args)
        args.append('message="%s"' % message)
        return self._cmd('PUTNOTIF %s' % ' '.join(args))

    def putval(self, identifier, values, options={}):
        """Send a PUTVAL command.

        Options must be passed as a Python dictionary. Example:
          options={'interval': 10}

        Values must be in a list. For example, ["N", tx_bytes, rx_bytes]
        It will be iterated through and produce a PUTVAL with ":" delimiter.
        Pay attention to types.db.

        Full documentation:
            http://collectd.org/wiki/index.php/Plain_text_protocol#PUTVAL

        """
        args = []
        args.append('"%s"' % identifier)
        if options:
            options_args = ["%s=%s" % (x, options[x]) for x in options]
            args.extend(options_args)
            values = list(map(str, values))
            args.append(':'.join(values))
        return self._cmd('PUTVAL %s' % ' '.join(args))

    def _cmd(self, c):
        try:
            return self._cmdattempt(c)
        except socket.error as e:
            (errno, errstr) = e.args
            sys.stderr.write("[error] Sending to socket failed: [%d] %s\n"
                             % (errno, errstr))
            self._sock = self._connect()
            return self._cmdattempt(c)

    def _cmdattempt(self, c):
        if self.noisy:
            print("[send] %s" % c)
        if not self._sock:
            sys.stderr.write("[error] Socket unavailable. Can not send.")
            return False
        self._sock.send((c + "\n").encode())
        status_message = self._readline()
        if self.noisy:
            print("[receive] %s" % status_message)
        if not status_message:
            return None
        code, message = status_message.split(' ', 1)
        if int(code):
            return int(code)
        return False

    def _connect(self):
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(self.path)
            if self.noisy:
                print("[socket] connected to %s" % self.path)
            return sock
        except socket.error as e:
            (errno, errstr) = e.args
            sys.stderr.write("[error] Connecting to socket failed: [%d] %s \n"
                             % (errno, errstr))
            sys.stderr.write("[l7stats_patch] fixing socket error\n")
            try:
                assert 0 == os.system("rm " + self.path)
                assert 0 == os.system("/etc/init.d/luci_statistics restart")
            except AssertionError:
                sys.stderr.write("[l7stats_patch] failed\n")
                return None
            else:
                sys.stderr.write("[l7stats_patch] succeeded\n")
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                while not os.path.exists(self.path):
                    time.sleep(.05)
                sock.connect(self.path)
                return sock

    def _readline(self):
        """Read single line from socket"""
        if not self._sock:
            sys.stderr.write("[error] Socket unavailable. Can not read.")
            return None
        try:
            data = ''
            buf = []
            recv = self._sock.recv
            while data != "\n":
                data = recv(1)
                data = data.decode('utf-8')
                if not data:
                    break
                if data != "\n":
                    buf.append(data)
            return ''.join(buf)
        except socket.error as e:
            (errno, errstr) = e.args
            sys.stderr.write("[error] Reading from socket failed: [%d] %s"
                             % (errno, errstr))
            self._sock = self._connect()
            return None

    def _readlines(self, sizehint=0):
        """Read multiple lines from socket"""
        total = 0
        list = []
        while True:
            line = self._readline()
            if not line:
                break
            list.append(line)
            total = len(list)
            if sizehint and total >= sizehint:
                break
        return list

    def __del__(self):
        if not self._sock:
            return
        try:
            self._sock.close()
        except socket.error as e:
            (errno, errstr) = e.args
            sys.stderr.write("[error] Closing socket failed: [%d] %s"
                             % (errno, errstr))
