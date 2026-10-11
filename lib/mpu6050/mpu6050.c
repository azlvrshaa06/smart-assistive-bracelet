#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <util/delay.h>
#include "mpu6050.h"
#include "i2c.h"



/* ========== Helper untuk I2C & Register ========== */

/* Move pointer to register with ACK checking and retries */
void _MPU6050_moveToReg(uint8_t reg)
{
    const uint8_t max_retries = 3;

    for (uint8_t attempt = 0; attempt < max_retries; attempt++) {

        /* START + SLA+W */
        i2c_start((MPU6050_ADDR << 1) | 0);

        /* Send register address */
        i2c_write(reg);

        /* STOP */
        i2c_stop();

        /* success */
        return;
    }

    /* If still failing, recover bus */
    i2c_bus_recover();
    _delay_ms(5);
}

void _MPU6050_writeToReg(uint8_t reg, uint8_t value) {
    i2c_start((MPU6050_ADDR << 1) | 0);
    i2c_write(reg);
    i2c_write(value);
    i2c_stop();
}

s32 _MPU6050_getRegValue(u8 reg, u8 len) {
    _MPU6050_moveToReg(reg);
    i2c_start((MPU6050_ADDR << 1) | 1);

    if (len == 1) {
        uint8_t b = i2c_read_nack();
        i2c_stop();
        return (s32)b;
    } else if (len == 2) {
        uint8_t msb = i2c_read_ack();
        uint8_t lsb = i2c_read_nack();
        i2c_stop();
        return (s32)((msb << 8) | lsb);
    }
    i2c_stop();
    return INT32_MIN;
}

/* ========== Inisialisasi Sensor ========== */
void MPU6050_Init(void) {
    _MPU6050_writeToReg(PWR_MGMT_1, 0x01); // Bangun sensor, pilih X-gyro clock
    _delay_ms(5);
    _MPU6050_writeToReg(PWR_MGMT_2, 0x00); // Nyalakan semua sensor
    _delay_ms(5);

    _MPU6050_writeToReg(SMPLRT_DIV, 0x07); // Sample rate divider
    _MPU6050_writeToReg(CONFIG, 0x00);     // No filter
    _MPU6050_writeToReg(GYRO_CONFIG, 0x18);// Gyro ±2000 °/s
    _MPU6050_writeToReg(ACCEL_CONFIG, 0x00);// Accel ±2g
}

/* ========== Baca Data Mentah ========== */
void MPU6050_getAccelRaw(s16 out[3]) {
    out[0] = (s16)_MPU6050_getRegValue(ACCEL_XOUT_H, 2);
    out[1] = (s16)_MPU6050_getRegValue(ACCEL_YOUT_H, 2);
    out[2] = (s16)_MPU6050_getRegValue(ACCEL_ZOUT_H, 2);
}

void MPU6050_getGyroRaw(s16 out[3]) {
    out[0] = (s16)_MPU6050_getRegValue(GYRO_XOUT_H, 2);
    out[1] = (s16)_MPU6050_getRegValue(GYRO_YOUT_H, 2);
    out[2] = (s16)_MPU6050_getRegValue(GYRO_ZOUT_H, 2);
}

/* ========== Konversi Data ========== */

float _MPU6050_countAccelAngle(float a, float b, float c) {
    return 57.295f * atan(a / sqrtf(b*b + c*c));
}

float* MPU6050_getAccelAngles(void) {
    s16 raw[3];
    MPU6050_getAccelRaw(raw);
    float *angles = (float*)malloc(3 * sizeof(float));
    angles[0] = _MPU6050_countAccelAngle(raw[0], raw[1], raw[2]);
    angles[1] = _MPU6050_countAccelAngle(raw[1], raw[0], raw[2]);
    angles[2] = _MPU6050_countAccelAngle(raw[2], raw[0], raw[1]);
    return angles;
}

float* MPU6050_getGyroAngles(float* prev, s32 delta) {
    s16 raw[3];
    MPU6050_getGyroRaw(raw);
    float *angles = (float*)malloc(3 * sizeof(float));
    for (u8 i = 0; i < 3; i++) {
        angles[i] = prev[i] + raw[i] * delta * 0.001f;
    }
    return angles;
}