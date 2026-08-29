import matplotlib.pyplot as plt
import numpy as np
import tools
import math


DATA_FILE = 'flight_datamr.bin'

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
lowAcc, highAcc, gyr, mag, temp, press, gps = tools.ReadBinaryDataNew(DATA_FILE)

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

tgps, lat, long, alt, velNorth, velEast, velDown, groundSpd, heading, horzAcc, vertAcc, spdAcc, sats, fix = zip(*gps)
tgps, lat, long, alt, velNorth, velEast, velDown, groundSpd, heading, horzAcc, vertAcc, spdAcc, sats, fix =  np.array(tgps), np.array(lat), np.array(long), np.array(alt), np.array(velNorth), np.array(velEast), \
        np.array(velDown), np.array(groundSpd), np.array(heading), np.array(horzAcc), np.array(vertAcc), np.array(spdAcc), np.array(sats), np.array(fix)


# =====================================================================
# --- 3D Orientation MEKF variables (6-state: attitude error + gyro bias) ---
# =====================================================================
# Define starting orientation
q = np.array([1.0, 0.0, 0.0, 0.0])

# Gyro bias estimate (deg/s, body frame) - starts at zero since gyr_cal already
# removes the static turn-on offset; this state now tracks whatever residual/
# drifting bias is left, which your own stationary-window data shows is real
# (~0.1 deg/s on one axis in the sample flight log).
b_gyr = np.array([0.0, 0.0, 0.0])

# Augmented covariance: [attitude_error(3), gyro_bias(3)]
P_ori = np.zeros((6, 6))
P_ori[0:3, 0:3] = np.eye(3) * 1.0          # initial attitude uncertainty
P_ori[3:6, 3:6] = np.eye(3) * 0.25         # initial bias uncertainty ((deg/s)^2), generous given measured residual

# Gyroscope white noise (attitude process noise)
Q_theta = np.eye(3) * 0.01

# Gyro bias random-walk noise - how fast the bias itself is allowed to wander.
# Keep this small: too large and the bias state just chases gyro noise instead
# of tracking a slowly-varying instrument bias.
Q_bias = np.eye(3) * 1e-6

# Accelerometer noise
R_acc = np.eye(3) * 0.1

# Magnetometer noise
R_mag = np.eye(3) * 0.5

# Magnetic declination
MAG_DEC = math.radians(1.08)


# =====================================================================
# --- Combined 3D translational Kalman filter (9-state) ---
# Replaces the separate 1D altitude + 2D horizontal filters. Position and
# velocity are estimated jointly in all 3 axes (N, E, Up), plus a 3-state
# accelerometer bias IN BODY FRAME (bx, by, bz). Body frame is the physically
# correct place for this bias to live since it belongs to the sensor, not the
# world - it must be rotated into the nav frame using the current attitude
# estimate at every step, which is why this can't stay split across two
# decoupled filters the way position/velocity alone could.
# =====================================================================
# State: [posN, posE, alt, velN, velE, vvel, bx, by, bz]
X_trans = np.zeros(9)

P_trans = np.eye(9) * 1.0
P_trans[6:9, 6:9] = np.eye(3) * 0.05   # initial accel bias uncertainty (g^2)

# Bias random-walk noise (g^2 per second) - same philosophy as Q_bias above:
# small enough that the bias tracks slow drift, not sample noise.
Q_accel_bias = np.eye(3) * 1e-7

# GPS noise multipliers
NM_hgps_pos = 5.0
NM_hgps_vel = 20.0
NM_vgps_pos = 15.0
NM_vgps_vel = 30.0

# Barometer static noise
R_baro = np.array([[2.0]])

# Base process noise floor and dynamic kinematic process noise scale
base_accel_noise = 0.01
sigma_trans = 5.0   # m^2/s^3-ish process noise density, same role as your old sigma_horiz/sigma_alt

GRAVITY = 9.80665

# Use the first pressure reading as the reference 0 altitude pressure
ground_pressure = prs[0]
ground_alt = alt[0]

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

    qx2, qy2, qz2 = qx*qx, qy*qy, qz*qz

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


def GyrUpdate(vang, q, b_gyr, P, dt):
    # Correct the measured rate with the current bias estimate BEFORE
    # integrating. This is the whole point: any residual bias directly
    # reduces the rotation applied to q each step, instead of silently
    # accumulating into attitude error every single sample.
    vang_corrected = vang - b_gyr

    dang = np.radians(vang_corrected) * dt
    theta = np.linalg.norm(dang)

    if theta < 1e-8:
        dq = np.array([1.0, 0.0, 0.0, 0.0])
    else:
        half_theta = theta / 2.0
        sin_half_theta = math.sin(half_theta)

        qw = math.cos(half_theta)
        qx = (dang[0] / theta) * sin_half_theta
        qy = (dang[1] / theta) * sin_half_theta
        qz = (dang[2] / theta) * sin_half_theta

        dq = np.array([qw, qx, qy, qz])

    q_upd = quat_mult(q, dq)
    q_upd = q_upd / np.linalg.norm(q_upd)

    wx, wy, wz = dang
    skew_dang = np.array([[  0, -wz,  wy],
                          [ wz,   0, -wx],
                          [-wy,  wx,   0]])

    # Augmented 6x6 state transition matrix.
    # Top-left: existing attitude-error propagation.
    # Top-right: how a gyro bias error feeds into attitude error growth -
    #            this off-diagonal block is what makes the bias observable
    #            at all once corrections start flowing back through P.
    # Bottom-right: bias is treated as (near) constant between corrections.
    F = np.eye(6)
    F[0:3, 0:3] = np.eye(3) - skew_dang
    F[0:3, 3:6] = -dt * np.eye(3)

    Q = np.zeros((6, 6))
    Q[0:3, 0:3] = Q_theta * dt
    Q[3:6, 3:6] = Q_bias * dt

    P_upd = F @ P @ F.T + Q

    return q_upd, P_upd


def SensorUpdate(actual_vec, expected_vec, q, b_gyr, P, R):
    actual_vec = actual_vec / np.linalg.norm(actual_vec)
    expected_vec = expected_vec / np.linalg.norm(expected_vec)

    error = actual_vec - expected_vec

    # H only observes attitude directly (a vector measurement doesn't
    # depend on gyro bias) - the bias columns are zero. The bias still
    # gets corrected because P has built up attitude/bias cross-covariance
    # during propagation; K's bottom rows come out nonzero automatically.
    H = np.zeros((3, 6))
    H[:, 0:3] = skew(expected_vec)

    S = H @ P @ H.T + R
    K = P @ H.T @ np.linalg.inv(S)

    correction = K @ error
    delta_theta = correction[0:3]
    delta_b = correction[3:6]

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

    b_gyr_new = b_gyr + delta_b

    I6 = np.eye(6)
    IKH = I6 - K @ H
    P_new = IKH @ P @ IKH.T + K @ R @ K.T

    return q_new, b_gyr_new, P_new


def get_expected_gravity(q):
    qw, qx, qy, qz = q

    g_expected = -np.array([
        2*qx*qz - 2*qy*qw,
        2*qy*qz + 2*qx*qw,
        1 - 2*qx*qx - 2*qy*qy
    ])
    return g_expected


def get_expected_magnetic(q, m_actual):
    R = quat_to_matrix(q)
    h = R @ m_actual

    bx = math.sqrt(h[0]*h[0] + h[1]*h[1])
    bz = h[2]

    b_nav = np.array([bx * math.cos(MAG_DEC), bx * math.sin(MAG_DEC), bz])

    m_expected = R.T @ b_nav

    return m_expected


def get_linear_acceleration(q, a_composite):
    # Unchanged from your original - takes an already bias-corrected body
    # accel reading and returns gravity-removed linear acceleration in m/s^2.
    R = quat_to_matrix(q)
    a_nav = R @ a_composite
    a_lin = a_nav - np.array([0.0, 0.0, -1.0])
    return a_lin * GRAVITY


def get_bias_jacobian(q):
    # d(a_nav_used)/d(bias_body), where a_nav_used = [aN, aE, aUp].
    # a_lin_ms2 = (R @ (a_meas - bias) - [0,0,-1]) * g0
    # => d(a_lin_ms2)/d(bias) = -R * g0
    # then aUp = -a_lin_ms2[2], aN/aE = a_lin_ms2[0:2], so flip the sign of
    # the third row to match.
    R = quat_to_matrix(q)
    M = -R * GRAVITY
    M[2, :] *= -1.0
    return M


def pressure_to_altitude(pressure, baseline_pressure):
    return 44330.0 * (1.0 - math.pow(pressure / baseline_pressure, 0.1903)) + ground_alt


def latlon_to_meters(lat, lon, lat_home, lon_home):
    R = 6371000.0

    phi1 = math.radians(lat_home)
    phi2 = math.radians(lat)
    dphi = math.radians(lat - lat_home)
    dlambda = math.radians(lon - lon_home)

    x = R * dlambda * math.cos((phi1 + phi2) / 2.0)
    y = R * dphi

    return np.array([y, x])


# Arrays for plotting estimated values later
t_q_est = []
q_est = []
q_raw_est = []
bgyr_est = []

t_trans_est = []
pos_est = []
vel_est = []
baccel_est = []

# Diagnostics plotting
tposerror = []
poserrorx = []
poserrory = []

tadyn = []
adyn = []


# Simulate moving through time and update filters when data is available
tcurr = 0
tend = tg[-1]

posg, posla, posm, posp, posha, posgps = 0, 0, 0, 0, 0, 0

t_prev_g = tg[0]
t_prev_accel = min([tla[0], tha[0]])

latest_low_g = np.array([0.0, 0.0, 0.0])
latest_high_g = np.array([0.0, 0.0, 0.0])


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

        if dt > 0:
            q, P_ori = GyrUpdate(np.array([xg[posg], yg[posg], zg[posg]]), q, b_gyr, P_ori, dt)

        t_q_est.append(tcurr)
        q_est.append(quat_to_euler(q))
        q_raw_est.append(q)
        bgyr_est.append(b_gyr.copy())

        posg += 1
    elif (posla < len(tla) and tcurr == tla[posla]) or (posha < len(tha) and tcurr == tha[posha]):
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

        sat_threshold = 7.0
        is_saturated = (abs(latest_low_g[0]) > sat_threshold or abs(latest_low_g[1]) > sat_threshold or abs(latest_low_g[2]) > sat_threshold)

        run_filter = False
        if is_saturated and high_g_updated:
            active_accel = latest_high_g
            run_filter = True
        elif not is_saturated and low_g_updated:
            active_accel = latest_low_g
            run_filter = True

        if run_filter:
            accel_magnitude = np.linalg.norm(active_accel)
            g_error = abs(accel_magnitude - 1.0)
            dynamic_variance = 0.1 + (1000.0 * (g_error ** 2))
            R_acc_dynamic = np.eye(3) * dynamic_variance

            tadyn.append(tcurr)
            adyn.append(dynamic_variance)

            # Update the attitude+gyro-bias MEKF
            a_expected = get_expected_gravity(q)
            q, b_gyr, P_ori = SensorUpdate(active_accel, a_expected, q, b_gyr, P_ori, R_acc_dynamic)

            # --- Combined 3D translational prediction, with accel bias ---
            dt_accel = tcurr - t_prev_accel
            if dt_accel > 0:
                bias_body = X_trans[6:9]
                a_corrected_body = active_accel - bias_body
                a_lin_ms2 = get_linear_acceleration(q, a_corrected_body)
                a_vec = np.array([a_lin_ms2[0], a_lin_ms2[1], -a_lin_ms2[2]])  # [aN, aE, aUp]

                # Same footstrike/vibration protection idea discussed
                # earlier: when the raw specific force is far from 1g, that
                # sample is unreliable for translation too, not just
                # attitude - inflate this step's process noise instead of
                # trusting the double-integration blindly.
                accel_trust_penalty = 1.0 + (200.0 * (g_error ** 2))

                F = np.eye(9)
                B = np.zeros((9, 3))
                M = get_bias_jacobian(q)

                F[0:3, 3:6] = dt_accel * np.eye(3)          # pos += vel*dt
                B[0:3, :] = 0.5 * dt_accel**2 * np.eye(3)   # pos += 0.5*a*dt^2
                B[3:6, :] = dt_accel * np.eye(3)            # vel += a*dt
                F[0:6, 6:9] = B[0:6, :] @ M                 # bias error coupling into pos/vel

                X_trans = F @ X_trans + B @ a_vec
                X_trans[6:9] = bias_body  # bias itself doesn't change in prediction

                qk_p = sigma_trans * accel_trust_penalty * (dt_accel**3) / 3.0 + base_accel_noise * dt_accel
                qk_c = sigma_trans * accel_trust_penalty * (dt_accel**2) / 2.0
                qk_v = sigma_trans * accel_trust_penalty * dt_accel

                Q_trans = np.zeros((9, 9))
                for i in range(3):
                    Q_trans[i, i] = qk_p
                    Q_trans[i, i+3] = qk_c
                    Q_trans[i+3, i] = qk_c
                    Q_trans[i+3, i+3] = qk_v
                Q_trans[6:9, 6:9] = Q_accel_bias * dt_accel

                P_trans = F @ P_trans @ F.T + Q_trans

                t_trans_est.append(tcurr)
                pos_est.append(X_trans[0:3].copy())
                vel_est.append(X_trans[3:6].copy())
                baccel_est.append(X_trans[6:9].copy())

            t_prev_accel = tcurr
    elif tcurr == tp[posp]:   # Barometer update
        measured_alt = pressure_to_altitude(prs[posp], ground_pressure)

        H = np.zeros((1, 9))
        H[0, 2] = 1.0

        y = np.array([measured_alt]) - (H @ X_trans)
        S = H @ P_trans @ H.T + R_baro
        K = P_trans @ H.T @ np.linalg.inv(S)

        X_trans = X_trans + (K @ y).flatten()

        I9 = np.eye(9)
        IKH = I9 - K @ H
        P_trans = IKH @ P_trans @ IKH.T + K @ R_baro @ K.T

        posp += 1
    elif tcurr == tgps[posgps]:   # GPS update
        if fix[posgps] == 0:
            posgps += 1
            continue

        measured_pos = latlon_to_meters(lat[posgps], long[posgps], lat_home, lon_home)

        H = np.zeros((6, 9))
        H[0, 0] = 1.0   # posN
        H[1, 1] = 1.0   # posE
        H[2, 2] = 1.0   # alt
        H[3, 3] = 1.0   # velN
        H[4, 4] = 1.0   # velE
        H[5, 5] = 1.0   # vvel

        vsAcc = spdAcc[posgps] * vertAcc[posgps] / horzAcc[posgps]

        R_gps = np.diag([
            horzAcc[posgps]**2 * NM_hgps_pos,
            horzAcc[posgps]**2 * NM_hgps_pos,
            vertAcc[posgps]**2 * NM_vgps_pos,
            spdAcc[posgps]**2 * NM_hgps_vel,
            spdAcc[posgps]**2 * NM_hgps_vel,
            vsAcc**2 * NM_vgps_vel
        ])

        measurement = np.array([
            measured_pos[0], measured_pos[1], alt[posgps],
            velNorth[posgps], velEast[posgps], -velDown[posgps]
        ])

        y = measurement - (H @ X_trans)
        S = H @ P_trans @ H.T + R_gps
        K = P_trans @ H.T @ np.linalg.inv(S)

        tposerror.append(tcurr)
        poserrorx.append(measured_pos[0] - X_trans[0])
        poserrory.append(measured_pos[1] - X_trans[1])

        X_trans = X_trans + (K @ y).flatten()

        I9 = np.eye(9)
        IKH = I9 - K @ H
        P_trans = IKH @ P_trans @ IKH.T + K @ R_gps @ K.T

        posgps += 1
    else:      # Magnetometer update
        m_actual = np.array([xm[posm], ym[posm], zm[posm]])

        m_expected = get_expected_magnetic(q, m_actual)

        q, b_gyr, P_ori = SensorUpdate(m_actual, m_expected, q, b_gyr, P_ori, R_mag)

        posm += 1


q_est = np.stack(q_est, axis=0)
bgyr_est = np.stack(bgyr_est, axis=0)
pos_est = np.stack(pos_est, axis=0)
vel_est = np.stack(vel_est, axis=0)
baccel_est = np.stack(baccel_est, axis=0)


# Plot all data
fig, axs = plt.subplots(2, 2, figsize=(16, 10))
fig.suptitle("Avionics Sensor Fusion Dashboard (bias-augmented)", fontsize=16, fontweight='bold')

axs[0, 0].plot(t_q_est, q_est[:, 0], label='X (Roll)', color='tab:blue')
axs[0, 0].plot(t_q_est, q_est[:, 1], label='Y (Pitch)', color='tab:orange')
axs[0, 0].plot(t_q_est, q_est[:, 2], label='Z (Yaw)', color='tab:green')
axs[0, 0].set_title("Orientation Estimate")
axs[0, 0].set_ylabel("Angle (Degrees)")
axs[0, 0].set_xlabel("Time (Seconds)")
axs[0, 0].legend(loc='upper right')
axs[0, 0].grid(True, linestyle='--', alpha=0.6)

axs[0, 1].plot(t_trans_est, pos_est[:, 2], color='tab:purple', linewidth=2)
axs[0, 1].set_title("Altitude Estimate (AGL)")
axs[0, 1].set_ylabel("Altitude (Meters)")
axs[0, 1].set_xlabel("Time (Seconds)")
axs[0, 1].grid(True, linestyle='--', alpha=0.6)

axs[1, 0].plot(t_trans_est, vel_est[:, 2], color='tab:red', linewidth=2)
axs[1, 0].set_title("Vertical Velocity Estimate")
axs[1, 0].set_ylabel("Velocity (m/s)")
axs[1, 0].set_xlabel("Time (Seconds)")
axs[1, 0].grid(True, linestyle='--', alpha=0.6)

axs[1, 1].plot(pos_est[:, 1], pos_est[:, 0], color='tab:brown', linewidth=2)
axs[1, 1].set_title("Horizontal Track Estimate")
axs[1, 1].set_ylabel("North/South Displacement (m)")
axs[1, 1].set_xlabel("East/West Displacement (m)")
axs[1, 1].axis('equal')
axs[1, 1].grid(True, linestyle='--', alpha=0.6)

plt.tight_layout()
plt.subplots_adjust(top=0.92)
plt.savefig('trans_dashboard.png', dpi=120)


# New: bias diagnostic plot - sanity check these converge and stay small/stable
fig2, axs2 = plt.subplots(1, 2, figsize=(14, 5))
fig2.suptitle("Estimated sensor bias (should converge, not wander unboundedly)", fontsize=14, fontweight='bold')

axs2[0].plot(t_q_est, bgyr_est[:, 0], label='X', color='tab:blue')
axs2[0].plot(t_q_est, bgyr_est[:, 1], label='Y', color='tab:orange')
axs2[0].plot(t_q_est, bgyr_est[:, 2], label='Z', color='tab:green')
axs2[0].set_title("Estimated gyro bias")
axs2[0].set_ylabel("deg/s")
axs2[0].set_xlabel("Time (s)")
axs2[0].legend()
axs2[0].grid(True, linestyle='--', alpha=0.6)

axs2[1].plot(t_trans_est, baccel_est[:, 0], label='X', color='tab:blue')
axs2[1].plot(t_trans_est, baccel_est[:, 1], label='Y', color='tab:orange')
axs2[1].plot(t_trans_est, baccel_est[:, 2], label='Z', color='tab:green')
axs2[1].set_title("Estimated accelerometer bias (body frame)")
axs2[1].set_ylabel("g")
axs2[1].set_xlabel("Time (s)")
axs2[1].legend()
axs2[1].grid(True, linestyle='--', alpha=0.6)

plt.tight_layout()
plt.savefig('bias_estimates.png', dpi=120)

print("Done. Wrote trans_dashboard.png and bias_estimates.png")
print("Final gyro bias estimate (deg/s):", b_gyr)
print("Final accel bias estimate (g):", X_trans[6:9])