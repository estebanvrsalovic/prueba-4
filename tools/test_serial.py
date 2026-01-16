import sys
import time
try:
    import serial
except Exception as e:
    print('pyserial no está instalado. Instálalo con: pip install pyserial')
    sys.exit(2)

port = sys.argv[1] if len(sys.argv) > 1 else 'COM6'
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 9600
print(f'Testing serial port {port} @ {baud}...')
try:
    ser = serial.Serial(port, baud, timeout=1)
except Exception as e:
    print('Error opening port:', e)
    sys.exit(3)

# flush
ser.reset_input_buffer()
ser.reset_output_buffer()

def send_and_read(cmd, wait=1.0):
    print('>>> SEND:', cmd)
    ser.write((cmd + '\n').encode('utf-8'))
    ser.flush()
    deadline = time.time() + wait
    out = []
    while time.time() < deadline:
        line = ser.readline()
        if line:
            try:
                out.append(line.decode('utf-8', errors='replace').rstrip('\r\n'))
            except:
                out.append(str(line))
    return out

# Give device a moment
time.sleep(0.5)

resp = send_and_read('help', wait=2.0)
print('<<< RESPONSE to help:')
for r in resp:
    print(r)

resp = send_and_read('status', wait=2.0)
print('<<< RESPONSE to status:')
for r in resp:
    print(r)

ser.close()
print('Done.')
