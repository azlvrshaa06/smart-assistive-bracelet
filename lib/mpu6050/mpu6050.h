#ifndef MPU6050_H
#define MPU6050_H

#include <stdlib.h>
#include <stdint.h>
#include "i2c.h"

/* ===== Custom Types ===== */
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* ===== Register Addresses ===== */
#define SMPLRT_DIV      0x19
#define CONFIG          0x1A
#define GYRO_CONFIG     0x1B
#define ACCEL_CONFIG    0x1C

/* Accelerometer data registers */
#define ACCEL_XOUT_H    0x3B
#define ACCEL_YOUT_H    0x3D
#define ACCEL_ZOUT_H    0x3F

/* Gyroscope data registers */
#define GYRO_XOUT_H     0x43
#define GYRO_YOUT_H     0x45
#define GYRO_ZOUT_H     0x47

#define WHO_AM_I        0x75

/* Power management */
#define PWR_MGMT_1      0x6B
#define PWR_MGMT_2      0x6C

/* Device address (AD0 grounded → 0x68) */
#define MPU6050_ADDR    0x68

/* ===== Function Prototypes ===== */

/* Internal helpers */
void _MPU6050_writeToReg(uint8_t reg, uint8_t value);
s32  _MPU6050_getRegValue(u8 reg, u8 len);
float _MPU6050_countAccelAngle(float a, float b, float c);
void _MPU6050_moveToReg(uint8_t reg);
/* Public API */
void MPU6050_Init(void);

/* Raw data readers */
void MPU6050_getAccelRaw(s16 out[3]);
void MPU6050_getGyroRaw(s16 out[3]);

/* Angle conversions */
float* MPU6050_getAccelAngles(void);
float* MPU6050_getGyroAngles(float* prev, s32 delta);

#endif /* MPU6050_H */