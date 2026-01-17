import sys
import time
try:
    import serial
except Exception as e:
    print('pyserial no está instalado. Instálalo con: pip install pyserial')
    sys.exit(2)

port = sys.argv[1] if len(sys.argv) > 1 else 'COM6'
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 9600
wait = int(sys.argv[3]) if len(sys.argv) > 3 else 8
print(f'Listening on {port} @ {baud} for {wait}s...')
try:
    ser = serial.Serial(port, baud, timeout=1)
except Exception as e:
    print('Error opening port:', e)
    sys.exit(3)

end = time.time() + wait
try:
    while time.time() < end:
        line = ser.readline()
        if line:
            try:
                print(line.decode('utf-8', errors='replace').rstrip('\r\n'))
            except:
                print(line)
finally:
    ser.close()
    print('Done listening.')
