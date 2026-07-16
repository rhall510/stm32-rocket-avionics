import matplotlib.pyplot as plt
import numpy as np
from crc import Calculator, Configuration
import struct
from matplotlib.animation import FuncAnimation


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

x = np.array(x)
y = np.array(y)
z = np.array(z)
n_total = len(t)

# --- 1. Pre-calculate a Unit Sphere Mesh ---
u = np.linspace(0, 2 * np.pi, 20)
v = np.linspace(0, np.pi, 15)
x_sphere = np.outer(np.cos(u), np.sin(v))
y_sphere = np.outer(np.sin(u), np.sin(v))
z_sphere = np.outer(np.ones_like(u), np.cos(v))

# Flatten the sphere coordinates into a 3xN matrix for easy multiplication
sphere_points = np.vstack((x_sphere.flatten(), y_sphere.flatten(), z_sphere.flatten()))


# --- 2. Set up the Figure ---
fig = plt.figure(figsize=(8, 8))
ax = fig.add_subplot(projection='3d')

raw_scatter = ax.scatter([], [], [], c="blue", alpha=0.5, label="Raw Data")

max_range = np.max(np.abs([x, y, z])) * 1.2
ax.set_xlim([-max_range, max_range])
ax.set_ylim([-max_range, max_range])
ax.set_zlim([-max_range, max_range])
ax.set_box_aspect([1, 1, 1])
ax.set_xlabel('X')
ax.set_ylabel('Y')
ax.set_zlabel('Z')

# We use a dictionary to hold the state of the surface so we can remove it each frame
state = {'surface': None}


# --- 3. The Animation Loop ---
def update(frame):
    curr_x = x[:frame]
    curr_y = y[:frame]
    curr_z = z[:frame]
    
    raw_scatter._offsets3d = (curr_x, curr_y, curr_z)

    if frame > 30:
        n_points = len(curr_x)
        M = np.zeros((n_points, 9))
        M[:, 0] = curr_x**2
        M[:, 1] = curr_y**2
        M[:, 2] = curr_z**2
        M[:, 3] = 2 * curr_x * curr_y
        M[:, 4] = 2 * curr_x * curr_z
        M[:, 5] = 2 * curr_y * curr_z
        M[:, 6] = 2 * curr_x
        M[:, 7] = 2 * curr_y
        M[:, 8] = 2 * curr_z
        
        ones = np.ones(n_points)
        
        try:
            p, residuals, rank, s = np.linalg.lstsq(M, ones, rcond=None)
            A, B, C, D, E, F, G, H, I = p.flatten()
            
            Q = np.array([[A, D, E],
                          [D, B, F],
                          [E, F, C]])
            U = np.array([[G], [H], [I]])
            
            V = -np.linalg.inv(Q) @ U
            k = 1.0 - (U.T @ V)[0][0]
            Q = Q / k
            
            evals, evecs = np.linalg.eigh(Q)
            
            if np.all(evals > 0):
                # Calculate the inverse transformation (mapping sphere OUT to ellipsoid)
                # Note the 1.0 / np.sqrt(evals) here instead of just np.sqrt
                W_ellipsoid = evecs @ np.diag(1.0 / np.sqrt(evals)) @ evecs.T
                
                # Transform the unit sphere points
                ellipsoid_points = W_ellipsoid @ sphere_points + V
                
                # Reshape back into 2D grids for wireframe plotting
                x_ell = ellipsoid_points[0, :].reshape(x_sphere.shape)
                y_ell = ellipsoid_points[1, :].reshape(y_sphere.shape)
                z_ell = ellipsoid_points[2, :].reshape(z_sphere.shape)
                
                # Remove the old wireframe
                if state['surface'] is not None:
                    state['surface'].remove()
                
                # Draw the new wireframe
                state['surface'] = ax.plot_wireframe(x_ell, y_ell, z_ell, color='orange', alpha=0.4)
                ax.set_title(f"Points: {frame} / {n_total} - Fitting Ellipsoid")
                
            else:
                ax.set_title(f"Points: {frame} / {n_total} - Gathering 3D coverage...")
                
        except np.linalg.LinAlgError:
            pass 

    return raw_scatter,

step_size = 1 #max(1, n_total // 100) 
frames = np.arange(10, n_total, step_size)

ani = FuncAnimation(fig, update, frames=frames, interval=20, blit=False, repeat=False)

plt.show()