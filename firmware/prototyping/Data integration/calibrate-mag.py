import matplotlib.pyplot as plt
import numpy as np
import tools


CALIBRATION_FILE = 'flight_dataroll.bin'

# Validate each data chunk and parse contents
lowAcc = []
highAcc = []
gyr = []
mag = []
temp = []
press = []
gps = []

lowAcc, highAcc, gyr, mag, temp, press, gps = tools.ReadBinaryData(CALIBRATION_FILE)


## Calibrate magnetometer
t, x, y, z = zip(*mag)

x = np.array(x)
y = np.array(y)
z = np.array(z)

tools.UnitSphereCalibrate(x, y, z)





