"""
this module contains the logic necessary to actually drive the ftdi/ arduino stack.
the idea is the slave will signal when it's ready to display a message, and the master will sit
around waiting for that signal. then send the message.

we give the Master instance a generator of strings and it chops them to an appropriate length,
converts them to bytestrings and sends them along to the slave at the pace dictated by the logic
described above.
"""

import serial
import time

import numpy as np

# this is where the FTDI usually seems to connect
FTDI_PREFIX = "/dev/ttyUSB"
# although these days we're not using ftdi
USB_PREFIX = "/dev/ttyACM"
# try this many port suffixes to the prefix above
NUM_PORTS = 10

# this is the highest rate the poor little arduino will support. turn down to 19200 if it drops
# characters or otherwise doesn't work as expected.
BAUD_RATE = 28800

# sleep in short intervals while waiting for the sign to get done
WAIT_INTERVAL = 0.001



# give the slave a moment to connect
SETTLE_TIME = 2

# we should probably send ascii characters
ENCODING = "ascii"

END_MARKER = "\r"

# 'ignore' or 'replace'
ASCII_MODE = "ignore"




class Master(object):
    """
    opens and maintains a connection to the arduino, and exposes a method to send it lines.
    """

    def __init__(self, **kwargs):
        # replace any of the below with kwargs.
        self.baud_rate = BAUD_RATE
        self.port_prefix = USB_PREFIX
        self.wait_interval = WAIT_INTERVAL
        self.encoding = ENCODING
        self.end_marker = END_MARKER
        self.mode = ASCII_MODE

        for key, value in kwargs.items():
            self.__setattr__(key, value)

        self.comm = serial.Serial(stopbits=serial.STOPBITS_ONE, baudrate=self.baud_rate)
        self.comm.timeout = self.wait_interval


    def just_connect(self):
        """
        open a serial connection to the whatever it is on whatever the fuck port it's on
        """
        for n in range(NUM_PORTS):
            port = self.port_prefix + str(n)
            try:
                self.comm.port = port
                self.comm.open()
            except FileNotFoundError:
                print("still connecting...")
            else:
                break
        time.sleep(SETTLE_TIME)
        print("connected")

    def send_it(self, line):
        """
        send a single line to the sign. blocks until it's done.
        """
        while not self.comm.read():
            continue
        self.comm.write((line + self.end_marker).encode(self.encoding, self.mode))

