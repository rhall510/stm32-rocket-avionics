#include "fusion.h"


// *** VERTICAL FILTER ***
// State vector: Altitude (m), Vertical velocity (m/s)
float32_t VF_STATE[2] = {0.0f, 0.0f};
arm_matrix_instance_f32 VF_STATE_M;

// Uncertainty matrix
float32_t VF_UNCRT[4] = {0.0f, 0.0f,
						 0.0f, 0.0f};
arm_matrix_instance_f32 VF_UNCRT_M;


// Barometer static noise
float R_Baro = 2.0f;

// GPS vertical noise multipliers (high as barometer should be more accurate short term)
float VF_GPS_NMult_pos = 15.0f;
float VF_GPS_NMult_vel = 30.0f;

// Base accelerometer noise added with each prediction step on top of the white noise covariance
float VF_R_Accel = 0.01f;

// Ground reference values
float VF_GRND_PRESS = 0.0f;
float VF_GRND_ALT = 0.0f;




void InitialiseVerticalFilter(float GroundPress, float GroundAlt) {
	// Set the ground reference
	VF_GRND_PRESS = GroundPress;
	VF_GRND_ALT = GroundAlt;

	VF_STATE[0] = GroundAlt;

	// Initialise matrices
	arm_mat_init_f32(&VF_STATE_M, 1, 2, VF_STATE);
	arm_mat_init_f32(&VF_UNCRT_M, 2, 2, VF_UNCRT);
}


void VF_NewAccData(SensorData *data) {

}

void VF_NewPressData(SensorData *data) {

}

void VF_NewGPSData(SensorData *data) {

}


inline float VF_GetCurrentEstimate() {
	return VF_STATE[0];
}


inline float PressToAlt(float Pressure) {
	return 44330.0f * (1.0f - powf(Pressure / VF_GRND_PRESS, 0.1903f)) + VF_GRND_ALT;
}











void Fusion_NewLRAccData(SensorData *data) {

}

void Fusion_NewGyrData(SensorData *data) {

}

void Fusion_NewHRAccData(SensorData *data) {

}

void Fusion_NewPressData(SensorData *data) {

}

void Fusion_NewMagData(SensorData *data) {

}

void Fusion_NewGPSData(SensorData *data) {

}


















