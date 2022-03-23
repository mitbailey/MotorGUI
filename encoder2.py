import serial #pyserial package for COM port connections
import subprocess #package for calling command line
import time
import sys
import os
import numpy as np
import datetime as dt
import signal

def write_get_serial(string, device):
    #writes one string, carriage returns it and reads the resulting line
    device.write(str.encode(string+"\r\n")) #need the \r\n to get full carriage return. stackoverflow#676172
    line=''
    time.sleep(0.01)
    while device.inWaiting() > 0: #are items in buffer waiting to be read
        line += device.readline() #append line to variable
        print(line)
    
    return line

encoder2=serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
encoder2.write(str.encode("$0V\r\n"))#test serial number
bei_device_id = encoder2.readline()

# print('Opened connection to USB-SSI interface on Port:\n'+encoder2.portstr+'. Serial number: '+bei_device_id)
print('Opened connection to USB-SSI interface on Port:')
print(encoder2.portstr)

print('Serial Number:')
print(bei_device_id.decode('utf-8'))

encoder2.write(str.encode("$0L1250\r\n"))#set encoder1 to 25bit readout w/ parity
encoder2.write(str.encode("$0L2250\r\n"))#set encoder2 to 25bit readout w/ parity
encoder2.write(str.encode("$0L1250\r\n"))#set encoder1 to 25bit readout w/ parity
time.sleep(0.1)

print(encoder2.readline().decode('utf-8'))

ofile = open('encoder_%u.txt'%(dt.datetime.now().timestamp()), 'w')

start_time=time.time()
encoder2.write(str.encode("$0A00010\r\n"))#read both encoders, constant streaming at a rate in milliseconds
while True:
    try:
        ts1 = dt.datetime.now().timestamp()
        init_val = ''
        while '*0R0' != init_val[-4:]:
            # print(init_val.replace('\r', ' '))
            init_val += encoder2.read(size=1).decode('utf-8')
        # print('something')
        data = ''
        # data header found!
        c = encoder2.read(size=1).decode('utf-8')
        while c != '\r':
            data += c
            c = encoder2.read(size=1).decode('utf-8')
            # print(data)
        print(data)
        ts2 = dt.datetime.now().timestamp()
        oline = str(ts1) + ',' + str(ts2) + ','
        oline += data
        ofile.write(oline + '\n')
    except KeyboardInterrupt:
        ofile.close()
        encoder2.close()


