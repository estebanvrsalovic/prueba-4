import serial, time, sys, argparse

# Usage: python read_serial.py [PORT] [BAUD] [DURATION] [--reset]
parser = argparse.ArgumentParser(description='Read serial port for a short time')
parser.add_argument('port', nargs='?', default='COM6', help='Serial port (default COM6)')
parser.add_argument('baud', nargs='?', type=int, default=115200, help='Baud rate (default 115200)')
parser.add_argument('duration', nargs='?', type=float, default=5.0, help='Read duration seconds (default 5)')
parser.add_argument('--reset', action='store_true', help='Toggle DTR to reset device before reading')
args = parser.parse_args()

port = args.port
baud = args.baud
duration = args.duration
timeout = 0.5

try:
    s = serial.Serial(port, baud, timeout=timeout)
except Exception as e:
    print('ERROR opening serial port:', e)
    sys.exit(1)

if args.reset:
    try:
        # Pulse DTR: set False->True to reset many microcontrollers
        s.setDTR(False)
        time.sleep(0.1)
        s.setDTR(True)
        # give device a bit to reboot
        time.sleep(0.2)
    except Exception as e:
        print('Warning: DTR toggle failed:', e)

end = time.time() + duration
print(f'Reading {duration}s from {port} at {baud}...')
try:
    while time.time() < end:
        data = s.read(1024)
        if data:
            try:
                sys.stdout.write(data.decode('utf-8', errors='replace'))
                sys.stdout.flush()
            except Exception as e:
                print('\n<decode error> ', e)

finally:
    s.close()

print('\nDone.')
