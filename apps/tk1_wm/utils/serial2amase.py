#!/usr//bin/python

import serial
import socket
import signal
import sys
import struct

def signal_handler(signal, frame):
    print 'closing socket'
    s.close()
    sys.exit()

#s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#s.connect(("127.0.0.1", 5555))

signal.signal(signal.SIGINT, signal_handler)

numBytes = 0
with serial.Serial('/dev/ttyUSB1', 115200, timeout=100) as ser:
    while True:
        #lengthba = ser.read(4);
        #length = struct.unpack("!I", lengthba)[0]
        #print('Received message of %u, waiting to receive %u' % (length, length - 4))
        c = ser.read()
        print "%x %u" % (ord(c),numBytes) 
        numBytes += 1
        #res = s.sendall(c)
        #print res
        #print "%x %u" % (ord(c),numBytes) 
        #sys.stdout.write(format(c, "s"))
        #sys.stdout.flush()


