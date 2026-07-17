import matplotlib.pyplot as plt
import numpy as np
import tools


CALIBRATION_FILE = 'flight_datastill.bin'

# Validate each data chunk and parse contents
lowAcc = []
highAcc = []
gyr = []
mag = []
temp = []
press = []
gps = []

lowAcc, highAcc, gyr, mag, temp, press, gps = tools.ReadBinaryData(CALIBRATION_FILE)


## Calibrate gyroscope
t, x, y, z = zip(*gyr)

x = np.array(x)
y = np.array(y)
z = np.array(z)

xoff = np.mean(x[200:-200])
yoff = np.mean(y[200:-200])
zoff = np.mean(z[200:-200])

xcorr = x - xoff
ycorr = y - yoff
zcorr = z - zoff

print(f"Gyroscope offsets:\n X = {-xoff}\n Y = {-yoff}\n Z = {-zoff}")

fig, axs = plt.subplots(1, 2, figsize=(10, 6))
fig.suptitle("Gyroscope data", fontsize=16)

# Uncalibrated
axs[0].plot(t[100:-100], x[100:-100], label='X')
axs[0].plot(t[100:-100], y[100:-100], label='Y')
axs[0].plot(t[100:-100], z[100:-100], label='Z')
axs[0].set_title("Gyroscope uncalibrated")
axs[0].set_ylabel("Rate (deg/s)")
axs[0].legend(loc='upper right')
axs[0].set_xlabel("Time (Seconds)")

# Calibrated
axs[1].plot(t[100:-100], xcorr[100:-100], label='X')
axs[1].plot(t[100:-100], ycorr[100:-100], label='Y')
axs[1].plot(t[100:-100], zcorr[100:-100], label='Z')
axs[1].set_title("Gyroscope calibrated")
axs[1].set_ylabel("Rate (deg/s)")
axs[1].legend(loc='upper right')
axs[1].set_xlabel("Time (Seconds)")

plt.tight_layout()
plt.show()



