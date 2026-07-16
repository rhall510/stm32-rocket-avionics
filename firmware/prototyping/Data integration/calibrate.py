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


t, x, y, z = zip(*mag)


## Calibrate magnetometer
x = np.array(x)
y = np.array(y)
z = np.array(z)

nmag = len(t)

# Build reading matrix: M * p + e = 1
M = np.zeros((nmag, 9))

M[:, 0] = x**2
M[:, 1] = y**2
M[:, 2] = z**2
M[:, 3] = 2 * x * y
M[:, 4] = 2 * x * z
M[:, 5] = 2 * y * z
M[:, 6] = 2 * x
M[:, 7] = 2 * y
M[:, 8] = 2 * z

ones = np.ones((nmag, 1))

# Solve by multiplying by M transpose: p = (M^T * M)^-1 * M^T * 1
M_T = M.T
p = np.linalg.inv(M_T @ M) @ M_T @ ones

# Extract the 9 ellipsoid coefficients
A, B, C, D, E, F, G, H, I = p.flatten()

# Construct the shape matrix Q and position vector U
Q = np.array([[A, D, E],
              [D, B, F],
              [E, F, C]])

U = np.array([[G], [H], [I]])

V = -np.linalg.inv(Q) @ U

print("Hard iron offset (V):")
print(V)


# Normalize Q so it represents a perfect sphere
k = 1.0 - (U.T @ V)[0][0]
Q = Q / k

# Eigenvalue decomposition on Q
evals, evecs = np.linalg.eigh(Q)

# Create a diagonal matrix of the square root of the eigenvalues
sqrt_evals = np.diag(np.sqrt(evals))

# Calculate soft iron correction matrix W^-1
W_inv = evecs @ sqrt_evals @ evecs.T

print("\nSoft iron matrix (W_inv):")
print(W_inv)





# Correct raw values
x_cent = x - V[0][0]
y_cent = y - V[1][0]
z_cent = z - V[2][0]

x_corr = x_cent * W_inv[0][0] + y_cent * W_inv[0][1] + z_cent * W_inv[0][2]
y_corr = x_cent * W_inv[1][0] + y_cent * W_inv[1][1] + z_cent * W_inv[1][2]
z_corr = x_cent * W_inv[2][0] + y_cent * W_inv[2][1] + z_cent * W_inv[2][2]


fig = plt.figure()
ax = fig.add_subplot(projection='3d')

ax.scatter(x, y, z, c="blue")
ax.scatter(x_corr, y_corr, z_corr, c="green")

ax.scatter(0, 0, 0, c="red", s=100)

ax.set_xlabel('X')
ax.set_ylabel('Y')
ax.set_zlabel('Z')

ax.set_box_aspect([1, 1, 1])

fig.tight_layout()

plt.show()




