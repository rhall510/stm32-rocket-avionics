#ifndef FUSION_H_
#define FUSION_H_


#include "datatypes.h"
#include "arm_math.h"


// Update all filters based on new sensor data
void Fusion_NewLRAccData(SensorData *data);
void Fusion_NewGyrData(SensorData *data);
void Fusion_NewHRAccData(SensorData *data);
void Fusion_NewPressData(SensorData *data);
void Fusion_NewMagData(SensorData *data);
void Fusion_NewGPSData(SensorData *data);



// *** VERTICAL FILTER ***
// Initialise the vertical filter matrices and set the ground reference
void InitialiseVerticalFilter(float GroundPress, float GroundAlt);

// Update the filters altitude estimate based on new sensor data
void VF_NewAccData(SensorData *data);
void VF_NewPressData(SensorData *data);
void VF_NewGPSData(SensorData *data);

// Get the filters current estimate of altitude relative to the ground reference
float VF_GetCurrentEstimate();

// Convert a pressure reading to altitude based on the current ground reference
float PressToAlt(float Pressure);



#endif /* FUSION_H_ */
