import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import tools


CALIBRATION_FILE = 'flight_datarollstable.bin'

lowAcc = []
highAcc = []
gyr = []
mag = []
temp = []
press = []
gps = []

lowAcc, highAcc, gyr, mag, temp, press, gps = tools.ReadBinaryData(CALIBRATION_FILE)


# Extract data into separate numpy arrays
tl, xl, yl, zl = zip(*lowAcc)
tl = np.array(tl)
xl = np.array(xl)
yl = np.array(yl)
zl = np.array(zl)

th, xh, yh, zh = zip(*highAcc)
th = np.array(th)
xh = np.array(xh)
yh = np.array(yh)
zh = np.array(zh)

# Calculate rolling variance
varl = pd.Series(xl).rolling(window=50).var().to_numpy() + pd.Series(yl).rolling(window=50).var().to_numpy() + pd.Series(zl).rolling(window=50).var().to_numpy()


VAR_THRESHOLD = 0.00002
BUFFER_TIME = 0.1

mask_below = varl < VAR_THRESHOLD

padded_mask = np.concatenate(([False], mask_below, [False]))
diffs = np.diff(padded_mask.astype(int))

starts = np.where(diffs == 1)[0]
ends = np.where(diffs == -1)[0] - 1


# Average points for each stationary window
xl_avg, yl_avg, zl_avg = [], [], []
xh_avg, yh_avg, zh_avg = [], [], []

for s, e in zip(starts, ends):
    t_start_buffered = tl[s] + BUFFER_TIME
    t_end_buffered = tl[e] - BUFFER_TIME

    # Skip if the buffer time makes the window negative/empty
    if t_start_buffered >= t_end_buffered:
        continue

    # Average the Low-G data in this time window
    segment_times = tl[s:e+1]
    valid_in_segment_l = (segment_times >= t_start_buffered) & (segment_times <= t_end_buffered)

    if np.any(valid_in_segment_l):
        xl_avg.append(np.mean(xl[s:e+1][valid_in_segment_l]))
        yl_avg.append(np.mean(yl[s:e+1][valid_in_segment_l]))
        zl_avg.append(np.mean(zl[s:e+1][valid_in_segment_l]))

    # Average the High-G data in the same time window
    valid_in_segment_h = (th >= t_start_buffered) & (th <= t_end_buffered)

    if np.any(valid_in_segment_h):
        xh_avg.append(np.mean(xh[valid_in_segment_h]))
        yh_avg.append(np.mean(yh[valid_in_segment_h]))
        zh_avg.append(np.mean(zh[valid_in_segment_h]))

xl_filt, yl_filt, zl_filt = np.array(xl_avg), np.array(yl_avg), np.array(zl_avg)
xh_filt, yh_filt, zh_filt = np.array(xh_avg), np.array(yh_avg), np.array(zh_avg)

print(f"Extracted {len(xl_filt)} stable orientations for calibration.")


## Calibrate accelerometers
print("\n\nLow-G accelerometer calibration values:")
tools.UnitSphereCalibrate(xl_filt, yl_filt, zl_filt)

print("\n\nHigh-G accelerometer calibration values:")
tools.UnitSphereCalibrate(xh_filt, yh_filt, zh_filt)


"""
fig, axs = plt.subplots(1, 2, figsize=(10, 6))
fig.suptitle("Accelerometer data", fontsize=16)

# Low-G Accelerometer
axs[0].plot(tl, xl, label='X')
axs[0].plot(tl, yl, label='Y')
axs[0].plot(tl, zl, label='Z')
# axs[0].plot(tl, magl, label='Mag')
# axs[0].plot(tl, varl, label='Var')
axs[0].set_title("Low-G Accelerometer")
axs[0].set_ylabel("Accel")
axs[0].legend(loc='upper right')

# High-G Accelerometer
axs[1].plot(th, xh, label='X')
axs[1].plot(th, yh, label='Y')
axs[1].plot(th, zh, label='Z')
# axs[1].plot(th, magh, label='Mag')
axs[1].set_title("High-G Accelerometer")
axs[1].set_ylabel("Accel")
axs[1].legend(loc='upper right')


axs[0].set_xlabel("Time (Seconds)")
axs[1].set_xlabel("Time (Seconds)")
plt.tight_layout()
plt.show()
"""


