import serial
import time
from datetime import datetime


# Config
SERIALPORT = input("Serial port of receiver:\n > ")
BAUDRATE = 115200

starttime = int(time.time())
rawfile = f"rangetest_RAW_{starttime}.csv"
aggfile = f"rangetest_SUMMARY_{starttime}.txt"


try:
    ser = serial.Serial(SERIALPORT, BAUDRATE, timeout=1)
    print(f"\n--- Connected to {SERIALPORT} ---")
    print(f"Raw packet stats saving to: {rawfile}")
    print(f"Aggregated stats saving to: {aggfile}")
    print("\nListening...\n")

    with open(rawfile, 'w', newline='') as rawf, open(aggfile, 'w', newline='') as aggf:
        rawf.write("PC_Timestamp,Sequence,RSSI,SNR,Status\n")
        
        while True:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if not line:
                        continue

                    now = datetime.now()
                    timestamp = now.strftime("%H:%M:%S.%f")[:-3]

                    # Raw packet data
                    if line.startswith("PKT:"):
                        data = line[4:]
                        rawf.write(f"{timestamp},{data}\n")
                        rawf.flush()

                    # Aggregated stats and other
                    else:
                        print(f"[{timestamp}] {line}")

                        aggf.write(f"[{timestamp}] {line}\n")
                        aggf.flush()

                except Exception as e:
                    print(f"Error reading serial line: {e}")

except serial.SerialException:
    print(f"Error: Could not open {SERIALPORT}")
    input("")
except KeyboardInterrupt:
    print("\n\nLogging finished.")
    print(f"Saved: {rawfile}")
    print(f"Saved: {aggfile}")
    input("")