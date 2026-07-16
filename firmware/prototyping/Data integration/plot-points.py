import matplotlib.pyplot as plt
import numpy as np
from crc import Calculator, Configuration
import struct


OUTPUT_FILE = 'flight_dataroll.bin'

# Read raw data
raw = b''
with open(OUTPUT_FILE, 'rb') as file:
    raw = file.read()

# Validate each data chunk and parse contents
lowAcc = []
highAcc = []
gyr = []
mag = []
temp = []
press = []
gps = []


chunks = raw.split(b'\xAA\xBB\xBB\xAA')
for idx, chunk in enumerate(chunks):
    if len(chunk) == 0:
        continue

    readings = chunk[0]
    nbytes = int.from_bytes(chunk[1:3], byteorder='big')
    crc = chunk[3]

    chunk = chunk[4:]

    if nbytes != len(chunk):
        print(f"Chunk number {idx} had incorrect length")
        continue

    crc8_config = Configuration(width=8, polynomial=0x07)
    crc_calc = Calculator(crc8_config)
    checksum = crc_calc.checksum(chunk)

    if crc != checksum:
        print(f"Chunk number {idx} had incorrect checksum")
        continue

    pos = 0
    dtype = 0
    dlen = 0

    while (pos < nbytes):
        header = int.from_bytes(chunk[pos:pos + 2], byteorder='big')
        dtype = header >> 12
        dlen = header & 0x0FFF

        if dtype == 0:   # Type 0 packet (low range acc)
            t = struct.unpack('<f', chunk[pos + 2 : pos + 6])[0]
            x = struct.unpack('<f', chunk[pos + 6 : pos + 10])[0]
            y = struct.unpack('<f', chunk[pos + 10 : pos + 14])[0]
            z = struct.unpack('<f', chunk[pos + 14 : pos + 18])[0]

            lowAcc.append([t, x, y, z])
        elif dtype == 1:   # Type 1 packet (gyroscope)
            t = struct.unpack('<f', chunk[pos + 2 : pos + 6])[0]
            x = struct.unpack('<f', chunk[pos + 6 : pos + 10])[0]
            y = struct.unpack('<f', chunk[pos + 10 : pos + 14])[0]
            z = struct.unpack('<f', chunk[pos + 14 : pos + 18])[0]

            gyr.append([t, x, y, z])
        elif dtype == 2:   # Type 2 packet (high range acc)
            t = struct.unpack('<f', chunk[pos + 2 : pos + 6])[0]
            x = struct.unpack('<f', chunk[pos + 6 : pos + 10])[0]
            y = struct.unpack('<f', chunk[pos + 10 : pos + 14])[0]
            z = struct.unpack('<f', chunk[pos + 14 : pos + 18])[0]

            if t != 0:
                highAcc.append([t, x, y, z])
        elif dtype == 3:   # Type 3 packet (magnetometer)
            t = struct.unpack('<f', chunk[pos + 2 : pos + 6])[0]
            x = struct.unpack('<f', chunk[pos + 6 : pos + 10])[0]
            y = struct.unpack('<f', chunk[pos + 10 : pos + 14])[0]
            z = struct.unpack('<f', chunk[pos + 14 : pos + 18])[0]

            mag.append([t, x, y, z])
        elif dtype == 4:   # Type 4 packet (press/temp)
            t = struct.unpack('<f', chunk[pos + 2 : pos + 6])[0]
            prs = struct.unpack('<f', chunk[pos + 6 : pos + 10])[0]
            tmp = struct.unpack('<f', chunk[pos + 10 : pos + 14])[0]

            press.append([t, prs])
            temp.append([t, tmp])
        elif dtype == 5:   # Type 5 packet (GPS)
            t = struct.unpack('<f', chunk[pos + 2 : pos + 6])[0]
            lat = struct.unpack('<f', chunk[pos + 6 : pos + 10])[0]
            long = struct.unpack('<f', chunk[pos + 10 : pos + 14])[0]
            alt = struct.unpack('<f', chunk[pos + 14 : pos + 18])[0]
            spd = struct.unpack('<f', chunk[pos + 18 : pos + 22])[0]
            sats = chunk[pos + 22]
            fix = chunk[pos + 23]

            if t != 0:
                gps.append([t, lat, long, alt, spd, sats, fix])

        pos += dlen + 2


fig = plt.figure()
ax = fig.add_subplot(projection='3d')

t, x, y, z = zip(*mag)
ax.scatter(x, y, z)

ax.scatter(0, 0, 0, c="red", s=100)

ax.set_xlabel('X')
ax.set_ylabel('Y')
ax.set_zlabel('Z')

fig.tight_layout()

plt.show()







