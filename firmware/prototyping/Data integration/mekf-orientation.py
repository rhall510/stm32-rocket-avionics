import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import tools
import math


DATA_FILE = 'flight_datastill.bin'

# Calibration values
gyr_cal = np.array([0.068633, -0.866754, 0.150951])

mag_cal_cent = np.array([0.05886926, 0.44385249, 0.02833682])
mag_cal_dist = np.array([[2.10899989, 0.02800124, 0.04438672],
                        [0.02800124, 2.02111567, -0.00440351],
                        [0.04438672, -0.00440351, 2.34470254]])

lacc_cal_cent = np.array([-0.00398834, -0.01495143, 0.00977724])
lacc_cal_dist = np.array([[1.00047372, 0.000176672972, -0.00303506789],
                        [0.000176672972, 0.996552141, 0.000214283291],
                        [-0.00303506789, 0.000214283291, 0.994217168]])

hacc_cal_cent = np.array([0.60846088, 1.28277898, 0.77979761])
hacc_cal_dist = np.array([[0.971669703, 0.0375371840, 0.000657705583],
                        [0.0375371840, 1.03017628, 0.0208469271],
                        [0.000657705583, 0.0208469271, 0.980978031]])


# Validate each data chunk and parse contents
lowAcc, highAcc, gyr, mag, temp, press, gps = tools.ReadBinaryData(DATA_FILE)

# Calibrate gyroscope, accelerometers, and magnetometer
tg, xg, yg, zg = zip(*gyr)
xg, yg, zg = tools.CalDataCent(np.array(xg[100:-100]), np.array(yg[100:-100]), np.array(zg[100:-100]), gyr_cal)
tg = tg[100:-100]

tla, xla, yla, zla = zip(*lowAcc)
xla, yla, zla = tools.CalDataCentDist(np.array(xla[100:-100]), np.array(yla[100:-100]), np.array(zla[100:-100]), lacc_cal_cent, lacc_cal_dist)
tla = tla[100:-100]

# tha, xha, yha, zha = zip(*highAcc)
# xha, yha, zha = tools.CalDataCentDist(np.array(xha), np.array(yha), np.array(zha), hacc_cal_cent, hacc_cal_dist)

tm, xm, ym, zm = zip(*mag)
xm, ym, zm = tools.CalDataCentDist(np.array(xm), np.array(ym), np.array(zm), mag_cal_cent, mag_cal_dist)

# Extract all data
# tp, prs = zip(*press)
# tp, prs = np.array(tp), np.array(prs)
# tt, tmp = zip(*temp)
# tt, tmp = np.array(tt), np.array(tmp)
# tgps, lat, long, alt, spd, sats, fix = zip(*gps)
# tgps, lat, long, alt, spd, sats, fix = np.array(tgps), np.array(lat), np.array(long), np.array(alt), np.array(spd), np.array(sats), np.array(fix)




# Define starting orientation
q = np.array([1.0, 0.0, 0.0, 0.0])

def quat_mult(q1, q2):
    w1, x1, y1, z1 = q1
    w2, x2, y2, z2 = q2

    w = w1*w2 - x1*x2 - y1*y2 - z1*z2
    x = w1*x2 + x1*w2 + y1*z2 - z1*y2
    y = w1*y2 - x1*z2 + y1*w2 + z1*x2
    z = w1*z2 + x1*y2 - y1*x2 + z1*w2

    return np.array([w, x, y, z])


def quat_to_euler(q):
    w, x, y, z = q
    ysqr = y * y

    t0 = +2.0 * (w * x + y * z)
    t1 = +1.0 - 2.0 * (x * x + ysqr)
    X = math.degrees(math.atan2(t0, t1))

    t2 = +2.0 * (w * y - z * x)
    t2 = +1.0 if t2 > +1.0 else t2
    t2 = -1.0 if t2 < -1.0 else t2
    Y = math.degrees(math.asin(t2))

    t3 = +2.0 * (w * z + x * y)
    t4 = +1.0 - 2.0 * (ysqr + z * z)
    Z = math.degrees(math.atan2(t3, t4))

    return np.array([X, Y, Z])


def GyrUpdate(vang, q, dt):
    # Calculate rotation done in timestep
    dang = vang * dt

    theta = np.linalg.norm(dang)

    if theta < 1e-8:  # Skip if rotation is very small
        dq = np.array([1.0, 0.0, 0.0, 0.0])
    else:
        # Calculate the rotation quaternion using an exponential map
        half_theta = theta / 2.0
        sin_half_theta = math.sin(half_theta)

        qw = math.cos(half_theta)
        qx = (dang[0] / theta) * sin_half_theta
        qy = (dang[1] / theta) * sin_half_theta
        qz = (dang[2] / theta) * sin_half_theta

        dq = np.array([qw, qx, qy, qz])

    # Multiply and normalize
    q_upd = quat_mult(q, dq)
    return q_upd / np.linalg.norm(q_upd)


# Arrays for storing the updated orientation estimate over time
t_q_est = []
q_est = []

# Simulate moving through time and update q when data is available
# Start at time 0 and keep going until the last recording time of the sensors in use
tstart = 0
tend = tg[-1]    #max(tg[-1], tla[-1], tm[-1])
tcurr = tstart

posg, posla, posm = 0, 0, 0     # Keep track of the earliest unused measurement in each sensor for efficiency

t_prev_g = tg[0]   # For calculating delta time between gyroscope measurements

while tcurr < tend:
    mintime = 9999999999
    if posg < len(tg) and tg[posg] < mintime:
        mintime = tg[posg]
    if posla < len(tla) and tla[posla] < mintime:
        mintime = tla[posla]
    if posm < len(tm) and tm[posm] < mintime:
        mintime = tm[posm]

    tcurr = mintime

    if tcurr == tg[posg]:   # Gyroscope update
        dt = tg[posg] - t_prev_g
        t_prev_g = tg[posg]

        # Only update if time has actually passed
        if dt > 0:
            q = GyrUpdate(np.array([xg[posg], yg[posg], zg[posg]]), q, dt)

        t_q_est.append(tcurr)
        q_est.append(quat_to_euler(q))
        posg += 1
    elif tcurr == tla[posla]:     # Low G accelerometer update
        posla += 1
    else:      # Magnetometer update
        posm += 1


q_est = np.stack(q_est, axis=0)


# Plot results
fig, axs = plt.subplots(1, 2, figsize=(16, 6))

axs[0].plot(tg, xg, label='X')
axs[0].plot(tg, yg, label='Y')
axs[0].plot(tg, zg, label='Z')
axs[0].set_title("Gyroscope")
axs[0].set_ylabel("Rotation")
axs[0].legend(loc='upper right')
axs[0].set_xlabel("Time (Seconds)")

axs[1].plot(t_q_est, q_est[:, 0], label='X')
axs[1].plot(t_q_est, q_est[:, 1], label='Y')
axs[1].plot(t_q_est, q_est[:, 2], label='Z')
axs[1].set_title("Orientation estimate")
axs[1].set_ylabel("Angle")
axs[1].legend(loc='upper right')
axs[1].set_xlabel("Time (Seconds)")

plt.tight_layout()
plt.show()

