import serial, time, sys
try:
    s=serial.Serial('COM5',9600,timeout=0.5)
except Exception as e:
    print('OPEN_ERROR',e)
    sys.exit(1)
# Ensure RTS = False (IO0 high) before reset
try:
    s.rts = False
    time.sleep(0.05)
    # Pulse DTR low then high to reset (EN)
    s.dtr = False
    time.sleep(0.05)
    s.dtr = True
    time.sleep(0.05)
except Exception as e:
    print('CONTROL_LINES_ERROR', e)

end=time.time()+12
print('Capturing (boot attempt)...')
with open('serial_log_boot.txt','w',encoding='utf8') as f:
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
