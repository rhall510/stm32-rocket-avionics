#ifndef DATATYPES_H_
#define DATATYPES_H_

struct Vector3 {
	float X;
	float Y;
	float Z;
};


struct TS_Vec3 {
	float Timestamp;
	float X;
	float Y;
	float Z;
};


struct TS_PressTemp {
	float Timestamp;
	float Press;
	float Temp;
};


struct TS_GPS {
	float Timestamp;
	float Latitude;
	float Longitude;
	float Altitude;
	float Speed;
	uint8_t Satellites;
	bool HasFix;
};


#endif /* DATATYPES_H_ */
