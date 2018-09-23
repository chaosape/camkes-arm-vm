#!/usr/bin/python3

import asyncio
import serial_asyncio
import struct
import socket
import serial
from time import sleep

def hexdump(src, length=16):
    FILTER = ''.join([(len(repr(chr(x))) == 3) and chr(x) or '.' for x in range(256)])
    lines = []
    for c in range(0, len(src), length):
        chars = src[c:c+length]
        hex = ' '.join(["%02x" % x for x in chars])
        printable = ''.join(["%s" % ((x <= 127 and FILTER[x]) or '.') for x in chars])
        lines.append("%04x  %-*s  %s\n" % (c, length*3, hex, printable))
    return ''.join(lines)

async def main(loop):
    serial_reader = 0
    serial_writer = 0

    serial_reader, serial_writer = await serial_asyncio.open_serial_connection(
        url='/dev/ttyUSB1',
        baudrate=115200)


    tcp_reader, tcp_writer = await asyncio.open_connection('127.0.0.1', 5555)
    
    sent = send_hack(serial_writer, tcp_reader)
    received = recv(serial_reader, tcp_writer)
    await asyncio.wait([sent,received])
    #await asyncio.wait([received])
    #await asyncio.wait([sent])

#+=+=+=+=539#@#@#@#@

async def send_hack(w,s):
    while True:
        idba = await s.read(8)
        id = struct.unpack("!Q",idba)[0];
        if(id != 400):
            continue
        waypointba = await s.read(8)
        waypoint = struct.unpack("!Q",waypointba)[0];
        print("Sending waypoint {} for id {}".format(waypoint,id))
        w.write(waypointba)
        await w.drain()

async def send(w,s):
    while True:
        restart = False
        # Expecting +=+=+=+=
        ba = await s.read(8)
        sentinel = ""
        try:
            sentinel = ba.decode('utf-8')
        except:
            continue
        
        if(sentinel != "+=+=+=+="):
            print("Bad first sentinel! Expecting '+=+=+=+=' but received '%s'."
                  % sentinel)
            continue

        string_size = ""
        while(1):
            c = await s.read(1)
            if(c[0] < 48 or c[0] > 57):
                restart = c[0] != 35
                break;
            try:
                string_size += c.decode('utf-8')
            except:
                restart = true
                break;

        if(restart):
            continue

        length = 0
        try:
            length = int(string_size)
        except:
            continue

        print("Received message reporting to be of length %u." % length)

        ba = await s.read(7)

        try:
            sentinel = ba.decode('utf-8')
        except:
            continue
        
        if(sentinel != "@#@#@#@"):
            print("Bad second sentinel! Expecting '#@#@#@#@' but received '%s'."
                  % ba.toString())
            continue

        msg = await s.read(length)


        ba = await s.read(8)

        try:
            sentinel = ba.decode('utf-8')
        except:
            continue
        
        if(sentinel != "!%!%!%!%"):
            print("Bad third sentinel! Expecting '!%!%!%!%' but received '%s'."
                  % ba.toString())
            continue

 
        #ignore checksum
        while(1):
            c = await s.read(1)
            if(c[0] < 48 or c[0] > 57):
                restart = c[0] != 63
                break;
            
        if(restart):
            continue
        
        ba = await s.read(7)
        try:
            sentinel = ba.decode('utf-8')
        except:
            continue
        
        if(sentinel != "^?^?^?^"):
            print("Bad first sentinel! Expecting '?^?^?^?^' but received '%s'."
                  % ba.toString())
            continue

        
        startidx = 0
        queue = [0,0,0,0,0,0]

        for i in range(0,len(msg)):
            sentinel = bytearray(filter(None, queue)).decode('utf-8')
            if(sentinel == "||0|0$"):
                startidx = i
                break;
            c = msg[i]
            queue.append(c)
            queue.pop(0)

            
        msg = msg[startidx:]

        size = bytearray(struct.pack('!I',len(msg)))

        if(length != 539):
            continue
        
        print("Sending %u bytes on serial." % len(msg))
        w.write(size)
        await w.drain()
        w.write(msg)
        await w.drain()            

async def recv(r,s):
    while True:
        lengthba = await r.readexactly(4)
        length = struct.unpack("!I", lengthba)[0]

        print(length)
        length = length - 4
        print("Reading %u." % length)
        msg = await r.readexactly(length)
        print("Read %u." % len(msg))
        if(len(msg) < length):
            print("Serial communications no longer consistent.")
            break;
        calcchksum = sum(bytearray(msg[0:length-4]))            
        sentchksum = struct.unpack("!I", msg[length-4:])[0]
        
        if( calcchksum != sentchksum ):
            print('BAD CHECKSUM! Not forwarding to TCP connection!\n')
            print('Calculate %u' % calcchksum)
            print('Received %u' % sentchksum)
        else:
            header = "#@#@#@#@$$"
            final =  header.encode() + msg
            print("Sending %u bytes to Amase" % len(final))
            s.write(final);
            await s.drain();
        
loop = asyncio.get_event_loop()
loop.run_until_complete(main(loop))
loop.close()
