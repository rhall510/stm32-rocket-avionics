from crc import Calculator, Configuration
import struct
import matplotlib.pyplot as plt

OUTPUT_FILE = 'dat.bin'

# Read raw data and split by packet sync word
raw = b''
with open(OUTPUT_FILE, 'rb') as file:
    raw = file.read().split(b'\x88\x44\x22\x11')

# Concatenate packet data into raw byte stream
concat = b''
counter = 0
for pkt in raw:
    if len(pkt) == 0:
        continue

    # Check sequence number for entirely missing packets
    packnum = int.from_bytes(pkt[0:4], byteorder='big')

    while packnum != counter:
        print(f"Packet number {counter} not received")
        counter += 1

    # Check packet integrity
    # Find and strip terminator
    if pkt[-6:] != b'\x33\x66\x99\xCC\r\n':
        print(f"Packet number {counter} had incorrect terminator")
        counter += 1
        continue

    pkt = pkt[4:-6]
    concat += pkt

    counter += 1


# Validate each data chunk and parse contents
lowAcc = []
highAcc = []
gyr = []
mag = []
temp = []
press = []
gps = []


chunks = concat.split(b'\xAA\xBB\xBB\xAA')
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

    # print(f"{readings} :: {nbytes} {len(chunk)} :: {crc} {checksum}")

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
            sats = chunk[22]
            fix = chunk[23]

            gps.append([t, lat, long, alt, spd, sats, fix])

        pos += dlen + 2


print("Parsing done. Generating plots...")

fig, axs = plt.subplots(4, 2, figsize=(12, 12), sharex=True)
fig.suptitle("Avionics Telemetry Data Over Time", fontsize=16)

# Low-G Accelerometer
if lowAcc:
    t, x, y, z = zip(*lowAcc)
    axs[0, 0].plot(t[100:], x[100:], label='X')
    axs[0, 0].plot(t[100:], y[100:], label='Y')
    axs[0, 0].plot(t[100:], z[100:], label='Z')
    axs[0, 0].set_title("Low-G Accelerometer")
    axs[0, 0].set_ylabel("Accel")
    axs[0, 0].legend(loc='upper right')

# High-G Accelerometer
if highAcc:
    t, x, y, z = zip(*highAcc)
    axs[1, 0].plot(t, x, label='X')
    axs[1, 0].plot(t, y, label='Y')
    axs[1, 0].plot(t, z, label='Z')
    axs[1, 0].set_title("High-G Accelerometer")
    axs[1, 0].set_ylabel("Accel")
    axs[1, 0].legend(loc='upper right')

# Gyroscope
if gyr:
    t, x, y, z = zip(*gyr)
    axs[0, 1].plot(t[100:], x[100:], label='X')
    axs[0, 1].plot(t[100:], y[100:], label='Y')
    axs[0, 1].plot(t[100:], z[100:], label='Z')
    axs[0, 1].set_title("Gyroscope")
    axs[0, 1].set_ylabel("Rate (deg/s)")
    axs[0, 1].legend(loc='upper right')

# Magnetometer
if mag:
    t, x, y, z = zip(*mag)
    axs[1, 1].plot(t, x, label='X')
    axs[1, 1].plot(t, y, label='Y')
    axs[1, 1].plot(t, z, label='Z')
    axs[1, 1].set_title("Magnetometer")
    axs[1, 1].set_ylabel("Mag Field")
    axs[1, 1].legend(loc='upper right')

# Pressure
if press:
    t, prs = zip(*press)
    axs[2, 0].plot(t, prs, color='orange')
    axs[2, 0].set_title("Barometric Pressure")
    axs[2, 0].set_ylabel("Pressure")

# Temperature
if temp:
    t, tmp = zip(*temp)
    axs[2, 1].plot(t, tmp, color='red')
    axs[2, 1].set_title("Temperature")
    axs[2, 1].set_ylabel("Temp (C)")

# GPS
if gps:
    t, lat, long, alt, spd, sats, fix = zip(*gps)

    axs[3, 0].plot(t, lat, label='Latitude', color='blue')
    axs[3, 0].set_ylabel("Latitude", color='blue')
    axs[3, 0].tick_params(axis='y', labelcolor='blue')

    ax_spd = axs[3, 0].twinx()
    ax_spd.plot(t, long, label='Longitude', color='green')
    ax_spd.set_ylabel("Longitude", color='green')
    ax_spd.tick_params(axis='y', labelcolor='green')

    axs[3, 0].set_title("GPS LatLong")

    axs[3, 1].plot(t, alt, label='Altitude', color='blue')
    axs[3, 1].set_ylabel("Altitude", color='blue')
    axs[3, 1].tick_params(axis='y', labelcolor='blue')

    # Twin axis for Speed
    ax_spd = axs[3, 1].twinx()
    ax_spd.plot(t, spd, label='Speed', color='green')
    ax_spd.set_ylabel("Speed", color='green')
    ax_spd.tick_params(axis='y', labelcolor='green')

    axs[3, 1].set_title("GPS Altitude & Speed")


axs[-1, 0].set_xlabel("Time (Seconds)")
axs[-1, 1].set_xlabel("Time (Seconds)")
plt.tight_layout()
plt.show()


print(len(lowAcc), len(highAcc), len(gyr), len(mag), len(temp), len(press), len(gps))

