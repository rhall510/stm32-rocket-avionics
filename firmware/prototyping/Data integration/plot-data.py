import matplotlib.pyplot as plt
import tools


READ_FILE = 'flight_datasr.bin'

lowAcc = []
highAcc = []
gyr = []
mag = []
temp = []
press = []
gps = []

lowAcc, highAcc, gyr, mag, temp, press, gps = tools.ReadBinaryData(READ_FILE)

print("Parsing done. Generating plots...")

fig, axs = plt.subplots(4, 2, figsize=(12, 12), sharex=True)
fig.suptitle("Avionics Telemetry Data Over Time", fontsize=16)

# Low-G Accelerometer
if lowAcc:
    t, x, y, z = zip(*lowAcc)
    axs[0, 0].plot(t, x, label='X')
    axs[0, 0].plot(t, y, label='Y')
    axs[0, 0].plot(t, z, label='Z')
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
    axs[0, 1].plot(t, x, label='X')
    axs[0, 1].plot(t, y, label='Y')
    axs[0, 1].plot(t, z, label='Z')
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
    t, lat, long, alt, velNorth, velEast, velDown, groundSpd, heading, horzAcc, vertAcc, spdAcc, sats, fix = zip(*gps)

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
    ax_spd.plot(t, groundSpd, label='Speed', color='green')
    ax_spd.set_ylabel("Speed", color='green')
    ax_spd.tick_params(axis='y', labelcolor='green')

    axs[3, 1].set_title("GPS Altitude & Speed")


axs[-1, 0].set_xlabel("Time (Seconds)")
axs[-1, 1].set_xlabel("Time (Seconds)")
plt.tight_layout()
plt.show()


print(len(lowAcc), len(highAcc), len(gyr), len(mag), len(temp), len(press), len(gps))

