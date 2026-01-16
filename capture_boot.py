#!/usr/bin/env python3
import time, sys
from serial.tools import list_ports
import serial

PORT = 'COM6'
BAUD = 9600
WAIT_SECS = 30
CAPTURE_SECS = 60
OUTFILE = 'bootlog.txt'

def wait_for_port(name, timeout):
    end = time.time() + timeout
    while time.time() < end:
        ports = [p.device for p in list_ports.comports()]
        if name in ports:
            return True
        time.sleep(0.2)
    return False

print(f"Esperando hasta {WAIT_SECS}s a que aparezca {PORT}...")
if not wait_for_port(PORT, WAIT_SECS):
    print(f"{PORT} no apareció en {WAIT_SECS}s. Conecta/reinicia la placa y vuelve a intentarlo.", file=sys.stderr)
    sys.exit(2)

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
except Exception as e:
    print('Error abriendo puerto:', e, file=sys.stderr)
    sys.exit(3)

print(f"Conectado a {PORT} a {BAUD} bps — grabando {CAPTURE_SECS}s a {OUTFILE}...")
with open(OUTFILE, 'wb') as f:
    start = time.time()
    while time.time() - start < CAPTURE_SECS:
        try:
            b = ser.read(1024)
        except Exception:
            break
        if b:
            f.write(b)
            f.flush()
        else:
            time.sleep(0.02)
ser.close()
print(f"Grabación finalizada — {OUTFILE} creado ({CAPTURE_SECS}s)")
sys.exit(0)
