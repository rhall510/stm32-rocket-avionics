#ifndef DATATYPES_H_
#define DATATYPES_H_

typedef struct{
	float X;
	float Y;
	float Z;
} Vec3;


typedef struct {
	float Timestamp;
	float X;
	float Y;
	float Z;
} TS_Vec3;


typedef struct {
	float Timestamp;
	float Press;
	float Temp;
} TS_PressTemp;


typedef struct {
	float Timestamp;
	float Latitude;
	float Longitude;
	float Altitude;
	float Speed;
	uint8_t Satellites;
	bool HasFix;
} TS_GPS;


#endif /* DATATYPES_H_ */
