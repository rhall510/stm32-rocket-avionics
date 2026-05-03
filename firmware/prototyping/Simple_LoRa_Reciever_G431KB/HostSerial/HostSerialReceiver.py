import serial

SERIAL_PORT = 'COM7'
BAUD_RATE = 1000000

try:
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
        print(f"Listening on {SERIAL_PORT}...")

        while True:
            if ser.in_waiting == 0:
                continue

            print(ser.read(ser.in_waiting))

except KeyboardInterrupt:
    print("\nStopped")
except Exception as e:
    print(f"Error: {e}")