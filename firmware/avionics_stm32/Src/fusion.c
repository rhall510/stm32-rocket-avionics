#include "fusion.h"


// *** ORIENTATION FILTER ***
// State vector (orientation quaternion)
float32_t OF_STATE[4] = {1.0f, 0.0f, 0.0f, 0.0f};
arm_matrix_instance_f32 OF_STATE_M;

// Uncertainty matrix
float32_t OF_UNCRT[9] = {1.0f, 0.0f, 0.0f,
						 0.0f, 1.0f, 0.0f,
						 0.0f, 0.0f, 1.0f};
arm_matrix_instance_f32 OF_UNCRT_M;

// Gyroscope noise
float32_t OF_GYR_NOISE[9] = {0.01f, 0.0f, 0.0f,
						 	 0.0f, 0.01f, 0.0f,
							 0.0f, 0.0f, 0.01f};
arm_matrix_instance_f32 OF_GYR_NOISE_M;

// Accelerometer noise
float32_t OF_ACC_NOISE[9] = {0.1f, 0.0f, 0.0f,
						 	 0.0f, 0.1f, 0.0f,
							 0.0f, 0.0f, 0.1f};
arm_matrix_instance_f32 OF_ACC_NOISE_M;

// Magnetometer noise
float32_t OF_MAG_NOISE[9] = {0.5f, 0.0f, 0.0f,
						 	 0.0f, 0.5f, 0.0f,
							 0.0f, 0.0f, 0.5f};
arm_matrix_instance_f32 OF_MAG_NOISE_M;

// Magnetic declination
float MAG_DEC = DEG2RAD(1.08);


void QuatMult(float *q1, float *q2, float *out) {
	// Extract first in case out is a pointer to one of the input arrays
	float w1 = q1[0];
	float x1 = q1[1];
	float y1 = q1[2];
	float z1 = q1[3];

	float w2 = q2[0];
	float x2 = q2[1];
	float y2 = q2[2];
	float z2 = q2[3];

	out[0] = w1*w2 - x1*x2 - y1*y2 - z1*z2;
	out[1] = w1*x2 + x1*w2 + y1*z2 - z1*y2;
	out[2] = w1*y2 - x1*z2 + y1*w2 + z1*x2;
	out[3] = w1*z2 + x1*y2 - y1*x2 + z1*w2;
}


void QuatToEuler(float *q, float *e) {
    float w = q[0];
    float x = q[1];
    float y = q[2];
    float z = q[3];

    float ysqr = y * y;

    // X axis (roll)
    float t0 = 2.0f * (w * x + y * z);
    float t1 = 1.0f - 2.0f * (x * x + ysqr);

    float atan2;
    arm_atan2_f32(t0, t1, &atan2);

    float X = RAD2DEG(atan2);

    // Y axis (pitch)
    float t2 = 2.0f * (w * y - z * x);

    // Clamp t2 to prevent bad results from asinf due to floating point inaccuracies
    t2 = t2 > 1.0f ? 1.0f : t2;
    t2 = t2 < -1.0f ? -1.0f : t2;
    float Y = RAD2DEG(asinf(t2));

    // Z axis (yaw)
    float t3 = 2.0f * (w * z + x * y);
    float t4 = 1.0f - 2.0f * (ysqr + z * z);

    arm_atan2_f32(t3, t4, &atan2);
    float Z = RAD2DEG(atan2);


    e[0] = X;
    e[1] = Y;
    e[2] = Z;
}


void QuatToMatrix(float *q, float *m) {
    float qw = q[0];
    float qx = q[1];
    float qy = q[2];
    float qz = q[3];

    // Pre calculate squares
    float qx2 = qx * qx;
    float qy2 = qy * qy;
    float qz2 = qz * qz;


    m[0] = 1.0f - 2.0f * qy2 - 2.0f * qz2;
    m[1] = 2.0f * qx * qy - 2.0f * qz * qw;
    m[2] = 2.0f * qx * qz + 2.0f * qy * qw;

    m[3] = 2.0f * qx * qy + 2.0f * qz * qw;
    m[4] = 1.0f - 2.0f * qx2 - 2.0f * qz2;
    m[5] = 2.0f * qy * qz - 2.0f * qx * qw;

    m[6] = 2.0f * qx * qz - 2.0f * qy * qw;
    m[7] = 2.0f * qy * qz + 2.0f * qx * qw;
    m[8] = 1.0f - 2.0f * qx2 - 2.0f * qy2;
}


void SkewMatrix(float *v, float *m) {
    m[0] =  0.0f;
    m[1] = -v[2];
    m[2] =  v[1];

    m[3] =  v[2];
    m[4] =  0.0f;
    m[5] = -v[0];

    m[6] = -v[1];
    m[7] =  v[0];
    m[8] =  0.0f;
}


void GetExpectedMag(float *q_est, float *mag_meas, float *mag_exp) {
	// Make the rotation matrix from the current estimated orientation
	float32_t R[9];
	arm_matrix_instance_f32 R_Mat;
	arm_mat_init_f32(&R_Mat, 3, 3, R);

	QuatToMatrix(q_est, R);

	// Rotate the measured vector to the navigation frame
	float32_t H[3];
	arm_matrix_instance_f32 H_Mat;
	arm_mat_init_f32(&H_Mat, 1, 3, H);

	arm_matrix_instance_f32 MM_Mat;
	arm_mat_init_f32(&MM_Mat, 1, 3, mag_meas);

	arm_mat_mult_f32(&R_Mat, &MM_Mat, &H_Mat);


	// Build idealised reference vector with all horizontal strength along the X axis and true north correction
	float bx;
	arm_sqrt_f32(H[0]*H[0] + H[1]*H[1], &bx);
	float b_nav[3] = {bx * arm_cos_f32(MAG_DEC), bx * arm_sin_f32(MAG_DEC), H[2]};

	arm_matrix_instance_f32 BN_Mat;
	arm_mat_init_f32(&BN_Mat, 1, 3, b_nav);

	// Rotate back to the body frame
	arm_matrix_instance_f32 ME_Mat;
	arm_mat_init_f32(&ME_Mat, 1, 3, mag_exp);


	float32_t RT[9];
	arm_matrix_instance_f32 RT_Mat;
	arm_mat_init_f32(&RT_Mat, 3, 3, RT);

	arm_mat_trans_f32(&R_Mat, &RT_Mat);
	arm_mat_mult_f32(&RT_Mat, &BN_Mat, &ME_Mat);
}





void InitialiseOrientationFilter() {
	arm_mat_init_f32(&OF_STATE_M, 1, 4, OF_STATE);
	arm_mat_init_f32(&OF_UNCRT_M, 3, 3, OF_UNCRT);
	arm_mat_init_f32(&OF_GYR_NOISE_M, 3, 3, OF_GYR_NOISE);
	arm_mat_init_f32(&OF_ACC_NOISE_M, 3, 3, OF_ACC_NOISE);
	arm_mat_init_f32(&OF_MAG_NOISE_M, 3, 3, OF_MAG_NOISE);
}


void OF_NewGyrData(SensorData *data) {
	static float t_last = 0.0f;
	float dt = data->data.tsvec3.Timestamp - t_last;

	if (t_last == 0.0f) { return; }   // Skip first reading (no dt to calculate)

	t_last = data->data.tsvec3.Timestamp;

	// Calculate rotation done in timestep
	float dang[3] = {DEG2RAD(data->data.tsvec3.X) * dt, DEG2RAD(data->data.tsvec3.Y) * dt, DEG2RAD(data->data.tsvec3.Z) * dt};

	float theta;
	arm_sqrt_f32(dang[0]*dang[0] + dang[1]*dang[1] + dang[2]*dang[2], &theta);

	float dq[4] = {0.0f};

	if (theta > 1e-8f) {   // If a measurable amount of rotation happened, calculate the rotation quaternion using exponential map
		float sht = arm_sin_f32(theta / 2.0f);

		dq[0] = arm_cos_f32(theta / 2.0f);
		dq[1] = sht * dang[0] / theta;
		dq[2] = sht * dang[1] / theta;
		dq[3] = sht * dang[2] / theta;
	}


	// Apply the rotation and normalise
	float q_upd[4];
	QuatMult(OF_STATE, dq, q_upd);

	float qmag;
	arm_sqrt_f32(q_upd[0]*q_upd[0] + q_upd[1]*q_upd[1] + q_upd[2]*q_upd[2] + q_upd[3]*q_upd[3], &qmag);
	q_upd[0] = q_upd[0] / qmag;
	q_upd[1] = q_upd[1] / qmag;
	q_upd[2] = q_upd[2] / qmag;
	q_upd[3] = q_upd[3] / qmag;


	// Build the state transition matrix
	float skew_dang[9];
	SkewMatrix(dq, skew_dang);

	float F[9];
	for (int i = 0; i < 9; i++) {   // Identity minus skew matrix
		if (i % 4 == 0) {
			F[i] = 1.0f - skew_dang[i];
		} else {
			F[i] = -skew_dang[i];
		}
	}

	arm_matrix_instance_f32 F_Mat;
	arm_mat_init_f32(&F_Mat, 3, 3, F);


	// Grow uncertainty
	float FT[9];
	arm_matrix_instance_f32 FT_Mat;
	arm_mat_init_f32(&FT_Mat, 3, 3, FT);
	arm_mat_trans_f32(&F_Mat, &FT_Mat);


	float FP[9];
	arm_matrix_instance_f32 FP_Mat;
	arm_mat_init_f32(&FP_Mat, 3, 3, FP);

	arm_mat_mult_f32(&F_Mat, &OF_UNCRT_M, &FP_Mat);


	float FPFT[9];
	arm_matrix_instance_f32 FPFT_Mat;
	arm_mat_init_f32(&FPFT_Mat, 3, 3, FPFT);

	arm_mat_mult_f32(&FP_Mat, &FT_Mat, &FPFT_Mat);


	// Add gyroscope noise
	for (int i = 0; i < 9; i++) {
		OF_UNCRT[i] += OF_GYR_NOISE[i] * dt;
	}
}


void OF_NewAccData(SensorData *data) {

}


void OF_NewMagData(SensorData *data) {

}






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


















