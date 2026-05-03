import serial

SERIAL_PORT = 'COM7'
BAUD_RATE = 1000000

received = 0

try:
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
        print(f"Listening on {SERIAL_PORT}...")

        buffer = b''
        while True:
            if ser.in_waiting == 0:
                continue

            buffer += ser.read(ser.in_waiting)

            while b'\r\n' in buffer:
                packet, buffer = buffer.split(b'\r\n', 1)

                if len(packet) > 0:
                    try:
                        text_part = packet[:-1].decode('ascii')
                        counter = packet[-1]
                        received += 1
                        print(f"{received} Received: {text_part} | Count: {counter}")

                    except UnicodeDecodeError:
                        print(f"Raw data: {packet}")

except KeyboardInterrupt:
    print("\nStopped")
except Exception as e:
    print(f"Error: {e}")