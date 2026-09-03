#ifndef FUSION_H_
#define FUSION_H_


#include "datatypes.h"
#include "arm_math.h"
#include <math.h>


// Angle converters
#define RAD2DEG(x) ((x) * (180.0f / M_PI))
#define DEG2RAD(x) ((x) * (M_PI / 180.0f))


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
void OF_NewGyrData(SensorData *data);
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



// *** VERTICAL FILTER ***
// Initialise the vertical filter matrices and set the ground reference
void InitialiseVerticalFilter(float GroundPress, float GroundAlt);

// Update the filters altitude estimate based on new sensor data
void VF_NewAccData(SensorData *data);
void VF_NewPressData(SensorData *data);
void VF_NewGPSData(SensorData *data);

// Convert a pressure reading to altitude based on the current ground reference
float PressToAlt(float Pressure);



#endif /* FUSION_H_ */
