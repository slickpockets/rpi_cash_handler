#!/usr/bin/env python
#gen1 9/5 *recieved


from __future__ import print_function
from subprocess import Popen, PIPE
from Crypto.Hash import SHA256
from threading import Thread
import sys, time, shlex, math, re
import serial, time, binascii
import datetime
import redis
import os
from pathlib import Path

db = redis.Redis('localhost', 6379)
devs = os.listdir("/dev/serial/by-id")
for dev in devs:
    if dev == "usb-FTDI_USB__-__Serial-if00-port0":
        Port = Path("/dev/serial/by-id/" + dev).resolve()
    else:
        pass

print(Port)

timestamp = datetime.datetime.utcnow().isoformat()
num = db.get("transaction:count")
transaction = ("transaction:%s" % num)
db.hset(transaction, "timestamp", timestamp)
db.hset("transaction:current", "timestamp", timestamp)
db.incr("transaction:count", 1)

currencies = {"USD": [1,2,5,10,20,50,100], "AUD": [5,10,20,50,100]}
fiat = currencies['USD']

escrowed = False
ackBit = 0
apexModel = 0
apexVersion = 0
billCredit = 0
billCount = bytearray([0,0,0,0,0,0,0,0])
laststatus = ''
QR_code = ''
stop_scan = False

sys.excepthook = lambda *args: None

def chksum(msg):
    chk = 0
    stop = len(msg) - 2
    for x in range(1, stop):
        chk ^= msg[x]
    return chk

##############################
#
##############################
def scan_money():
    global stop_scan
    global ackBit
    global escrowed
    global laststatus
    global apexModel

    print("scan started")
    # Connect to the bill scanner
    ser = serial.Serial(
                    port=str(Port),
                    baudrate=9600,
                    bytesize=serial.SEVENBITS,
                    parity=serial.PARITY_EVEN,
                    stopbits=serial.STOPBITS_ONE
                    )
    print("scan running<br>")
    while stop_scan == False and ser.isOpen():
        # Only run while a receipt has not been generated.
        try:
            #The receipt is a file that is posted to a directory
            with open('input/receipt'):
                stop_scan = True
                print("Receipt File found<br>")
        except IOError:
            stop_scan = False

        # description of protocol can be found here:
        # http://www.vending.org/technical/MDB_3.0.pdf
        # extra info here:
        # http://www.pyramidacceptors.com/files/RS_232.pdf

        # basic message   0      1      2      3      4      5      6      7
        #               start,   len,   ack, bills,escrow,resv'd,  end, checksum
        msg = bytearray([0x02,  0x08,  0x10,  0x7F,  0x10,  0x00,  0x03,  0x00])

        msg[2] = 0x10 | ackBit
        if (ackBit == 1):
            ackBit = 0
        else:
            ackBit = 1

        if (escrowed):
            msg[4] |= 0x20

        #calculate checksum of message for Byte 7
        msg[7] = chksum(msg)

        #print(">> %s" % binascii.hexlify(msg))

        ser.write(msg)
        time.sleep(0.1)

        out = ''
        while ser.inWaiting() > 0:
            out += ser.read(1)
        if (out == ''): continue

        #print "<<", binascii.hexlify(out)

        status = ""
        if (ord(out[3]) & 1): status += " IDLING "
        if (ord(out[3]) & 2): status += " ACCEPTING "
        if (ord(out[3]) & 4):
            status += " ESCROWED "
            escrowed = True
        else:
            escrowed = False
        if (ord(out[3]) & 8): status += " STACKING "
        if (ord(out[3]) & 0x10): status += " STACKED "
        if (ord(out[3]) & 0x20): status += " RETURNING "
        if (ord(out[3]) & 0x40): status += " RETURNED "

        if (ord(out[4]) & 1): status += " CHEATED! "
        if (ord(out[4]) & 2): status += " REJECTED "
        if (ord(out[4]) & 4): status += " JAMMED! "
        if (ord(out[4]) & 8): status += " FULL! "
        if (ord(out[4]) & 0x10): status += " w/CASSETTE "
        if (laststatus != status):
            print ('Acceptor status:' + status)
            laststatus = status

        if (ord(out[5]) & 1): print ('Acceptor powering up / initializing.')
        if (ord(out[5]) & 2): print ('Acceptor received invalid command.')
        if (ord(out[5]) & 4): print ('Acceptor has failed!')

        if (len(out) > 8):
            if (out[7] != apexModel or out[8] != apexVersion):
                apexModel = out[7]
                apexVersion = out[8]
                print ("Connected to Acceptor model" + binascii.hexlify(apexModel) + "FW ver" + binascii.hexlify(apexVersion))

        #print status
        billCredit = ord(out[5]) & 0x38
        if(billCredit == 0): billCredit = 0
        if(billCredit == 8): billCredit = 1 
        if(billCredit == 0x10): billCredit = 2
        if(billCredit == 0x18): billCredit = 3 # $5 dollar bill
        if(billCredit == 0x20): billCredit = 4
        if(billCredit == 0x28): billCredit = 5 # $20 dollar bill
        if(billCredit == 0x30): billCredit = 6
        if(billCredit == 0x38): billCredit = 7


        #------------------ Save progress to file (that is read by php) ---------------
        if(billCredit != 0):
            lastCredit = billCredit
            if(ord(out[3]) & 0x10):
                billCount[billCredit] += 1
                #outFile = open('/var/www/btc/scanner/input/bill_log.txt', 'a')
                #print "Bill credited: Bill#", billCredit
                # Omitting the binary data, so this is commented out: print(to_money_int(billCredit) + '\t' + str(binascii.hexlify(billCount)), file=outFile)
                pipe = db.pipeline()
                if billCredit  == 1:
                    pipe.hincrby(transaction, fiat[0], 1).hincrby(transaction, 'total', fiat[0]).hincrby('transaction:current', 'total', fiat[0]).hincrby('transaction:current', fiat[0], 1).incrby('total', fiat[0]).execute()
                elif billCredit == 2:
                    pipe.hincrby(transaction, fiat[1], 1).hincrby(transaction, 'total', fiat[1]).hincrby('transaction:current', 'total', fiat[1]).hincrby('transaction:current', fiat[1], 1).incrby('total', fiat[1]).execute()
                elif billCredit == 3:
                    pipe.hincrby(transaction, fiat[2], 1).hincrby(transaction, 'total', fiat[2]).hincrby('transaction:current', 'total', fiat[2]).hincrby('transaction:current', fiat[2], 1).incrby('total', fiat[2]).execute()
                elif billCredit == 4:
                    pipe.hincrby(transaction, fiat[3], 1).hincrby(transaction, 'total', fiat[3]).hincrby('transaction:current', 'total', fiat[3]).hincrby('transaction:current', fiat[3], 1).incrby('total', fiat[3]).execute()
                elif billCredit == 5:
                    pipe.hincrby(transaction, fiat[4], 1).hincrby(transaction, 'total', fiat[4]).hincrby('transaction:current', 'total', fiat[4]).hincrby('transaction:current', fiat[4], 1).incrby('total', fiat[4]).execute()
                elif billCredit == 6:
                    pipe.hincrby(transaction, fiat[5], 1).hincrby(transaction, 'total', fiat[5]).hincrby('transaction:current', 'total', fiat[5]).hincrby('transaction:current', fiat[5], 1).incrby('total', fiat[5]).execute()
                elif billCredit == 7:
                    pipe.hincrby(transaction, fiat[6], 1).hincrby(transaction, 'total', fiat[6]).hincrby('transaction:current', 'total', fiat[6]).hincrby('transaction:current', fiat[6], 1).incrby('total', fiat[6]).execute()

#		if billCredit  == 1:
#                    pipe.hincrby(transaction, '$5', 1).hincrby(transaction, 'total', 5).hincrby('transaction:current', 'total', 5).hincrby('transaction:current', '$5', 1).incrby('total', 5).execute()
#                elif billCredit == 2:
#                    pipe.hincrby(transaction, '$10', 1).hincrby(transaction, 'total', 10).hincrby('transaction:current', 'total', 10).hincrby('transaction:current', '$10', 1).incrby('total', 10).execute()
#                elif billCredit == 3:
#                    pipe.hincrby(transaction, '$20', 1).hincrby(transaction, 'total', 20).hincrby('transaction:current', 'total', 20).hincrby('transaction:current', '$20', 1).incrby('total', 20).execute()
#                elif billCredit == 4:
#                    pipe.hincrby(transaction, '$50', 1).hincrby(transaction, 'total', 50).hincrby('transaction:current', 'total', 50).hincrby('transaction:current', '$50', 1).incrby('total', 50).execute()
#                elif billCredit == 5:
#                    pipe.hincrby(transaction, '$100', 1).hincrby(transaction, 'total', 100).hincrby('transaction:current', 'total', 100).hincrby('transaction:current', '$100', 1).incrby('total', 100).execute()
                print(str(billCredit) + '\t' + str(binascii.hexlify(billCount)))
                #outFile.close()
                #print "Acceptor now holds:",binascii.hexlify(billCount)


        time.sleep(0.05)

    #                          0      1      2      3      4      5      6      7
    #                        start   len    ack    off   escrow reserved end    chksum
    stopCommand = bytearray([0x02,  0x08,  0x10,  0x00,  0x10,  0x00,  0x03,  0x00])
    stopCommand[7] = chksum(stopCommand)
    ser.write(stopCommand)
    time.sleep(0.1)
    #print(binascii.hexlify(billCount))
    print ("port closed")
    print ("stop_scan is: ")
    print (stop_scan)
    ser.close()

##############################
#
##############################
# main code start
if __name__ == '__main__':
    try:
        t = Thread(target=scan_money)
        print("starting thread<br>")
        t.start()
        t.join()

    except Exception, errtxt:
        print("Unable to start thread:" + errtxt)

print("stopped")
