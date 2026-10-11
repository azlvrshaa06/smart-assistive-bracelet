#ifndef MAX30105_AVR_H
#define MAX30105_AVR_H


#include <stdint.h>
#include <stdbool.h>


/* Configure for your MCU */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif


/* FIFO storage */
#define STORAGE_SIZE 32


/* I2C address */
#define MAX30105_ADDRESS 0x57


/* Registers (subset used in medium driver) */
#define MAX30105_INTSTAT1        0x00
#define MAX30105_INTSTAT2        0x01
#define MAX30105_INTENABLE1      0x02
#define MAX30105_INTENABLE2      0x03
#define MAX30105_FIFOWRITEPTR    0x04
#define MAX30105_FIFOOVERFLOW    0x05
#define MAX30105_FIFOREADPTR     0x06
#define MAX30105_FIFODATA        0x07
#define MAX30105_FIFOCONFIG      0x08
#define MAX30105_MODECONFIG      0x09
#define MAX30105_PARTICLECONFIG  0x0A
#define MAX30105_LED1_PULSEAMP   0x0C
#define MAX30105_LED2_PULSEAMP   0x0D
#define MAX30105_LED3_PULSEAMP   0x0E
#define MAX30105_LED_PROX_AMP    0x10
#define MAX30105_MULTILEDCONFIG1 0x11
#define MAX30105_MULTILEDCONFIG2 0x12
#define MAX30105_DIETEMPINT      0x1F
#define MAX30105_DIETEMPFRAC     0x20
#define MAX30105_DIETEMPCONFIG   0x21
#define MAX30105_PROXINTTHRESH   0x30
#define MAX30105_REVISIONID      0xFE
#define MAX30105_PARTID          0xFF


/* Useful masks/values */
#define MAX30105_MODE_REDONLY    0x02
#define MAX30105_MODE_REDIRONLY  0x03
#define MAX30105_MODE_MULTILED   0x07


#define MAX30105_ADCRANGE_2048   0x00
#define MAX30105_ADCRANGE_4096   0x20
#define MAX30105_ADCRANGE_8192   0x40
#define MAX30105_ADCRANGE_16384  0x60


#define MAX30105_SAMPLERATE_50   0x00
#define MAX30105_SAMPLERATE_100  0x04
#define MAX30105_SAMPLERATE_200  0x08
#define MAX30105_SAMPLERATE_400  0x0C
#define MAX30105_SAMPLERATE_800  0x10
#define MAX30105_SAMPLERATE_1000 0x14
#define MAX30105_SAMPLERATE_1600 0x18
#define MAX30105_SAMPLERATE_3200 0x1C


#define MAX30105_PULSEWIDTH_69   0x00
#define MAX30105_PULSEWIDTH_118  0x01
#define MAX30105_PULSEWIDTH_215  0x02
#define MAX30105_PULSEWIDTH_411  0x03


/* Multi-LED slot values */
#define SLOT_NONE       0x00
#define SLOT_RED_LED    0x01
#define SLOT_IR_LED     0x02
#define SLOT_GREEN_LED  0x03


/* device struct */
typedef struct {
uint8_t activeLEDs;      /* number of LEDs in use (1..3) */
uint8_t revisionID;
uint32_t red[STORAGE_SIZE];
uint32_t ir[STORAGE_SIZE];
uint32_t green[STORAGE_SIZE];
uint8_t head;            /* FIFO head index */
uint8_t tail;            /* FIFO tail index */
} MAX30105_t;


/* High-level API */
void max30105_twi_init(uint32_t scl_freq_hz); /* initialize AVR TWI - call before begin */
bool max30105_begin(MAX30105_t *dev);
void max30105_setup(MAX30105_t *dev,
uint8_t powerLevel,
uint8_t sampleAverage,
uint8_t ledMode,
int sampleRate,
int pulseWidth,
int adcRange);


/* Reading / data */
uint8_t max30105_available(MAX30105_t *dev);
uint32_t max30105_getRed(MAX30105_t *dev);
uint32_t max30105_getIR(MAX30105_t *dev);
uint32_t max30105_getGreen(MAX30105_t *dev);
uint32_t max30105_getFIFORed(MAX30105_t *dev);
uint32_t max30105_getFIFOIR(MAX30105_t *dev);
uint32_t max30105_getFIFOGreen(MAX30105_t *dev);
void max30105_nextSample(MAX30105_t *dev);
uint16_t max30105_check(MAX30105_t *dev); /* read FIFO into buffers, return number of samples read */


/* Configuration helpers */
void max30105_setLEDMode(MAX30105_t *dev, uint8_t mode);
void max30105_setADCRange(MAX30105_t *dev, uint8_t adcRange);
void max30105_setSampleRate(MAX30105_t *dev, uint8_t sampleRate);
void max30105_setPulseWidth(MAX30105_t *dev, uint8_t pulseWidth);
void max30105_setPulseAmplitudeRed(MAX30105_t *dev, uint8_t amp);
void max30105_setPulseAmplitudeIR(MAX30105_t *dev, uint8_t amp);
void max30105_setPulseAmplitudeGreen(MAX30105_t *dev, uint8_t amp);
void max30105_enableSlot(MAX30105_t *dev, uint8_t slotNumber, uint8_t device);
void max30105_clearFIFO(MAX30105_t *dev);


/* Low-level register helpers */
int8_t max30105_readRegister8(uint8_t reg);
void max30105_writeRegister8(uint8_t reg, uint8_t val);
void max30105_bitMask(uint8_t reg, uint8_t mask, uint8_t thing);


#endif /* MAX30105_AVR_H */
