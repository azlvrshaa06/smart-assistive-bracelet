# Assistive Smart Bracelet

A wearable bracelet built on a microcontroller that monitors heart rate and detects falls on its own, then alerts the user through an OLED display, a buzzer, and an LED. The firmware is written at the register level for the Arduino Nano (ATmega328P) without high-level Arduino libraries such as `Wire`, and is built with PlatformIO.

> This is an academic prototype developed for the Microcontroller course at Binus University. It is not a medical device and must not be used as a basis for diagnosis.

---

## Project Team

| Name | Student ID | Role |
|------|------------|------|
| Azalia Varsha Jeslyn Putri Legowo | 2802519902 | MPU6050 sensor setup and integration; testing and troubleshooting; PCB development; final assembly (wiring and casing installation) |
| Julius Lim Khan | 2802527381 | MAX30102 sensor setup and integration; testing and troubleshooting; casing design; PCB schematic; final assembly (wiring and casing installation) |

---

## Background and Objectives

Older adults are more likely to experience abnormal heart rates and falls, and both conditions are often noticed too late when no one is monitoring the person directly. Many commercial wearables, such as the Samsung Galaxy Fit 3 and the Xiaomi Watch 2, rely on a smartphone or an external network to deliver alerts.

This project takes a different approach. It is a standalone embedded device that needs no internet connection, mobile application, or smartphone, and it combines heart rate monitoring and fall detection in a single wearable.

The objectives are as follows.

1. Measure the user's heart rate in real time using a wearable microcontroller-based device.
2. Detect falls using a motion sensor.
3. Integrate the sensors, the microcontroller, and the output devices so that the system gives visual and audible alerts when an abnormal condition occurs.
4. Realize a working hardware and firmware prototype that operates independently.

---

## Key Features

- Real-time heart rate measurement from a photoplethysmography (PPG) sensor, shown as Instant BPM and Stable BPM.
- Automatic alarm when the stable heart rate is outside the range of 60 to 100 bpm.
- Fall detection with the MPU6050 accelerometer, using a state machine that looks for free-fall followed by an impact.
- Three distinct alarm patterns that indicate the type and urgency of the event.
- Live status shown on a 128x64 OLED display.
- Fully standalone operation on battery power.

---

## System Overview

### Hardware Components

| Component | Function | I2C Address / Pin |
|-----------|----------|-------------------|
| Arduino Nano (ATmega328P, 16 MHz) | Main controller | A4 (SDA), A5 (SCL) |
| MAX30102 | Heart rate sensor (PPG, Red and IR LEDs) | `0x57` |
| MPU6050 (GY-521 module) | 3-axis accelerometer and gyroscope | `0x68` |
| OLED 128x64 (SSD1306) | Status display | `0x3C` |
| Active buzzer | Audible alarm | PD5 (D5) |
| Red LED with 1 kΩ resistor | Visual alarm | PD7 (D7) |
| 2 x 5 kΩ resistors | Pull-up for the SDA and SCL lines | - |
| MT3608 step-up converter | Raises the battery voltage to 5 V | - |
| 3.7 V Li-ion battery (1000 mAh) | Power source | - |

The heart rate sensor, the MPU6050, and the OLED share a single I2C bus on pins A4 and A5.

### Block Diagram

The system consists of four blocks: input, processing unit, output, and power source.

```
   INPUT                 PROCESSING UNIT                 OUTPUT
┌──────────┐  I2C      ┌────────────────┐  I2C      ┌──────────────┐
│ MPU6050  │──────────▶│                │──────────▶│ OLED 128x64  │
├──────────┤           │  Arduino Nano  │  GPIO     ├──────────────┤
│ MAX30102 │──────────▶│                │──────────▶│ Buzzer       │
└──────────┘           │                │──────────▶│ Red LED      │
                       └───────▲────────┘           └──────────────┘
                               │
                    Battery (power source)
```

### Wiring Notes

- The battery connects to the MT3608 step-up converter, whose output feeds the VIN pin of the Arduino Nano. The MAX30102, MPU6050, and OLED are powered from the 5 V rail.
- SDA and SCL of all three I2C devices connect to A4 and A5, with a 5 kΩ pull-up resistor from 5 V to each line.
- The buzzer is driven from D5, and the red LED is driven from D7 through a 1 kΩ current-limiting resistor.

---

## Repository Structure

```
assistive-smart-bracelet/
├── platformio.ini          # board and framework configuration
├── src/
│   └── main.cpp            # main program (loop, alarm, fall state machine)
├── lib/
│   ├── i2c/                # I2C driver (hardware TWI)
│   ├── millis/             # 1 ms timer based on Timer1
│   ├── max30105_avr/       # MAX30102/MAX30105 sensor driver
│   ├── heartrate/          # heart rate detection algorithm
│   ├── mpu6050/            # MPU6050 driver
│   ├── lcd/                # SSD1306 OLED driver
│   └── font/               # 6-column font for the OLED
├── include/
└── test/
```

### Module Descriptions

| Module | Description |
|--------|-------------|
| `i2c` | Hardware TWI driver for the ATmega328P. It provides initialization at 100 kHz, start, write, read (ACK and NACK), stop, and a timeout to prevent the program from hanging. |
| `millis` | Provides an Arduino-style `millis()` using Timer1 in CTC mode with an interrupt every 1 ms. |
| `max30105_avr` | Sensor driver. It configures the registers, reads the sensor FIFO, and stores the IR and Red samples in a circular buffer of 32 entries. |
| `heartrate` | Converts the IR signal into a BPM value (see the algorithm section below). |
| `mpu6050` | Initializes the sensor (accelerometer ±2 g, gyroscope ±2000 °/s) and reads the raw data. |
| `lcd` and `font` | SSD1306 OLED driver with a frame buffer in RAM. Text is drawn with a 6-column font stored in PROGMEM. |

---

## Software Design

### Initialization

When the device is powered on, the program performs the following steps.

1. Timer1 is configured for `millis()`, and global interrupts are enabled with `sei()`.
2. The I2C bus is initialized at 100 kHz.
3. The buzzer pin (PD5) and the LED pin (PD7) are set as outputs and turned off.
4. The OLED is initialized and shows `Initializing...`.
5. The MPU6050 is initialized, and the gyroscope is calibrated using 200 samples.
6. The heart rate sensor is verified through its Part ID (`0x15`). If this check fails, the OLED shows `MAX30105 ERROR` and the program stops.
7. The sensor is configured with `max30105_setup`, and the heart rate algorithm is reset with `hr_init`.

### Main Loop

Each iteration of the main loop performs five tasks.

1. **Heart rate.** The sensor FIFO is read, and every IR and Red sample is passed to `hr_process`. When a new beat is detected, the Stable BPM is compared with the limits of 60 and 100 bpm.
2. **Fall detection (every 50 ms).** The acceleration is read and processed by the fall detection state machine.
3. **Message reset.** After the alarm ends, the status `FALL: IMPACT!` returns to `FALL: none`.
4. **Display refresh (every 250 ms).** The OLED is updated. It is also updated immediately whenever the fall status changes.
5. **Alarm update.** `alarm_update()` runs without blocking, so sensor reading is never delayed by the alarm.

---

## Heart Rate Algorithm

The algorithm is implemented in `lib/heartrate/` and operates on the IR signal from the sensor.

1. **Signal check.** If the IR value is below 10000, the system assumes that no finger or skin is in contact with the sensor and resets all internal states (BPM = 0).
2. **Baseline and threshold.** A baseline is tracked with an IIR filter (weight 1/32). The threshold equals the baseline plus a dynamic offset of at least 100.
3. **Beat detection.** A beat is counted when the IR value crosses the threshold while rising, after a refractory period of 250 ms and with an amplitude change of at least 50. The direction of change in the IR and Red channels is also compared to filter out noise.
4. **Hysteresis.** The detector is re-armed after the IR value drops below the threshold minus 120.
5. **Beat intervals (RR).** Intervals between 300 and 2000 ms are stored in a buffer of 5 values.
6. **Result.** Instant BPM is computed from the latest interval. Stable BPM is computed as a trimmed mean around the median with a tolerance of ±20%. A BPM value is considered valid only between 30 and 220.

Sensor configuration used in the firmware: LED current `0xAF`, 4-sample averaging, Red and IR mode, 400 samples per second, pulse width 411 µs, and ADC range 16384. The effective output rate is about 100 samples per second.

---

## Fall Detection Algorithm

The acceleration is read every 50 ms, and the magnitude (in g) and the jerk (the change in magnitude from the previous sample) are calculated. A state machine with three states processes these values.

| State | Condition | Action | OLED Display |
|-------|-----------|--------|--------------|
| `IDLE` | Magnitude < 0.6 g | Move to `POSSIBLE_FALL` | `FALL: possible` |
| `POSSIBLE_FALL` | Magnitude < 0.6 g for 3 consecutive samples | Move to `WAIT_IMPACT` | `FALL: awaiting` |
| `POSSIBLE_FALL` | Magnitude returns to 0.6 g or higher | Return to `IDLE` | `FALL: none` |
| `WAIT_IMPACT` | Magnitude > 2.5 g or jerk > 1.5 g within 1000 ms | Trigger the alarm and return to `IDLE` | `FALL: IMPACT!` |
| `WAIT_IMPACT` | The 1000 ms window ends without an impact | Return to `IDLE` | `FALL: none` |

This sequence follows the pattern of a real fall, which is a loss of support (free-fall) followed by an impact. As a result, ordinary movements such as walking do not trigger the alarm directly.

---

## Alarm System

The alarm lasts 3 seconds and runs without blocking. The buzzer and the LED switch on together, and the pattern depends on the cause.

| Cause | On | Off | Description |
|-------|----|-----|-------------|
| Abnormal heart rate | 100 ms | 400 ms | Slow rhythm |
| Fall | 100 ms | 100 ms | Medium rhythm |
| Abnormal heart rate and fall | 50 ms | 50 ms | Fastest rhythm |

---

## OLED Display

```
Smart Assistive

Inst  : 072 bpm
Stable: 071 bpm

STATUS: NORMAL
FALL: none
```

| Line | Content |
|------|---------|
| 1 | Title `Smart Assistive` |
| 3 | Instant BPM |
| 4 | Stable BPM |
| 6 | `STATUS: NORMAL` or `STATUS: ALARM` (heart rate status) |
| 7 | Fall status (`none`, `possible`, `awaiting`, `IMPACT!`) |

---

## Configuration

The main parameters can be changed directly in the source code.

| Parameter | File | Default | Description |
|-----------|------|---------|-------------|
| `HR_MIN`, `HR_MAX` | `src/main.cpp` | 60 and 100 | Normal BPM range |
| `ALARM_DURATION_MS` | `src/main.cpp` | 3000 | Alarm duration |
| `FREEFALL_G` | `src/main.cpp` | 0.6 | Free-fall threshold (g) |
| `IMPACT_G` | `src/main.cpp` | 2.5 | Impact threshold (g) |
| `JERK_THRESHOLD_G` | `src/main.cpp` | 1.5 | Jerk threshold (g) |
| `FREEFALL_SAMPLES` | `src/main.cpp` | 3 | Consecutive free-fall samples required |
| `IMPACT_WINDOW_MS` | `src/main.cpp` | 1000 | Time window to wait for an impact |
| `SAMPLE_MS` | `src/main.cpp` | 50 | MPU6050 sampling period |
| `IR_CUTOFF_MIN` | `lib/heartrate/heartrate.cpp` | 10000 | Minimum IR level for a valid finger contact |

---

## Build and Upload

The project uses [PlatformIO](https://platformio.org/), which is best used through the PlatformIO IDE extension for VS Code.

```bash
# build
pio run

# upload to the Arduino Nano
pio run -t upload
```

The configuration in `platformio.ini` is as follows.

```ini
[env:nanoatmega328new]
platform = atmelavr
board = nanoatmega328new
framework = arduino
```

The `nanoatmega328new` environment is intended for an Arduino Nano with the new bootloader. If the upload fails on a board with the old bootloader, change `board` to `nanoatmega328`.

---

## Test Results

### Heart Rate Test

The readings of the device were compared with a Samsung Galaxy Fit 3 in 10 measurements.

| No | Samsung Galaxy Fit 3 | Assistive Smart Bracelet | Difference | Alarm |
|----|----------------------|--------------------------|------------|-------|
| 1 | 106 | 101 | 5 | On |
| 2 | 107 | 109 | -2 | On |
| 3 | 111 | 110 | 1 | On |
| 4 | 111 | 117 | -6 | On |
| 5 | 114 | 119 | -5 | On |
| 6 | 115 | 120 | -5 | On |
| 7 | 117 | 121 | -4 | On |
| 8 | 120 | 121 | -1 | On |
| 9 | 121 | 122 | -1 | On |
| 10 | 122 | 122 | 0 | On |
| Average | 114.4 | 116.2 | -1.8 | |

The average difference is about 1.57%. All readings were above 100 bpm, so the heart rate alarm was triggered in every trial.

### Fall Detection Test

The test covered 10 fall scenarios. In every scenario, the system passed through the `possible` and `awaiting` states, detected the impact, and triggered the alarm.

---

## Limitations and Future Work

- The heart rate test covered only readings above 100 bpm, and the fall test covered only fall scenarios. Daily activities such as walking, sitting down quickly, or raising the arm have not been tested, so the rate of false alarms is still unknown.
- The lower limit of 60 bpm may trigger the alarm for healthy people with a low resting heart rate.
- The gyroscope is only calibrated and is not yet used in the fall detection logic.
- The system does not measure SpO2.
- Alerts are local only (buzzer and LED), and there is no wireless notification.

Planned improvements include further calibration of the heart rate sensor, a wider range of test scenarios, wireless communication, power optimization, and a better user interface.

---

## Troubleshooting

**The OLED shows `MAX30105 ERROR`.** The sensor was not detected on the I2C bus. Check the SDA and SCL wiring (A4 and A5), the power supply of the sensor, and the pull-up resistors.

**The BPM value stays at 0.** The IR signal is below 10000. Make sure the skin is in steady contact with the sensor and that no ambient light reaches it.

**The OLED is blank.** Confirm that the OLED address is `0x3C` and that the controller is an SSD1306. For an SH1106, change the controller definition in `lib/lcd/lcd.h`.

**The fall alarm is too sensitive or does not appear.** Adjust `FREEFALL_G`, `IMPACT_G`, and `JERK_THRESHOLD_G` in `src/main.cpp`.

**The upload fails.** Check the COM port and the USB driver, then try changing `board` to `nanoatmega328` if the board has the old bootloader.

---

## Credits and References

- The Timer1-based `millis()` function is adapted from [monoclecat/avr-millis-function](https://github.com/monoclecat/avr-millis-function).