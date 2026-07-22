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

	float VelNorth;
	float VelEast;
	float VelDown;
	float GroundSpeed;

	float Heading;

	float HorzAccuracy;
	float VertAccuracy;
	float SpeedAccuracy;

	uint8_t Satellites;
	uint8_t FixType;
} TS_GPS;


typedef enum {
	SENSOR_DATA_LRACC,
	SENSOR_DATA_GYR,
	SENSOR_DATA_HRACC,
	SENSOR_DATA_MAG,
	SENSOR_DATA_PRSTMP,
	SENSOR_DATA_GPS
} SensorDataType;


typedef struct {
	SensorDataType type;
	union {
		TS_Vec3 tsvec3;
		TS_PressTemp tsprstmp;
		TS_GPS tsgps;
	} data;
} SensorData;


#endif /* DATATYPES_H_ */



