#include "max30105_avr.h"
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include "i2c.h"


/* ---------- Low-level register access ---------- */

int8_t max30105_readRegister8(uint8_t reg)
{
    uint8_t result;

    /* write register address */
    i2c_start(MAX30105_ADDRESS << 1);   // write
    i2c_write(reg);
    
    /* repeated start for read */
    i2c_start((MAX30105_ADDRESS << 1) | 1); // read
    result = i2c_read_nack();

    i2c_stop();
    return (int8_t)result;
}

void max30105_writeRegister8(uint8_t reg, uint8_t val)
{
    i2c_start(MAX30105_ADDRESS << 1);   // write
    i2c_write(reg);
    i2c_write(val);
    i2c_stop();
}

void max30105_bitMask(uint8_t reg, uint8_t mask, uint8_t thing) {
    uint8_t originalContents = max30105_readRegister8(reg);
    originalContents &= mask;
    max30105_writeRegister8(reg, originalContents | thing);
}

/* ---------- Device management ---------- */

bool max30105_begin(MAX30105_t *dev) {
    if (!dev) return false;
    /* Initialize struct */
    dev->activeLEDs = 1;
    dev->head = dev->tail = 0;
    dev->revisionID = max30105_readRegister8(MAX30105_REVISIONID);
    uint8_t pid = max30105_readRegister8(MAX30105_PARTID);
    /* expected 0x15 for MAX30105/MAX30102; accept it as simple check */
    if (pid != 0x15) {
        /* still proceed but caller may detect */
        return false;
    }
    return true;
}

void max30105_clearFIFO(MAX30105_t *dev) {
    (void)dev;
    max30105_writeRegister8(MAX30105_FIFOWRITEPTR, 0);
    max30105_writeRegister8(MAX30105_FIFOOVERFLOW, 0);
    max30105_writeRegister8(MAX30105_FIFOREADPTR, 0);
}

/* Configuration helpers (wrap bitMask or writes) */
void max30105_setLEDMode(MAX30105_t *dev, uint8_t mode) {
    (void)dev;
    max30105_bitMask(MAX30105_MODECONFIG, 0xF8, mode);
    /* set activeLEDs from mode */
    if (mode == MAX30105_MODE_MULTILED) dev->activeLEDs = 3;
    else if (mode == MAX30105_MODE_REDIRONLY) dev->activeLEDs = 2;
    else dev->activeLEDs = 1;
}

void max30105_setADCRange(MAX30105_t *dev, uint8_t adcRange) {
    (void)dev;
    max30105_bitMask(MAX30105_PARTICLECONFIG, 0x9F, adcRange);
}

void max30105_setSampleRate(MAX30105_t *dev, uint8_t sampleRate) {
    (void)dev;
    max30105_bitMask(MAX30105_PARTICLECONFIG, 0xE3, sampleRate);
}

void max30105_setPulseWidth(MAX30105_t *dev, uint8_t pulseWidth) {
    (void)dev;
    max30105_bitMask(MAX30105_PARTICLECONFIG, 0xFC, pulseWidth);
}

void max30105_setPulseAmplitudeRed(MAX30105_t *dev, uint8_t amp) {
    (void)dev;
    max30105_writeRegister8(MAX30105_LED1_PULSEAMP, amp);
}
void max30105_setPulseAmplitudeIR(MAX30105_t *dev, uint8_t amp) {
    (void)dev;
    max30105_writeRegister8(MAX30105_LED2_PULSEAMP, amp);
}
void max30105_setPulseAmplitudeGreen(MAX30105_t *dev, uint8_t amp) {
    (void)dev;
    max30105_writeRegister8(MAX30105_LED3_PULSEAMP, amp);
}

/* Multi-slot enable */
void max30105_enableSlot(MAX30105_t *dev, uint8_t slotNumber, uint8_t device) {
    (void)dev;
    /* slot1 -> MULTILEDCONFIG1 bits [2:0]
       slot2 -> MULTILEDCONFIG1 bits [6:4]
       slot3 -> MULTILEDCONFIG2 bits [2:0]
       slot4 -> MULTILEDCONFIG2 bits [6:4] */
    uint8_t tmp;
    switch (slotNumber) {
        case 1:
            max30105_bitMask(MAX30105_MULTILEDCONFIG1, 0xF8, device);
            break;
        case 2:
            tmp = device << 4;
            max30105_bitMask(MAX30105_MULTILEDCONFIG1, 0x8F, tmp);
            break;
        case 3:
            max30105_bitMask(MAX30105_MULTILEDCONFIG2, 0xF8, device);
            break;
        case 4:
            tmp = device << 4;
            max30105_bitMask(MAX30105_MULTILEDCONFIG2, 0x8F, tmp);
            break;
        default:
            break;
    }
}

/* Utility: get FIFO pointers */
static uint8_t getWritePointer(void) { return max30105_readRegister8(MAX30105_FIFOWRITEPTR); }
static uint8_t getReadPointer(void)  { return max30105_readRegister8(MAX30105_FIFOREADPTR); }

/* ---------- Data collection ---------- */

uint8_t max30105_available(MAX30105_t *dev) {
    int8_t numberOfSamples = dev->head - dev->tail;
    if (numberOfSamples < 0) numberOfSamples += STORAGE_SIZE;
    return (uint8_t)numberOfSamples;
}

uint32_t max30105_getRed(MAX30105_t *dev) {
    /* return most recent (head) */
    return dev->red[dev->head];
}
uint32_t max30105_getIR(MAX30105_t *dev) {
    return dev->ir[dev->head];
}
uint32_t max30105_getGreen(MAX30105_t *dev) {
    return dev->green[dev->head];
}
uint32_t max30105_getFIFORed(MAX30105_t *dev) { return dev->red[dev->tail]; }
uint32_t max30105_getFIFOIR(MAX30105_t *dev)  { return dev->ir[dev->tail]; }
uint32_t max30105_getFIFOGreen(MAX30105_t *dev){ return dev->green[dev->tail]; }

void max30105_nextSample(MAX30105_t *dev) {
    if (max30105_available(dev)) {
        dev->tail++;
        dev->tail %= STORAGE_SIZE;
    }
}

/* check() — read FIFO and store into circular buffers.
   Returns number of samples read.
*/
uint16_t max30105_check(MAX30105_t *dev)
{
    uint8_t readPointer = getReadPointer();
    uint8_t writePointer = getWritePointer();

    int numberOfSamples = writePointer - readPointer;
    if (numberOfSamples < 0)
        numberOfSamples += 32;

    if (numberOfSamples == 0)
        return 0;

    int bytesToRead = numberOfSamples * dev->activeLEDs * 3;

    // Set FIFO pointer
    i2c_start((MAX30105_ADDRESS << 1) | 0);
    i2c_write(MAX30105_FIFODATA);
    

    // Restart for read
    i2c_start((MAX30105_ADDRESS << 1) | 1);

    for (int s = 0; s < numberOfSamples; s++)
    {
        uint32_t red, ir, green;

        // ----- READ RED -----
        uint8_t b2 = i2c_read_ack();
        uint8_t b1 = i2c_read_ack();
        uint8_t b0 = (dev->activeLEDs == 1 && s == numberOfSamples - 1) ? i2c_read_nack() : i2c_read_ack();

        red = ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;
        red &= 0x3FFFF;

        dev->red[dev->head] = red;

        // ----- READ IR -----
        if (dev->activeLEDs > 1)
        {
            b2 = i2c_read_ack();
            b1 = i2c_read_ack();
            b0 = (dev->activeLEDs == 2 && s == numberOfSamples - 1) ? i2c_read_nack() : i2c_read_ack();

            ir = ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;
            ir &= 0x3FFFF;

            dev->ir[dev->head] = ir;
        }

        // ----- GREEN -----
        if (dev->activeLEDs > 2)
        {
            b2 = i2c_read_ack();
            b1 = i2c_read_ack();
            b0 = i2c_read_nack(); // last byte

            green = ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;
            green &= 0x3FFFF;

            dev->green[dev->head] = green;
        }

        dev->head = (dev->head + 1) % STORAGE_SIZE;
    }

    i2c_stop();

    // FIX PALING PENTING:
    max30105_writeRegister8(MAX30105_FIFOREADPTR, writePointer);

    return numberOfSamples;
}


/* ----- High-level setup wrapper (simplified) ----- */
void max30105_setup(MAX30105_t *dev,
                    uint8_t powerLevel,
                    uint8_t sampleAverage,
                    uint8_t ledMode,
                    int sampleRate,
                    int pulseWidth,
                    int adcRange) {
    /* Reset recommended by full library — omitted here for brevity (could write reset bit) */

    /* FIFO Average */
    if (sampleAverage == 1) max30105_bitMask(MAX30105_FIFOCONFIG, 0x1F, 0x00);
    else if (sampleAverage == 2) max30105_bitMask(MAX30105_FIFOCONFIG, 0x1F, 0x20);
    else if (sampleAverage == 4) max30105_bitMask(MAX30105_FIFOCONFIG, 0x1F, 0x40);
    else if (sampleAverage == 8) max30105_bitMask(MAX30105_FIFOCONFIG, 0x1F, 0x60);
    else if (sampleAverage == 16) max30105_bitMask(MAX30105_FIFOCONFIG, 0x1F, 0x80);
    else if (sampleAverage == 32) max30105_bitMask(MAX30105_FIFOCONFIG, 0x1F, 0xA0);
    else max30105_bitMask(MAX30105_FIFOCONFIG, 0x1F, 0x40);

    /* Rollover on */
    max30105_bitMask(MAX30105_FIFOCONFIG, 0xEF, 0x10);

    /* Mode */
    if (ledMode == 3) max30105_setLEDMode(dev, MAX30105_MODE_MULTILED);
    else if (ledMode == 2) max30105_setLEDMode(dev, MAX30105_MODE_REDIRONLY);
    else max30105_setLEDMode(dev, MAX30105_MODE_REDONLY);

    /* ADC range */
    if (adcRange < 4096) max30105_setADCRange(dev, MAX30105_ADCRANGE_2048);
    else if (adcRange < 8192) max30105_setADCRange(dev, MAX30105_ADCRANGE_4096);
    else if (adcRange < 16384) max30105_setADCRange(dev, MAX30105_ADCRANGE_8192);
    else max30105_setADCRange(dev, MAX30105_ADCRANGE_16384);

    /* Sample rate selection */
    if (sampleRate < 100) max30105_setSampleRate(dev, MAX30105_SAMPLERATE_50);
    else if (sampleRate < 200) max30105_setSampleRate(dev, MAX30105_SAMPLERATE_100);
    else if (sampleRate < 400) max30105_setSampleRate(dev, MAX30105_SAMPLERATE_200);
    else if (sampleRate < 800) max30105_setSampleRate(dev, MAX30105_SAMPLERATE_400);
    else if (sampleRate < 1000) max30105_setSampleRate(dev, MAX30105_SAMPLERATE_800);
    else if (sampleRate < 1600) max30105_setSampleRate(dev, MAX30105_SAMPLERATE_1000);
    else if (sampleRate < 3200) max30105_setSampleRate(dev, MAX30105_SAMPLERATE_1600);
    else max30105_setSampleRate(dev, MAX30105_SAMPLERATE_3200);

    /* Pulse width */
    if (pulseWidth < 118) max30105_setPulseWidth(dev, MAX30105_PULSEWIDTH_69);
    else if (pulseWidth < 215) max30105_setPulseWidth(dev, MAX30105_PULSEWIDTH_118);
    else if (pulseWidth < 411) max30105_setPulseWidth(dev, MAX30105_PULSEWIDTH_215);
    else max30105_setPulseWidth(dev, MAX30105_PULSEWIDTH_411);

    /* LED amplitudes */
    max30105_setPulseAmplitudeRed(dev, powerLevel);
    max30105_setPulseAmplitudeIR(dev, powerLevel);
    max30105_setPulseAmplitudeGreen(dev, powerLevel);
    max30105_writeRegister8(MAX30105_LED_PROX_AMP, powerLevel);

    /* Slots (enable according to ledMode) */
    max30105_enableSlot(dev, 1, SLOT_RED_LED);
    if (dev->activeLEDs > 1) max30105_enableSlot(dev, 2, SLOT_IR_LED);
    if (dev->activeLEDs > 2) max30105_enableSlot(dev, 3, SLOT_GREEN_LED);

    max30105_clearFIFO(dev);
}
