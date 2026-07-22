import matplotlib.pyplot as plt
import numpy as np
import tools
import math


DATA_FILE = 'flight_datasr.bin'

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

tha, xha, yha, zha = zip(*highAcc)
xha, yha, zha = tools.CalDataCentDist(np.array(xha[100:-100]), np.array(yha)[100:-100], np.array(zha[100:-100]), hacc_cal_cent, hacc_cal_dist)
tha = tha[100:-100]

tm, xm, ym, zm = zip(*mag)
xm, ym, zm = tools.CalDataCentDist(np.array(xm), np.array(ym), np.array(zm), mag_cal_cent, mag_cal_dist)

# Extract all data
tp, prs = zip(*press)
tp, prs = np.array(tp), np.array(prs)

# tt, tmp = zip(*temp)
# tt, tmp = np.array(tt), np.array(tmp)

tgps, lat, long, alt, velNorth, velEast, velDown, groundSpd, heading, horzAcc, vertAcc, spdAcc, sats, fix = zip(*gps)
tgps, lat, long, alt, groundSpd, sats, fix = np.array(tgps), np.array(lat), np.array(long), np.array(alt), np.array(groundSpd), np.array(sats), np.array(fix)


# --- 3D Orientation MEKF variables ---
# Define starting orientation
q = np.array([1.0, 0.0, 0.0, 0.0])

# Initialize covariance matrix with high uncertainty
P_ori = np.eye(3) * 1.0

# Gyroscope noise
Q_ori = np.eye(3) * 0.01

# Accelerometer noise
R_acc = np.eye(3) * 0.1

# Magnetometer noise
R_mag = np.eye(3) * 0.5


# --- 1D Vertical kalman filter variables ---
# State vector (Altitude (meters), Vertical velocity (m/s))
X_alt = np.array([0.0, 0.0])

# Uncertainty matrix
P_alt = np.eye(2) * 1.0

# Barometer noise
R_baro = np.array([[2.0]]) # 2 meters squared variance



# --- 2D Horizontal kalman filter variables ---
# State vector (Pos north (m), Pos east (m), Vel north (m/s), Vel east (m/s)]
X_horiz = np.array([0.0, 0.0, 0.0, 0.0])

# Uncertainty matrix
P_horiz = np.eye(4) * 1.0

# GPS Noise
R_gps = np.eye(2) * 4.0

# Initialise home as the first GPS coordinate
lat_home = lat[0]
lon_home = long[0]


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


def quat_to_matrix(q):
    qw, qx, qy, qz = q

    # Pre-calculate squares
    qx2, qy2, qz2 = qx*qx, qy*qy, qz*qz

    # Build the 3x3 rotation matrix
    R = np.array([
        [1 - 2*qy2 - 2*qz2,     2*qx*qy - 2*qz*qw,     2*qx*qz + 2*qy*qw],
        [    2*qx*qy + 2*qz*qw, 1 - 2*qx2 - 2*qz2,     2*qy*qz - 2*qx*qw],
        [    2*qx*qz - 2*qy*qw,     2*qy*qz + 2*qx*qw, 1 - 2*qx2 - 2*qy2]
    ])

    return R


def skew(v):
    return np.array([
        [    0, -v[2],  v[1]],
        [ v[2],     0, -v[0]],
        [-v[1],  v[0],     0]
    ])


def GyrUpdate(vang, q, dt):
    # Calculate rotation done in timestep
    dang = np.radians(vang) * dt

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
    q_upd = q_upd / np.linalg.norm(q_upd)

    # Build the Skew-Symmetric matrix of the delta angles
    wx, wy, wz = dang
    skew_dang = np.array([[  0, -wz,  wy],
                          [ wz,   0, -wx],
                          [-wy,  wx,   0]])

    # Build the state transition matrix
    F = np.eye(3) - skew_dang

    # Grow the uncertainty bubble
    P_upd = F @ P_ori @ F.T + (Q_ori * dt)

    return q_upd, P_upd


def SensorUpdate(actual_vec, expected_vec, q, P, R):
    # Normalize the vectors
    actual_vec = actual_vec / np.linalg.norm(actual_vec)
    expected_vec = expected_vec / np.linalg.norm(expected_vec)

    # Calculate the error
    error = actual_vec - expected_vec

    H = skew(expected_vec)

    # Calculate covariance
    S = H @ P @ H.T + R

    # Calculate the kalman gain
    K = P @ H.T @ np.linalg.inv(S)

    # Calculate and apply the orientation correction
    delta_theta = K @ error
    theta = np.linalg.norm(delta_theta)

    if theta < 1e-8:
        dq = np.array([1.0, 0.0, 0.0, 0.0])
    else:
        half_theta = theta / 2.0
        sin_half_theta = math.sin(half_theta)
        dq = np.array([
            math.cos(half_theta),
            (delta_theta[0] / theta) * sin_half_theta,
            (delta_theta[1] / theta) * sin_half_theta,
            (delta_theta[2] / theta) * sin_half_theta
        ])

    q_new = quat_mult(q, dq)
    q_new = q_new / np.linalg.norm(q_new)

    # Shrink uncertainty
    I = np.eye(3)
    P_new = (I - K @ H) @ P

    return q_new, P_new


def get_expected_gravity(q):
    qw, qx, qy, qz = q

    g_expected = np.array([
        2*qx*qz - 2*qy*qw,
        2*qy*qz + 2*qx*qw,
        1 - 2*qx*qx - 2*qy*qy
    ])
    return g_expected


def get_expected_magnetic(q, m_actual):
    R = quat_to_matrix(q)

    # Rotate the actual measurement to the navigation frame
    h = R @ m_actual

    # Build the idealized reference vector (all horizontal strength along X)
    bx = math.sqrt(h[0]*h[0] + h[1]*h[1])
    bz = h[2]
    b_nav = np.array([bx, 0.0, bz])

    # Rotate back to the body frame
    m_expected = R.T @ b_nav

    return m_expected


def get_linear_acceleration(q, a_composite):
    # Rotate the raw body acceleration into the Earth frame
    R = quat_to_matrix(q)
    a_nav = R @ a_composite

    # Remove gravity
    a_lin = a_nav - np.array([0.0, 0.0, 1.0])

    return a_lin * 9.80665    # Convert to m/s^2


def pressure_to_altitude(pressure, baseline_pressure):
    return 44330.0 * (1.0 - math.pow(pressure / baseline_pressure, 0.1903))


def latlon_to_meters(lat, lon, lat_home, lon_home):
    R = 6371000.0  # Radius of the Earth in meters

    phi1 = math.radians(lat_home)
    phi2 = math.radians(lat)
    dphi = math.radians(lat - lat_home)
    dlambda = math.radians(lon - lon_home)

    # Equirectangular approximation
    x = R * dlambda * math.cos((phi1 + phi2) / 2.0) # Meters east (Y in the navigation frame)
    y = R * dphi # Meters north (X in the navigation frame)

    return np.array([y, x])


# Arrays for storing the updated orientation estimate over time
t_q_est = []
q_est = []
q_raw_est = []

# Simulate moving through time and update q when data is available
# Start at time 0 and keep going until the last recording time of the sensors in use
tstart = 0
tend = tg[-1]    #max(tg[-1], tla[-1], tm[-1])
tcurr = tstart

posg, posla, posm = 0, 0, 0     # Keep track of the earliest unused measurement in each sensor for efficiency

t_prev_g = tg[0]   # For calculating delta time between gyroscope measurements

# Initialise memory for swapping between accelerometer readings
latest_low_g = np.array([0.0, 0.0, 0.0])
latest_high_g = np.array([0.0, 0.0, 0.0])

t_prev_accel = min([tla[0], tha[0]])    # For calculating delta time between accelerometer measurements
posha = 0

# Use the first pressure reading as the reference 0 altitude pressure
ground_pressure = prs[0]
posp = 0

# For plotting altitude and vertical velocity estimate
t_vert_est = []
vvel_est = []
alt_est = []


posgps = 0


# For plotting horizontal movement estimates
t_horz_est = []
horz_nv_est = []
horz_ev_est = []
horz_nd_est = []
horz_ed_est = []


while tcurr < tend:
    mintime = 9999999999
    if posg < len(tg) and tg[posg] < mintime:
        mintime = tg[posg]
    if posla < len(tla) and tla[posla] < mintime:
        mintime = tla[posla]
    if posm < len(tm) and tm[posm] < mintime:
        mintime = tm[posm]
    if posha < len(tha) and tha[posha] < mintime:
        mintime = tha[posha]
    if posp < len(tp) and tp[posp] < mintime:
        mintime = tp[posp]
    if posgps < len(tgps) and tgps[posgps] < mintime:
        mintime = tgps[posgps]

    tcurr = mintime

    if tcurr == tg[posg]:   # Gyroscope update
        dt = tg[posg] - t_prev_g
        t_prev_g = tg[posg]

        # Only update if time has actually passed
        if dt > 0:
            q, P_ori = GyrUpdate(np.array([xg[posg], yg[posg], zg[posg]]), q, dt)

        t_q_est.append(tcurr)
        q_est.append(quat_to_euler(q))
        q_raw_est.append(q)
        posg += 1
    elif (posla < len(tla) and tcurr == tla[posla]) or (posha < len(tha) and tcurr == tha[posha]):      # Low or high G accelerometer update
        # Determine which accelerometer updated
        low_g_updated = False
        high_g_updated = False

        if tcurr == tla[posla]:
            latest_low_g = np.array([xla[posla], yla[posla], zla[posla]])
            low_g_updated = True
            posla += 1
        else:
            latest_high_g = np.array([xha[posha], yha[posha], zha[posha]])
            high_g_updated = True
            posha += 1

        # Determine current mode based on low G saturation
        sat_threshold = 7.0  # Just below the 8G limit
        is_saturated = (abs(latest_low_g[0]) > sat_threshold or abs(latest_low_g[1]) > sat_threshold or abs(latest_low_g[2]) > sat_threshold)

        # Only run the filter if there is fresh valid data
        run_filter = False
        if is_saturated and high_g_updated:
            active_accel = latest_high_g
            run_filter = True
        elif not is_saturated and low_g_updated:
            active_accel = latest_low_g
            run_filter = True

        if run_filter:
            # Calculate distance from 1G magnitude to dynamically adjust weight lower when accelerating
            accel_magnitude = np.linalg.norm(active_accel)
            g_error = abs(accel_magnitude - 1.0)
            dynamic_variance = 0.1 + (1000.0 * (g_error ** 2))
            R_acc_dynamic = np.eye(3) * dynamic_variance

            # Update the attitude MEKF
            a_expected = get_expected_gravity(q)
            q, P_ori = SensorUpdate(active_accel, a_expected, q, P_ori, R_acc_dynamic)

            # Extract true linear acceleration for translation
            a_lin_ms2 = get_linear_acceleration(q, active_accel)

            # Run translation prediction
            dt_accel = tcurr - t_prev_accel
            if dt_accel > 0:
                # Flip Z so positive is up
                a_z_up = -a_lin_ms2[2]

                # State transition matrix
                F = np.array([[1.0, dt_accel],
                              [0.0, 1.0]])

                # Control input matrix
                B = np.array([0.5 * dt_accel**2, dt_accel])

                # Predict new state
                X_alt = F @ X_alt + (B * a_z_up)

                # Dynamically calculate vertical kinematic process noise
                sigma_alt = 2.0  # Vertical acceleration variance
                qa_p = sigma_alt * (dt_accel**3) / 3.0
                qa_c = sigma_alt * (dt_accel**2) / 2.0
                qa_v = sigma_alt * dt_accel

                Q_alt_dyn = np.array([
                    [qa_p, qa_c],
                    [qa_c, qa_v]
                ])

                # Grow uncertainty
                P_alt = F @ P_alt @ F.T + Q_alt_dyn


                # Horizontal prediction
                # Extract horizontal accelerations
                a_horiz = np.array([a_lin_ms2[0], a_lin_ms2[1]])

                # State transition matrix
                F_h = np.array([
                    [1.0, 0.0, dt_accel, 0.0],
                    [0.0, 1.0, 0.0, dt_accel],
                    [0.0, 0.0, 1.0, 0.0],
                    [0.0, 0.0, 0.0, 1.0]
                ])

                # Control input matrix
                B_h = np.array([
                    [0.5 * dt_accel**2, 0.0],
                    [0.0, 0.5 * dt_accel**2],
                    [dt_accel, 0.0],
                    [0.0, dt_accel]
                ])

                # Predict new state and grow uncertainty
                X_horiz = F_h @ X_horiz + (B_h @ a_horiz)

                # Dynamically calculate horizontal kinematic process noise
                sigma_horiz = 5.0  # High variance to absorb IMU errors
                qh_p = sigma_horiz * (dt_accel**3) / 3.0
                qh_c = sigma_horiz * (dt_accel**2) / 2.0
                qh_v = sigma_horiz * dt_accel

                Q_horiz_dyn = np.array([
                    [qh_p, 0.0,  qh_c, 0.0],
                    [0.0,  qh_p, 0.0,  qh_c],
                    [qh_c, 0.0,  qh_v, 0.0],
                    [0.0,  qh_c, 0.0,  qh_v]
                ])

                # Grow uncertainty
                P_horiz = F_h @ P_horiz @ F_h.T + Q_horiz_dyn

                t_horz_est.append(tcurr)
                horz_nv_est.append(X_horiz[2])
                horz_ev_est.append(X_horiz[3])
                horz_nd_est.append(X_horiz[0])
                horz_ed_est.append(X_horiz[1])

            t_prev_accel = tcurr
    elif tcurr == tp[posp]:   # Barometer update
        # Convert pressure to altitude
        measured_alt = pressure_to_altitude(prs[posp], ground_pressure)

        # Measurement matrix
        H = np.array([[1.0, 0.0]])

        # Calculate error and kalman gain
        y = measured_alt - (H @ X_alt)
        S = H @ P_alt @ H.T + R_baro
        K = P_alt @ H.T @ np.linalg.inv(S)

        # Correct the state
        X_alt = X_alt + (K @ y).flatten()

        # Shrink the uncertainty
        I = np.eye(2)
        P_alt = (I - K @ H) @ P_alt

        t_vert_est.append(tcurr)
        alt_est.append(X_alt[0])
        vvel_est.append(X_alt[1])

        posp += 1
    elif tcurr == tgps[posgps]:   # GPS update
        if fix[posgps] > 0:   # Only trust the GPS if it has a satellite fix
            # Convert spherical Lat/Lon to flat cartesian meters
            measured_pos = latlon_to_meters(lat[posgps], long[posgps], lat_home, lon_home)

            # Measurement matrix
            H_h = np.array([
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0]
            ])

            # Calculate error and kalman gain
            y_h = measured_pos - (H_h @ X_horiz)
            S_h = H_h @ P_horiz @ H_h.T + R_gps
            K_h = P_horiz @ H_h.T @ np.linalg.inv(S_h)

            # Correct the state and shrink uncertainty
            X_horiz = X_horiz + (K_h @ y_h).flatten()
            I_h = np.eye(4)
            P_horiz = (I_h - K_h @ H_h) @ P_horiz

        posgps += 1
    else:      # Magnetometer update
        m_actual = np.array([xm[posm], ym[posm], zm[posm]])

        # Calculate what magnetic north should look like based on guessed rotation
        m_expected = get_expected_magnetic(q, m_actual)

        q, P_ori = SensorUpdate(m_actual, m_expected, q, P_ori, R_mag)

        posm += 1


q_est = np.stack(q_est, axis=0)



# Plot all data
fig, axs = plt.subplots(2, 2, figsize=(16, 10))
fig.suptitle("Avionics Sensor Fusion Dashboard", fontsize=16, fontweight='bold')

# 1. Orientation (Top Left)
# axs[0, 0].plot(t_q_est, q_est[:, 0], label='X (Roll)', color='tab:blue')
# axs[0, 0].plot(t_q_est, q_est[:, 1], label='Y (Pitch)', color='tab:orange')
# axs[0, 0].plot(t_q_est, q_est[:, 2], label='Z (Yaw)', color='tab:green')
# axs[0, 0].set_title("Orientation Estimate")
# axs[0, 0].set_ylabel("Angle (Degrees)")
# axs[0, 0].set_xlabel("Time (Seconds)")
# axs[0, 0].legend(loc='upper right')
# axs[0, 0].grid(True, linestyle='--', alpha=0.6)


axs[0, 0].plot(t_horz_est, horz_nv_est, label='North', color='tab:blue')
axs[0, 0].plot(t_horz_est, horz_ev_est, label='East', color='tab:orange')
axs[0, 0].set_title("Horzontal velocity")
axs[0, 0].set_ylabel("Speed")
axs[0, 0].set_xlabel("Time (Seconds)")
axs[0, 0].legend(loc='upper right')
axs[0, 0].grid(True, linestyle='--', alpha=0.6)

# 2. Altitude (Top Right)
axs[0, 1].plot(t_vert_est, alt_est, color='tab:purple', linewidth=2)
axs[0, 1].set_title("Altitude Estimate (AGL)")
axs[0, 1].set_ylabel("Altitude (Meters)")
axs[0, 1].set_xlabel("Time (Seconds)")
axs[0, 1].grid(True, linestyle='--', alpha=0.6)

# 3. Vertical Velocity (Bottom Left)
axs[1, 0].plot(t_vert_est, vvel_est, color='tab:red', linewidth=2)
axs[1, 0].set_title("Vertical Velocity Estimate")
axs[1, 0].set_ylabel("Velocity (m/s)")
axs[1, 0].set_xlabel("Time (Seconds)")
axs[1, 0].grid(True, linestyle='--', alpha=0.6)

# 4. Horizontal Track (Bottom Right)
axs[1, 1].plot(horz_ed_est, horz_nd_est, color='tab:brown', linewidth=2)
axs[1, 1].set_title("Horizontal Track Estimate")
axs[1, 1].set_ylabel("North/South Displacement (m)")
axs[1, 1].set_xlabel("East/West Displacement (m)")
axs[1, 1].axis('equal')
axs[1, 1].grid(True, linestyle='--', alpha=0.6)

plt.tight_layout()
plt.subplots_adjust(top=0.92)
plt.show()






# Path and orientation animation
import matplotlib.animation as animation

# 1. Interpolate position data to match the high-speed gyroscope timestamps
pos_n = np.interp(t_q_est, t_horz_est, horz_nd_est)
pos_e = np.interp(t_q_est, t_horz_est, horz_ed_est)
pos_u = np.interp(t_q_est, t_vert_est, alt_est)

# Convert quaternions to numpy array
q_raw_est = np.stack(q_raw_est, axis=0)

fig = plt.figure(figsize=(10, 10))
ax = fig.add_subplot(111, projection='3d')

# Set axes labels to standard ENU (East, North, Up)
ax.set_xlabel('East (X) [meters]')
ax.set_ylabel('North (Y) [meters]')
ax.set_zlabel('Altitude (Z) [meters]')
ax.set_title("3D Flight Path & Orientation")

# 2. Dynamically scale the 3D axes so the flight path fits perfectly
# We find the largest distance traveled in any one direction to lock the aspect ratio
max_range = np.array([pos_e.max()-pos_e.min(), pos_n.max()-pos_n.min(), pos_u.max()-pos_u.min()]).max() / 2.0
if max_range < 1.0:
    max_range = 1.0 # Prevent zero-division if the board hasn't moved at all

mid_x = (pos_e.max() + pos_e.min()) * 0.5
mid_y = (pos_n.max() + pos_n.min()) * 0.5
mid_z = (pos_u.max() + pos_u.min()) * 0.5

ax.set_xlim(mid_x - max_range, mid_x + max_range)
ax.set_ylim(mid_y - max_range, mid_y + max_range)
ax.set_zlim(mid_z - max_range, mid_z + max_range)

# 3. Initialize the visual elements
# The flight trail
trail, = ax.plot([], [], [], color='gray', alpha=0.5, linestyle='--', label='Flight Path')

# The axis indicators (Scaled to 10% of the flight path so they are always visible)
axis_len = max_range * 0.1 
line_x, = ax.plot([], [], [], color='r', linewidth=4, label='X Axis (Roll)')
line_y, = ax.plot([], [], [], color='g', linewidth=4, label='Y Axis (Pitch)')
line_z, = ax.plot([], [], [], color='b', linewidth=4, label='Z Axis (Yaw)')
ax.legend()

def update(num, q_raw, p_e, p_n, p_u, line_x, line_y, line_z, trail):
    # Get current position for this frame
    cx, cy, cz = p_e[num], p_n[num], p_u[num]
    
    # Get the Body-to-Navigation rotation matrix for the current quaternion
    R = quat_to_matrix(q_raw[num])
    
    # Map the Math (North, East, Down) to the Plot (East, North, Up)
    # R[0,:] is North, R[1,:] is East, R[2,:] is Down
    # We multiply by axis_len so the lines scale to the size of the graph
    
    # X Axis (Roll)
    x_vec = np.array([R[1,0], R[0,0], -R[2,0]]) * axis_len
    line_x.set_data_3d([cx, cx + x_vec[0]], [cy, cy + x_vec[1]], [cz, cz + x_vec[2]])
    
    # Y Axis (Pitch)
    y_vec = np.array([R[1,1], R[0,1], -R[2,1]]) * axis_len
    line_y.set_data_3d([cx, cx + y_vec[0]], [cy, cy + y_vec[1]], [cz, cz + y_vec[2]])
    
    # Z Axis (Yaw)
    z_vec = np.array([R[1,2], R[0,2], -R[2,2]]) * axis_len
    line_z.set_data_3d([cx, cx + z_vec[0]], [cy, cy + z_vec[1]], [cz, cz + z_vec[2]])
    
    # Update the flight path trail (from start up to current frame)
    trail.set_data_3d(p_e[:num], p_n[:num], p_u[:num])
    
    return line_x, line_y, line_z, trail

# Step by 5 frames for playback speed
ani = animation.FuncAnimation(fig, update, frames=range(0, len(q_raw_est), 5),
                              fargs=(q_raw_est, pos_e, pos_n, pos_u, line_x, line_y, line_z, trail),
                              interval=20, blit=False)

plt.show()






