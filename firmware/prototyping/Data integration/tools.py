from crc import Calculator, Configuration
import struct
import matplotlib.pyplot as plt
import numpy as np


def ReadBinaryData(file):
    # Read raw data
    raw = b''
    with open(file, 'rb') as file:
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

    return lowAcc, highAcc, gyr, mag, temp, press, gps


def UnitSphereCalibrate(x, y, z):
    nsamples = len(x)

    # Build reading matrix: M * p + e = 1
    M = np.zeros((nsamples, 9))

    M[:, 0] = x**2
    M[:, 1] = y**2
    M[:, 2] = z**2
    M[:, 3] = 2 * x * y
    M[:, 4] = 2 * x * z
    M[:, 5] = 2 * y * z
    M[:, 6] = 2 * x
    M[:, 7] = 2 * y
    M[:, 8] = 2 * z

    ones = np.ones((nsamples, 1))

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

    print("Center offset (V):")
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

    print("\nDistortion matrix (W_inv):")
    print(W_inv)


    # Correct raw values
    x_cent = x - V[0][0]
    y_cent = y - V[1][0]
    z_cent = z - V[2][0]

    x_corr = x_cent * W_inv[0][0] + y_cent * W_inv[0][1] + z_cent * W_inv[0][2]
    y_corr = x_cent * W_inv[1][0] + y_cent * W_inv[1][1] + z_cent * W_inv[1][2]
    z_corr = x_cent * W_inv[2][0] + y_cent * W_inv[2][1] + z_cent * W_inv[2][2]


    # Downsample for plotting
    max_plot_points = 500
    if nsamples > max_plot_points:
        # Randomly select indices without replacement
        idx = np.random.choice(nsamples, max_plot_points, replace=False)

        x_plot, y_plot, z_plot = x[idx], y[idx], z[idx]
        x_corr_plot, y_corr_plot, z_corr_plot = x_corr[idx], y_corr[idx], z_corr[idx]
    else:
        x_plot, y_plot, z_plot = x, y, z
        x_corr_plot, y_corr_plot, z_corr_plot = x_corr, y_corr, z_corr


    fig = plt.figure()
    ax = fig.add_subplot(projection='3d')

    ax.scatter(x_plot, y_plot, z_plot, c="blue", alpha=0.7, label="Raw")
    ax.scatter(x_corr_plot, y_corr_plot, z_corr_plot, c="green", alpha=0.7, label="Calibrated")

    # Plot unit sphere wireframe
    u = np.linspace(0, 2 * np.pi, 40)
    v = np.linspace(0, np.pi, 40)

    x_sphere = np.outer(np.cos(u), np.sin(v))
    y_sphere = np.outer(np.sin(u), np.sin(v))
    z_sphere = np.outer(np.ones(np.size(u)), np.cos(v))

    ax.plot_wireframe(x_sphere, y_sphere, z_sphere, color="red", alpha=0.2, linewidth=0.5)

    # Mark the center
    ax.scatter(0, 0, 0, c="red", s=100, marker='x')

    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_zlabel('Z')

    ax.set_box_aspect([1, 1, 1])
    ax.set_aspect('equal')

    ax.legend(loc='upper right')

    fig.tight_layout()

    plt.show()



    # 2D plots
    fig, axs = plt.subplots(1, 3, figsize=(15, 5))

    # Generate points for a unit circle
    theta = np.linspace(0, 2 * np.pi, 100)
    circle_x = np.cos(theta)
    circle_y = np.sin(theta)

    # Plot formatting helper
    def format_plot(ax, title, xlabel, ylabel):
        ax.set_title(title)
        ax.set_xlabel(xlabel)
        ax.set_ylabel(ylabel)
        ax.grid(True, linestyle='--', alpha=0.6)
        ax.set_aspect('equal', adjustable='datalim')

    # XY Plane
    axs[0].scatter(x, y, c="blue", alpha=0.7, label="Raw")
    axs[0].scatter(x_corr_plot, y_corr_plot, c="green", alpha=0.7, label="Calibrated")
    axs[0].plot(circle_x, circle_y, c="red", linewidth=2, label="Unit Circle")
    format_plot(axs[0], 'XY Plane', 'X', 'Y')
    axs[0].legend()

    # XZ Plane
    axs[1].scatter(x, z, c="blue", alpha=0.7)
    axs[1].scatter(x_corr_plot, z_corr_plot, c="green", alpha=0.7)
    axs[1].plot(circle_x, circle_y, c="red", linewidth=2)
    format_plot(axs[1], 'XZ Plane', 'X', 'Z')

    # YZ Plane
    axs[2].scatter(y, z, c="blue", alpha=0.7)
    axs[2].scatter(y_corr_plot, z_corr_plot, c="green", alpha=0.7)
    axs[2].plot(circle_x, circle_y, c="red", linewidth=2)
    format_plot(axs[2], 'YZ Plane', 'Y', 'Z')

    fig.tight_layout()
    plt.show()

    return x_corr, y_corr, z_corr



def CalDataCent(x, y, z, cent):
    x_cent = x - cent[0]
    y_cent = y - cent[1]
    z_cent = z - cent[2]

    return x_cent, y_cent, z_cent


def CalDataCentDist(x, y, z, cent, dist):
    x_cent = x - cent[0]
    y_cent = y - cent[1]
    z_cent = z - cent[2]

    x_corr = x_cent * dist[0][0] + y_cent * dist[0][1] + z_cent * dist[0][2]
    y_corr = x_cent * dist[1][0] + y_cent * dist[1][1] + z_cent * dist[1][2]
    z_corr = x_cent * dist[2][0] + y_cent * dist[2][1] + z_cent * dist[2][2]

    return x_corr, y_corr, z_corr






