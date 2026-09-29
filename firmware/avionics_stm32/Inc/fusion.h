#ifndef FUSION_H_
#define FUSION_H_


#include "datatypes.h"
#include "arm_math.h"
#include <math.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"


// Angle converters
#define RAD2DEG(x) ((x) * (180.0f / M_PI))
#define DEG2RAD(x) ((x) * (M_PI / 180.0f))



// Calibration values
extern float GYR_CAL_CENT[3];

extern float MAG_CAL_CENT[3];
extern float MAG_CAL_DIST[9];

extern float LACC_CAL_CENT[3];
extern float LACC_CAL_DIST[9];

extern float HACC_CAL_CENT[3];
extern float HACC_CAL_DIST[9];


// Reading calibration helpers
// Simple offset calibration for sensor data vectors in the TS_Vec3 format. Subtracts the offset IN PLACE
void CalVectorCent(TS_Vec3 *vec, float *offset);

// Calibrate sensor data vectors in the TS_Vec3 format by subtracting offset and correcting distortions using dist (float[9]) IN PLACE
void CalVectorCentDist(TS_Vec3 *vec, float *offset, float *dist);




// Configuration
// Threshold above which a reading from the low range accelerometer is considered unreliable
// The high range accelerometer readings will be used instead while above this threshold
#define LRACC_SAT_THRESH 7.0f




// Update all filters based on new sensor data
void Fusion_NewLRAccData(SensorData *data);
void Fusion_NewGyrData(SensorData *data);
void Fusion_NewHRAccData(SensorData *data);
void Fusion_NewPressData(SensorData *data);
void Fusion_NewMagData(SensorData *data);
void Fusion_NewGPSData(SensorData *data);



// *** ORIENTATION FILTER ***
// Initialise the orientation filter matrices
void InitialiseOrientationFilter();

// Update the filters orientation estimate based on new sensor data
void OF_NewGyrData(SensorData *data, float dt);
void OF_NewAccData(SensorData *data);
void OF_NewMagData(SensorData *data);


// Helper functions
// Multiply q1 and q2 quaternions and place the result in out
void QuatMult(float *q1, float *q2, float *out);

// Convert the quaternion q to euler angle representation e
void QuatToEuler(float *q, float *e);

// Convert the quaternion q to a 3x3 matrix representing the rotation m
void QuatToMatrix(float *q, float *m);

// Make a 3x3 skew-symmetric matrix m from a 1x3 vector v
void SkewMatrix(float *v, float *m);

// Get the expected magnetometer reading based on the current estimated rotation
// Corrects for magnetic declination and uses an actual measurement (mag_meas) to account for the downward component of readings
void GetExpectedMag(float *q_est, float *mag_meas, float *mag_exp);

// Get the expected gravity vector based on the current estimated rotation
void GetExpectedGravity(float *q_est, float *g_exp);

// Expose state
extern float32_t OF_STATE[4];




// *** VERTICAL FILTER ***
// Initialise the vertical filter matrices and set the ground reference
void InitialiseVerticalFilter(float GroundPress, float GroundAlt);

// Update the filters altitude estimate based on new sensor data
void VF_NewAccData(SensorData *data, float dt);
void VF_NewPressData(SensorData *data);
void VF_NewGPSData(SensorData *data);

// Convert a pressure reading to altitude based on the current ground reference
float PressToAlt(float Pressure);

// Get the navigation frame acceleration (lacc) based on the current estimated rotation (q_est) and body frame acceleration (macc)
void GetLinearAcceleration(float *q_est, float *macc, float *lacc);

// Expose state
extern float32_t VF_STATE[2];




// *** HORIZONTAL FILTER ***
// Initialise the horizontal filter matrices and set the home GPS coordinates
void InitialiseHorizontalFilter(float HomeLat, float HomeLon);

// Update the filters horizontal tracking estimate based on new sensor data
void HF_NewAccData(SensorData *data, float dt);
void HF_NewGPSData(SensorData *data);

// Convert a GPS coordinate to flat Earth meters relative to the home position
void LatLonToMeters(float lat, float lon, float *meters_north, float *meters_east);

// Expose state
extern float32_t HF_STATE[4];

// Export navigation frame acceleration for flight state machine
extern float32_t NAV_ACCEL[3];


#endif /* FUSION_H_ */
