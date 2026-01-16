import serial, time, sys
try:
    s=serial.Serial('COM5',9600,timeout=1)
except Exception as e:
    print('OPEN_ERROR',e)
    sys.exit(1)
end=time.time()+20
with open('serial_log.txt','w',encoding='utf8') as f:
    while time.time()<end:
        try:
            l=s.readline().decode('utf8',errors='ignore').rstrip()
            if l:
                print(l)
                f.write(l+'\n')
        except Exception as e:
            print('READ_ERROR',e)
            break
s.close()
print('Capture complete')
