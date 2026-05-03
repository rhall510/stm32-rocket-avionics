import serial

SERIAL_PORT = 'COM7'
BAUD_RATE = 1000000
OUTPUT_FILE = 'dat.bin'

try:
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
        print(f"Listening on {SERIAL_PORT}...")

        buffer = b''
        with open(OUTPUT_FILE, 'ab') as file:
            while True:
                if ser.in_waiting == 0:
                    continue

                data = ser.read(ser.in_waiting)
                buffer += data

                file.write(data)
                file.flush()

except KeyboardInterrupt:
    print("\nStopped")
except Exception as e:
    print(f"Error: {e}")



