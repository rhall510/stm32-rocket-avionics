#include "fusion.h"


// Timestamps for calculating delta time
static float gyr_tlast = 0.0f;
static float acc_tlast = 0.0f;

// If the last low range accelerometer reading was above the safe threshold
static bool lracc_saturated = false;



// *** ORIENTATION FILTER ***
// State vector (orientation quaternion)
float32_t OF_STATE[4] = {1.0f, 0.0f, 0.0f, 0.0f};

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


void GetExpectedGravity(float *q_est, float *g_exp) {
    float qw = q_est[0];
    float qx = q_est[1];
    float qy = q_est[2];
    float qz = q_est[3];

    g_exp[0] = -(2.0f * qx * qz - 2.0f * qy * qw);
    g_exp[1] = -(2.0f * qy * qz + 2.0f * qx * qw);
    g_exp[2] = -(1.0f - 2.0f * qx * qx - 2.0f * qy * qy);
}



void InitialiseOrientationFilter() {
	arm_mat_init_f32(&OF_UNCRT_M, 3, 3, OF_UNCRT);
	arm_mat_init_f32(&OF_GYR_NOISE_M, 3, 3, OF_GYR_NOISE);
	arm_mat_init_f32(&OF_ACC_NOISE_M, 3, 3, OF_ACC_NOISE);
	arm_mat_init_f32(&OF_MAG_NOISE_M, 3, 3, OF_MAG_NOISE);
}


void OF_NewGyrData(SensorData *data, float dt) {
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


// Helper function to handle the MEKF measurement update step
static void OF_MeasurementTaken(float *act_vec, float *exp_vec, arm_matrix_instance_f32 *R_Mat) {
    // Normalize vectors
    float a_mag, e_mag;
    arm_sqrt_f32(act_vec[0]*act_vec[0] + act_vec[1]*act_vec[1] + act_vec[2]*act_vec[2], &a_mag);
    act_vec[0] /= a_mag;
    act_vec[1] /= a_mag;
    act_vec[2] /= a_mag;

    arm_sqrt_f32(exp_vec[0]*exp_vec[0] + exp_vec[1]*exp_vec[1] + exp_vec[2]*exp_vec[2], &e_mag);
    exp_vec[0] /= e_mag;
    exp_vec[1] /= e_mag;
    exp_vec[2] /= e_mag;

    // Calculate error
    float err[3] = {act_vec[0] - exp_vec[0], act_vec[1] - exp_vec[1], act_vec[2] - exp_vec[2]};
    arm_matrix_instance_f32 ERR_Mat;
    arm_mat_init_f32(&ERR_Mat, 3, 1, err);

    // H matrix (skew symmetric of expected vector)
    float H[9];
    SkewMatrix(exp_vec, H);
    arm_matrix_instance_f32 H_Mat;
    arm_mat_init_f32(&H_Mat, 3, 3, H);

    // H transpose
    float HT[9];
    arm_matrix_instance_f32 HT_Mat;
    arm_mat_init_f32(&HT_Mat, 3, 3, HT);
    arm_mat_trans_f32(&H_Mat, &HT_Mat);

    // Calculate S = H * P * H^T + R
    float HP[9];
    arm_matrix_instance_f32 HP_Mat;
    arm_mat_init_f32(&HP_Mat, 3, 3, HP);
    arm_mat_mult_f32(&H_Mat, &OF_UNCRT_M, &HP_Mat);

    float HPHT[9];
    arm_matrix_instance_f32 HPHT_Mat;
    arm_mat_init_f32(&HPHT_Mat, 3, 3, HPHT);
    arm_mat_mult_f32(&HP_Mat, &HT_Mat, &HPHT_Mat);

    float S[9];
    arm_matrix_instance_f32 S_Mat;
    arm_mat_init_f32(&S_Mat, 3, 3, S);
    arm_mat_add_f32(&HPHT_Mat, R_Mat, &S_Mat);

    // Inverse S
    float SI[9];
    arm_matrix_instance_f32 SI_Mat;
    arm_mat_init_f32(&SI_Mat, 3, 3, SI);
    arm_mat_inverse_f32(&S_Mat, &SI_Mat);

    // Calculate Kalman Gain K = P * H^T * S^-1
    float PHT[9];
    arm_matrix_instance_f32 PHT_Mat;
    arm_mat_init_f32(&PHT_Mat, 3, 3, PHT);
    arm_mat_mult_f32(&OF_UNCRT_M, &HT_Mat, &PHT_Mat);

    float K[9];
    arm_matrix_instance_f32 K_Mat;
    arm_mat_init_f32(&K_Mat, 3, 3, K);
    arm_mat_mult_f32(&PHT_Mat, &SI_Mat, &K_Mat);

    // Calculate delta_theta = K * error
    float dtheta[3];
    arm_matrix_instance_f32 DTHETA_Mat;
    arm_mat_init_f32(&DTHETA_Mat, 3, 1, dtheta);
    arm_mat_mult_f32(&K_Mat, &ERR_Mat, &DTHETA_Mat);

    // Apply the rotation and normalise
    float theta;
    arm_sqrt_f32(dtheta[0]*dtheta[0] + dtheta[1]*dtheta[1] + dtheta[2]*dtheta[2], &theta);

    float dq[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    if (theta > 1e-8f) {
        float sht = arm_sin_f32(theta / 2.0f);
        dq[0] = arm_cos_f32(theta / 2.0f);
        dq[1] = sht * dtheta[0] / theta;
        dq[2] = sht * dtheta[1] / theta;
        dq[3] = sht * dtheta[2] / theta;
    }

    float q_upd[4];
    QuatMult(OF_STATE, dq, q_upd);

    float qmag;
    arm_sqrt_f32(q_upd[0]*q_upd[0] + q_upd[1]*q_upd[1] + q_upd[2]*q_upd[2] + q_upd[3]*q_upd[3], &qmag);
    OF_STATE[0] = q_upd[0] / qmag;
    OF_STATE[1] = q_upd[1] / qmag;
    OF_STATE[2] = q_upd[2] / qmag;
    OF_STATE[3] = q_upd[3] / qmag;

    // Shrink uncertainty using Joseph form: P_new = (I - KH) * P * (I - KH)^T + K * R * K^T
    float KH[9];
    arm_matrix_instance_f32 KH_Mat;
    arm_mat_init_f32(&KH_Mat, 3, 3, KH);
    arm_mat_mult_f32(&K_Mat, &H_Mat, &KH_Mat);

    float I_mat[9] = {1.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f,
                      0.0f, 0.0f, 1.0f};
    arm_matrix_instance_f32 I_Mat_inst;
    arm_mat_init_f32(&I_Mat_inst, 3, 3, I_mat);

    float IKH[9];
    arm_matrix_instance_f32 IKH_Mat;
    arm_mat_init_f32(&IKH_Mat, 3, 3, IKH);
    arm_mat_sub_f32(&I_Mat_inst, &KH_Mat, &IKH_Mat);

    float IKHT[9];
    arm_matrix_instance_f32 IKHT_Mat;
    arm_mat_init_f32(&IKHT_Mat, 3, 3, IKHT);
    arm_mat_trans_f32(&IKH_Mat, &IKHT_Mat);

    float IKHP[9];
    arm_matrix_instance_f32 IKHP_Mat;
    arm_mat_init_f32(&IKHP_Mat, 3, 3, IKHP);
    arm_mat_mult_f32(&IKH_Mat, &OF_UNCRT_M, &IKHP_Mat);

    float Term1[9];
    arm_matrix_instance_f32 Term1_Mat;
    arm_mat_init_f32(&Term1_Mat, 3, 3, Term1);
    arm_mat_mult_f32(&IKHP_Mat, &IKHT_Mat, &Term1_Mat);

    float KT[9];
    arm_matrix_instance_f32 KT_Mat;
    arm_mat_init_f32(&KT_Mat, 3, 3, KT);
    arm_mat_trans_f32(&K_Mat, &KT_Mat);

    float KR[9];
    arm_matrix_instance_f32 KR_Mat;
    arm_mat_init_f32(&KR_Mat, 3, 3, KR);
    arm_mat_mult_f32(&K_Mat, R_Mat, &KR_Mat);

    float Term2[9];
    arm_matrix_instance_f32 Term2_Mat;
    arm_mat_init_f32(&Term2_Mat, 3, 3, Term2);
    arm_mat_mult_f32(&KR_Mat, &KT_Mat, &Term2_Mat);


    arm_mat_add_f32(&Term1_Mat, &Term2_Mat, &OF_UNCRT_M);
}


void OF_NewAccData(SensorData *data) {
    float a_act[3] = {data->data.tsvec3.X, data->data.tsvec3.Y, data->data.tsvec3.Z};

    // Calculate dynamic variance based on deviation from 1G
    float a_mag;
    arm_sqrt_f32(a_act[0]*a_act[0] + a_act[1]*a_act[1] + a_act[2]*a_act[2], &a_mag);

    float g_error = fabsf(a_mag - 1.0f);
    float dyn_var = 0.1f + (1000.0f * (g_error * g_error));

    float R_dyn[9] = {dyn_var, 0.0f, 0.0f,
                      0.0f, dyn_var, 0.0f,
                      0.0f, 0.0f, dyn_var};

    arm_matrix_instance_f32 R_Mat;
    arm_mat_init_f32(&R_Mat, 3, 3, R_dyn);

    // Calculate expected gravity vector
    float a_exp[3];
    GetExpectedGravity(OF_STATE, a_exp);

    // Run sensor update step
    OF_MeasurementTaken(a_act, a_exp, &R_Mat);
}


void OF_NewMagData(SensorData *data) {
    float m_act[3] = {data->data.tsvec3.X, data->data.tsvec3.Y, data->data.tsvec3.Z};

	float m_exp[3];
    GetExpectedMag(OF_STATE, m_act, m_exp);

    // Run sensor update step
    OF_MeasurementTaken(m_act, m_exp, &OF_MAG_NOISE_M);
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
	VF_STATE[1] = 0.0f;

	// Initialise matrices
	arm_mat_init_f32(&VF_STATE_M, 2, 1, VF_STATE);
	arm_mat_init_f32(&VF_UNCRT_M, 2, 2, VF_UNCRT);
}


void VF_NewAccData(SensorData *data, float dt) {
    // Get vertical component of linear acceleration
    float a_comp[3] = {data->data.tsvec3.X, data->data.tsvec3.Y, data->data.tsvec3.Z};
    float a_nav[3];
    GetLinearAcceleration(OF_STATE, a_comp, a_nav);

    // State prediction (X = F*X + B*u)
    // State transition matrix
    float F[4] = {1.0f, dt,
                  0.0f, 1.0f};
    arm_matrix_instance_f32 F_Mat;
    arm_mat_init_f32(&F_Mat, 2, 2, F);

    float X_pred[2];
    arm_matrix_instance_f32 X_Pred_Mat;
    arm_mat_init_f32(&X_Pred_Mat, 2, 1, X_pred);

    arm_mat_mult_f32(&F_Mat, &VF_STATE_M, &X_Pred_Mat);

    // Control input matrix
    float B[2] = {0.5f * dt * dt, dt};

    // Predict new state
    VF_STATE[0] = X_pred[0] + (B[0] * a_nav[2]);
    VF_STATE[1] = X_pred[1] + (B[1] * a_nav[2]);

    // Covariance prediction (P = F*P*F^T + Q)
    // Dynamically calculate vertical kinematic process noise
    float sigma_alt = 2.0f;   // Vertical acceleration variance
    float dt2 = dt * dt;

    float qa_p = sigma_alt * (dt2 * dt / 3.0f) + (VF_R_Accel * dt);
    float qa_c = sigma_alt * (dt2 / 2.0f);
    float qa_v = sigma_alt * dt;

    float Q[4] = {qa_p, qa_c,
                  qa_c, qa_v};
    arm_matrix_instance_f32 Q_Mat;
    arm_mat_init_f32(&Q_Mat, 2, 2, Q);

    // Grow uncertainty
    float FT[4];
    arm_matrix_instance_f32 FT_Mat;
    arm_mat_init_f32(&FT_Mat, 2, 2, FT);
    arm_mat_trans_f32(&F_Mat, &FT_Mat);

    float FP[4];
    arm_matrix_instance_f32 FP_Mat;
    arm_mat_init_f32(&FP_Mat, 2, 2, FP);
    arm_mat_mult_f32(&F_Mat, &VF_UNCRT_M, &FP_Mat);

    float FPFT[4];
    arm_matrix_instance_f32 FPFT_Mat;
    arm_mat_init_f32(&FPFT_Mat, 2, 2, FPFT);
    arm_mat_mult_f32(&FP_Mat, &FT_Mat, &FPFT_Mat);

    arm_mat_add_f32(&FPFT_Mat, &Q_Mat, &VF_UNCRT_M);
}


void VF_NewPressData(SensorData *data) {
    float measured_alt = PressToAlt(data->data.tsprstmp.Press);

    // Measurement matrix
    float H[2] = {1.0f, 0.0f};
    arm_matrix_instance_f32 H_Mat;
    arm_mat_init_f32(&H_Mat, 1, 2, H);

    // Calculate error and kalman gain
    float HT[2];
    arm_matrix_instance_f32 HT_Mat;
    arm_mat_init_f32(&HT_Mat, 2, 1, HT);
    arm_mat_trans_f32(&H_Mat, &HT_Mat);

    // S = H*P*H^T + R_Baro
    float HP[2];
    arm_matrix_instance_f32 HP_Mat;
    arm_mat_init_f32(&HP_Mat, 1, 2, HP);
    arm_mat_mult_f32(&H_Mat, &VF_UNCRT_M, &HP_Mat);

    float HPHT[1];
    arm_matrix_instance_f32 HPHT_Mat;
    arm_mat_init_f32(&HPHT_Mat, 1, 1, HPHT);
    arm_mat_mult_f32(&HP_Mat, &HT_Mat, &HPHT_Mat);

    float S = HPHT[0] + R_Baro;
    float SI = 1.0f / S;
    arm_matrix_instance_f32 SI_Mat;
    arm_mat_init_f32(&SI_Mat, 1, 1, &SI);

    // K = P*H^T*S^-1
    float PHT[2];
    arm_matrix_instance_f32 PHT_Mat;
    arm_mat_init_f32(&PHT_Mat, 2, 1, PHT);
    arm_mat_mult_f32(&VF_UNCRT_M, &HT_Mat, &PHT_Mat);

    float K[2];
    arm_matrix_instance_f32 K_Mat;
    arm_mat_init_f32(&K_Mat, 2, 1, K);
    arm_mat_mult_f32(&PHT_Mat, &SI_Mat, &K_Mat);

    // Correct the state
    float y = measured_alt - VF_STATE[0];
    VF_STATE[0] += K[0] * y;
    VF_STATE[1] += K[1] * y;

    // Shrink the uncertainty using Joseph form  P_new = (I - KH)*P*(I - KH)^T + K*R*K^T
    float KH[4];
    arm_matrix_instance_f32 KH_Mat;
    arm_mat_init_f32(&KH_Mat, 2, 2, KH);
    arm_mat_mult_f32(&K_Mat, &H_Mat, &KH_Mat);

    float I[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    arm_matrix_instance_f32 I_Mat;
    arm_mat_init_f32(&I_Mat, 2, 2, I);

    float IKH[4];
    arm_matrix_instance_f32 IKH_Mat;
    arm_mat_init_f32(&IKH_Mat, 2, 2, IKH);
    arm_mat_sub_f32(&I_Mat, &KH_Mat, &IKH_Mat);

    float IKHT[4];
    arm_matrix_instance_f32 IKHT_Mat;
    arm_mat_init_f32(&IKHT_Mat, 2, 2, IKHT);
    arm_mat_trans_f32(&IKH_Mat, &IKHT_Mat);

    float IKHP[4];
    arm_matrix_instance_f32 IKHP_Mat;
    arm_mat_init_f32(&IKHP_Mat, 2, 2, IKHP);
    arm_mat_mult_f32(&IKH_Mat, &VF_UNCRT_M, &IKHP_Mat);

    float T1[4];
    arm_matrix_instance_f32 T1_Mat;
    arm_mat_init_f32(&T1_Mat, 2, 2, T1);
    arm_mat_mult_f32(&IKHP_Mat, &IKHT_Mat, &T1_Mat);

    float KT[2];
    arm_matrix_instance_f32 KT_Mat;
    arm_mat_init_f32(&KT_Mat, 1, 2, KT);
    arm_mat_trans_f32(&K_Mat, &KT_Mat);

    float KR[2] = {K[0] * R_Baro, K[1] * R_Baro};
    arm_matrix_instance_f32 KR_Mat;
    arm_mat_init_f32(&KR_Mat, 2, 1, KR);

    float T2[4];
    arm_matrix_instance_f32 T2_Mat;
    arm_mat_init_f32(&T2_Mat, 2, 2, T2);
    arm_mat_mult_f32(&KR_Mat, &KT_Mat, &T2_Mat);

    arm_mat_add_f32(&T1_Mat, &T2_Mat, &VF_UNCRT_M);
}


void VF_NewGPSData(SensorData *data) {
	// Skip if no fix
    if (data->data.tsgps.FixType == 0) { return; }

    // Approximate vertical velocity accuracy
    float vsAcc = (data->data.tsgps.HorzAccuracy > 0.001f)
    			? (data->data.tsgps.SpeedAccuracy * data->data.tsgps.VertAccuracy / data->data.tsgps.HorzAccuracy)
    			: data->data.tsgps.SpeedAccuracy;

    // Dynamic variance matrix
    float q_pos = data->data.tsgps.VertAccuracy * data->data.tsgps.VertAccuracy * VF_GPS_NMult_pos;
    float q_vel = vsAcc * vsAcc * VF_GPS_NMult_vel;
    float Q[4] = {q_pos, 0.0f,
                  0.0f,  q_vel};
    arm_matrix_instance_f32 Q_Mat;
    arm_mat_init_f32(&Q_Mat, 2, 2, Q);

    // Measurement matrix (measures both altitude and vertical velocity)
    float H[4] = {1.0f, 0.0f,
                  0.0f, 1.0f};
    arm_matrix_instance_f32 H_Mat;
    arm_mat_init_f32(&H_Mat, 2, 2, H);

    // Calculate error and kalman gain
    float y[2] = {data->data.tsgps.Altitude - VF_STATE[0], -data->data.tsgps.VelDown - VF_STATE[1]};
    arm_matrix_instance_f32 Y_Mat;
    arm_mat_init_f32(&Y_Mat, 2, 1, y);

    float HT[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    arm_matrix_instance_f32 HT_Mat;
    arm_mat_init_f32(&HT_Mat, 2, 2, HT);

    float HP[4];
    arm_matrix_instance_f32 HP_Mat;
    arm_mat_init_f32(&HP_Mat, 2, 2, HP);
    arm_mat_mult_f32(&H_Mat, &VF_UNCRT_M, &HP_Mat);

    float HPHT[4];
    arm_matrix_instance_f32 HPHT_Mat;
    arm_mat_init_f32(&HPHT_Mat, 2, 2, HPHT);
    arm_mat_mult_f32(&HP_Mat, &HT_Mat, &HPHT_Mat);

    float S[4];
    arm_matrix_instance_f32 S_Mat;
    arm_mat_init_f32(&S_Mat, 2, 2, S);
    arm_mat_add_f32(&HPHT_Mat, &Q_Mat, &S_Mat);

    float SI[4];
    arm_matrix_instance_f32 SI_Mat;
    arm_mat_init_f32(&SI_Mat, 2, 2, SI);
    arm_mat_inverse_f32(&S_Mat, &SI_Mat);

    float PHT[4];
    arm_matrix_instance_f32 PHT_Mat;
    arm_mat_init_f32(&PHT_Mat, 2, 2, PHT);
    arm_mat_mult_f32(&VF_UNCRT_M, &HT_Mat, &PHT_Mat);

    float K[4];
    arm_matrix_instance_f32 K_Mat;
    arm_mat_init_f32(&K_Mat, 2, 2, K);
    arm_mat_mult_f32(&PHT_Mat, &SI_Mat, &K_Mat);

    // Correct the state
    float Ky[2];
    arm_matrix_instance_f32 Ky_Mat;
    arm_mat_init_f32(&Ky_Mat, 2, 1, Ky);
    arm_mat_mult_f32(&K_Mat, &Y_Mat, &Ky_Mat);

    VF_STATE[0] += Ky[0];
    VF_STATE[1] += Ky[1];

    // Shrink the uncertainty using Joseph form  P_new = (I - KH)*P*(I - KH)^T + K*Q*K^T
    float KH[4];
    arm_matrix_instance_f32 KH_Mat;
    arm_mat_init_f32(&KH_Mat, 2, 2, KH);
    arm_mat_mult_f32(&K_Mat, &H_Mat, &KH_Mat);

    float I_mat[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    arm_matrix_instance_f32 I_Mat_inst;
    arm_mat_init_f32(&I_Mat_inst, 2, 2, I_mat);

    float IKH[4];
    arm_matrix_instance_f32 IKH_Mat;
    arm_mat_init_f32(&IKH_Mat, 2, 2, IKH);
    arm_mat_sub_f32(&I_Mat_inst, &KH_Mat, &IKH_Mat);

    float IKHT[4];
    arm_matrix_instance_f32 IKHT_Mat;
    arm_mat_init_f32(&IKHT_Mat, 2, 2, IKHT);
    arm_mat_trans_f32(&IKH_Mat, &IKHT_Mat);

    float IKHP[4];
    arm_matrix_instance_f32 IKHP_Mat;
    arm_mat_init_f32(&IKHP_Mat, 2, 2, IKHP);
    arm_mat_mult_f32(&IKH_Mat, &VF_UNCRT_M, &IKHP_Mat);

    float Term1[4];
    arm_matrix_instance_f32 Term1_Mat;
    arm_mat_init_f32(&Term1_Mat, 2, 2, Term1);
    arm_mat_mult_f32(&IKHP_Mat, &IKHT_Mat, &Term1_Mat);

    float KT[4];
    arm_matrix_instance_f32 KT_Mat;
    arm_mat_init_f32(&KT_Mat, 2, 2, KT);
    arm_mat_trans_f32(&K_Mat, &KT_Mat);

    float KQ[4];
    arm_matrix_instance_f32 KQ_Mat;
    arm_mat_init_f32(&KQ_Mat, 2, 2, KQ);
    arm_mat_mult_f32(&K_Mat, &Q_Mat, &KQ_Mat);

    float Term2[4];
    arm_matrix_instance_f32 Term2_Mat;
    arm_mat_init_f32(&Term2_Mat, 2, 2, Term2);
    arm_mat_mult_f32(&KQ_Mat, &KT_Mat, &Term2_Mat);

    arm_mat_add_f32(&Term1_Mat, &Term2_Mat, &VF_UNCRT_M);
}


inline float PressToAlt(float Pressure) {
	return 44330.0f * (1.0f - powf(Pressure / VF_GRND_PRESS, 0.1903f)) + VF_GRND_ALT;
}


void GetLinearAcceleration(float *q_est, float *macc, float *lacc) {
    float R[9];
    arm_matrix_instance_f32 R_Mat;
    arm_mat_init_f32(&R_Mat, 3, 3, R);
    QuatToMatrix(OF_STATE, R);

    arm_matrix_instance_f32 ABody_Mat, ANav_Mat;
    arm_mat_init_f32(&ABody_Mat, 3, 1, macc);
    arm_mat_init_f32(&ANav_Mat, 3, 1, lacc);

    arm_mat_mult_f32(&R_Mat, &ABody_Mat, &ANav_Mat);

    // Subtract gravity and flip Z so positive is up
    lacc[2] = -(lacc[2] + 1.0f);

    // Convert to m/s^2
    for (int i = 0; i < 3; i++) {
    	lacc[i] = lacc[i] * 9.80665f;
    }
}








// *** HORIZONTAL FILTER ***
// State vector: Pos North (m), Pos East (m), Vel North (m/s), Vel East (m/s)
float32_t HF_STATE[4] = {0.0f, 0.0f, 0.0f, 0.0f};
arm_matrix_instance_f32 HF_STATE_M;

// Uncertainty matrix
float32_t HF_UNCRT[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                          0.0f, 1.0f, 0.0f, 0.0f,
                          0.0f, 0.0f, 1.0f, 0.0f,
                          0.0f, 0.0f, 0.0f, 1.0f};
arm_matrix_instance_f32 HF_UNCRT_M;

// Noise constants
float HF_GPS_NMult_pos = 5.0f;
float HF_GPS_NMult_vel = 20.0f;
float HF_R_Accel = 0.01f;

// Home reference
float HF_LAT_HOME = 0.0f;
float HF_LON_HOME = 0.0f;


void InitialiseHorizontalFilter(float HomeLat, float HomeLon) {
    HF_LAT_HOME = HomeLat;
    HF_LON_HOME = HomeLon;

    for (int i = 0; i < 4; i++) {
        HF_STATE[i] = 0.0f;
    }

    arm_mat_init_f32(&HF_STATE_M, 4, 1, HF_STATE);
    arm_mat_init_f32(&HF_UNCRT_M, 4, 4, HF_UNCRT);
}


void LatLonToMeters(float lat, float lon, float *meters_north, float *meters_east) {
    float R = 6371000.0f;   // Earth radius in meters

    float phi1 = DEG2RAD(HF_LAT_HOME);
    float phi2 = DEG2RAD(lat);
    float dphi = DEG2RAD(lat - HF_LAT_HOME);
    float dlambda = DEG2RAD(lon - HF_LON_HOME);

    // Equirectangular approximation
    *meters_east = R * dlambda * arm_cos_f32((phi1 + phi2) / 2.0f);
    *meters_north = R * dphi;
}


void HF_NewAccData(SensorData *data, float dt) {
    float a_comp[3] = {data->data.tsvec3.X, data->data.tsvec3.Y, data->data.tsvec3.Z};
    float a_nav[3];
    GetLinearAcceleration(OF_STATE, a_comp, a_nav);


    // State prediction (manual to save matrix operations)
    float dt2 = dt * dt;

    float new_n_pos = HF_STATE[0] + (dt * HF_STATE[2]) + (0.5f * dt2 * a_nav[0]);
    float new_e_pos = HF_STATE[1] + (dt * HF_STATE[3]) + (0.5f * dt2 * a_nav[1]);
    float new_n_vel = HF_STATE[2] + (dt * a_nav[0]);
    float new_e_vel = HF_STATE[3] + (dt * a_nav[1]);

    HF_STATE[0] = new_n_pos;
    HF_STATE[1] = new_e_pos;
    HF_STATE[2] = new_n_vel;
    HF_STATE[3] = new_e_vel;

    // Dynamically calculate horizontal kinematic process noise
    float F[16] = {1.0f, 0.0f, dt,   0.0f,
                   0.0f, 1.0f, 0.0f, dt,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   0.0f, 0.0f, 0.0f, 1.0f};
    arm_matrix_instance_f32 F_Mat;
    arm_mat_init_f32(&F_Mat, 4, 4, F);

    float sigma_horiz = 5.0f;   // High variance to absorb IMU errors
    float qh_p = sigma_horiz * (dt2 * dt / 3.0f) + (HF_R_Accel * dt);
    float qh_c = sigma_horiz * (dt2 / 2.0f);
    float qh_v = sigma_horiz * dt;

    float Q[16] = {qh_p, 0.0f, qh_c, 0.0f,
                   0.0f, qh_p, 0.0f, qh_c,
                   qh_c, 0.0f, qh_v, 0.0f,
                   0.0f, qh_c, 0.0f, qh_v};
    arm_matrix_instance_f32 Q_Mat;
    arm_mat_init_f32(&Q_Mat, 4, 4, Q);

    arm_matrix_instance_f32 P_Mat;
    arm_mat_init_f32(&P_Mat, 4, 4, HF_UNCRT);

    float FT[16], FP[16], FPFT[16];
    arm_matrix_instance_f32 FT_Mat, FP_Mat, FPFT_Mat;
    arm_mat_init_f32(&FT_Mat, 4, 4, FT);
    arm_mat_init_f32(&FP_Mat, 4, 4, FP);
    arm_mat_init_f32(&FPFT_Mat, 4, 4, FPFT);

    arm_mat_trans_f32(&F_Mat, &FT_Mat);
    arm_mat_mult_f32(&F_Mat, &P_Mat, &FP_Mat);
    arm_mat_mult_f32(&FP_Mat, &FT_Mat, &FPFT_Mat);

    arm_mat_add_f32(&FPFT_Mat, &Q_Mat, &HF_UNCRT_M);
}


void HF_NewGPSData(SensorData *data) {
    if (data->data.tsgps.FixType == 0) { return; }

    float mN, mE;
    LatLonToMeters(data->data.tsgps.Latitude, data->data.tsgps.Longitude, &mN, &mE);

    // Dynamic variance matrix
    float q_pos = data->data.tsgps.HorzAccuracy * data->data.tsgps.HorzAccuracy * HF_GPS_NMult_pos;
    float q_vel = data->data.tsgps.SpeedAccuracy * data->data.tsgps.SpeedAccuracy * HF_GPS_NMult_vel;

    float R[16] = {q_pos, 0.0f, 0.0f, 0.0f,
                   0.0f, q_pos, 0.0f, 0.0f,
                   0.0f, 0.0f, q_vel, 0.0f,
                   0.0f, 0.0f, 0.0f, q_vel};
    arm_matrix_instance_f32 R_Mat;
    arm_mat_init_f32(&R_Mat, 4, 4, R);

    // Calculate error and kalman gain
    // S = P + R
    float S[16];
    arm_matrix_instance_f32 S_Mat;
    arm_mat_init_f32(&S_Mat, 4, 4, S);
    arm_mat_add_f32(&HF_UNCRT_M, &R_Mat, &S_Mat);

    // K = P * S^-1
    float SI[16];
    arm_matrix_instance_f32 SI_Mat;
    arm_mat_init_f32(&SI_Mat, 4, 4, SI);
    arm_mat_inverse_f32(&S_Mat, &SI_Mat);

    float K[16];
    arm_matrix_instance_f32 K_Mat;
    arm_mat_init_f32(&K_Mat, 4, 4, K);
    arm_mat_mult_f32(&HF_UNCRT_M, &SI_Mat, &K_Mat);

    // Calculate error
    float y[4] = {mN - HF_STATE[0], mE - HF_STATE[1], data->data.tsgps.VelNorth - HF_STATE[2], data->data.tsgps.VelEast - HF_STATE[3]};
    arm_matrix_instance_f32 Y_Mat;
    arm_mat_init_f32(&Y_Mat, 4, 1, y);

    // Correct state X = X + Ky
    float Ky[4];
    arm_matrix_instance_f32 Ky_Mat;
    arm_mat_init_f32(&Ky_Mat, 4, 1, Ky);
    arm_mat_mult_f32(&K_Mat, &Y_Mat, &Ky_Mat);

    for (int i = 0; i < 4; i++) {
        HF_STATE[i] += Ky[i];
    }


    // Shrink uncertainty using Joseph form  P_new = (I - K)*P*(I - K)^T + K*R*K^T
    float I_mat[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                       0.0f, 1.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 1.0f, 0.0f,
                       0.0f, 0.0f, 0.0f, 1.0f};
    arm_matrix_instance_f32 I_Mat;
    arm_mat_init_f32(&I_Mat, 4, 4, I_mat);

    float IK[16], IKT[16], IKP[16], T1[16];
    arm_matrix_instance_f32 IK_Mat, IKT_Mat, IKP_Mat, T1_Mat;
    arm_mat_init_f32(&IK_Mat, 4, 4, IK);
    arm_mat_init_f32(&IKT_Mat, 4, 4, IKT);
    arm_mat_init_f32(&IKP_Mat, 4, 4, IKP);
    arm_mat_init_f32(&T1_Mat, 4, 4, T1);

    arm_mat_sub_f32(&I_Mat, &K_Mat, &IK_Mat);
    arm_mat_trans_f32(&IK_Mat, &IKT_Mat);
    arm_mat_mult_f32(&IK_Mat, &HF_UNCRT_M, &IKP_Mat);
    arm_mat_mult_f32(&IKP_Mat, &IKT_Mat, &T1_Mat);

    float KT[16], KR[16], T2[16];
    arm_matrix_instance_f32 KT_Mat, KR_Mat, T2_Mat;
    arm_mat_init_f32(&KT_Mat, 4, 4, KT);
    arm_mat_init_f32(&KR_Mat, 4, 4, KR);
    arm_mat_init_f32(&T2_Mat, 4, 4, T2);

    arm_mat_trans_f32(&K_Mat, &KT_Mat);
    arm_mat_mult_f32(&K_Mat, &R_Mat, &KR_Mat);
    arm_mat_mult_f32(&KR_Mat, &KT_Mat, &T2_Mat);

    arm_mat_add_f32(&T1_Mat, &T2_Mat, &HF_UNCRT_M);
}











void Fusion_NewLRAccData(SensorData *data) {
	// Determine if the saturation threshold has been reached in any axis
	if (fabsf(data->data.tsvec3.X) > LRACC_SAT_THRESH || fabsf(data->data.tsvec3.Y) > LRACC_SAT_THRESH || fabsf(data->data.tsvec3.Z) > LRACC_SAT_THRESH) {
		lracc_saturated = true;
		return;   // Discard and let the high range accelerometer drive the filter
	}

	lracc_saturated = false;

	// Don't need dt for orientation filter
	OF_NewAccData(data);

	float dt = data->data.tsvec3.Timestamp - acc_tlast;

	// Only update movement filters if dt is valid
	if (acc_tlast > 0.0f && dt > 0.0f) {
		VF_NewAccData(data, dt);
		HF_NewAccData(data, dt);
	}
	acc_tlast = data->data.tsvec3.Timestamp;
}

void Fusion_NewGyrData(SensorData *data) {
	float dt = data->data.tsvec3.Timestamp - gyr_tlast;

	// Only update filter if dt is valid
	if (gyr_tlast > 0.0f && dt > 0.0f) {
		OF_NewGyrData(data, dt);
	}

	gyr_tlast = data->data.tsvec3.Timestamp;
}

void Fusion_NewHRAccData(SensorData *data) {
	// Ignore high range readings if the low range sensor is not saturated
	if (!lracc_saturated) { return; }

	float dt = data->data.tsvec3.Timestamp - acc_tlast;

	// Only update movement filters if dt is valid
	if (acc_tlast > 0.0f && dt > 0.0f) {
		VF_NewAccData(data, dt);
		HF_NewAccData(data, dt);
	}
	acc_tlast = data->data.tsvec3.Timestamp;
}

void Fusion_NewPressData(SensorData *data) {
	VF_NewPressData(data);
}

void Fusion_NewMagData(SensorData *data) {
	OF_NewMagData(data);
}

void Fusion_NewGPSData(SensorData *data) {
	VF_NewGPSData(data);
	HF_NewGPSData(data);
}


















