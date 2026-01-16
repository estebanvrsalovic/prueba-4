import serial, time, sys
try:
    s=serial.Serial('COM5',9600,timeout=0.5)
except Exception as e:
    print('OPEN_ERROR',e)
    sys.exit(1)
# Toggle DTR/RTS to reset device (some boards use these signals)
try:
    s.dtr = False
    s.rts = True
    time.sleep(0.05)
    s.dtr = True
    s.rts = False
except Exception as e:
    print('CONTROL_LINES_ERROR', e)

end=time.time()+12
print('Capturing...')
with open('serial_log_reset.txt','w',encoding='utf8') as f:
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
