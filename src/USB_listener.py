import serial;
import io;
import time;
import os;

if __name__ == '__main__' :
    try :
        # configure the serial connections (the parameters differs on the device you are connecting to)
        with serial.Serial(port=r"/dev/cu.usbmodem3458387035361", baudrate=12000000, timeout=1,
                       xonxoff=False, rtscts=False, dsrdtr=True) as ser:
            for line in ser:
                print(line)

    except :
        print('Program exit !')
        pass
    finally :
        ser.close()
    pass
