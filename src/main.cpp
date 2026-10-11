#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "max30105_avr.h"
#include "millis.h"
#include "heartrate.h"
#include "lcd.h"
#include "i2c.h"

extern "C" {
#include "mpu6050.h"
}

/* ================= PIN ================= */
#define BUZZER_PIN 5
#define LED_PIN    7

#define ALARM_DURATION_MS  3000

/* ================= IO ================= */
static inline void buzzer_init(void){ DDRD |= (1<<BUZZER_PIN); }
static inline void led_init(void){ DDRD |= (1<<LED_PIN); }
static inline void buzzer_on(void){ PORTD |= (1<<BUZZER_PIN); }
static inline void buzzer_off(void){ PORTD &= ~(1<<BUZZER_PIN); }
static inline void led_on(void){ PORTD |= (1<<LED_PIN); }
static inline void led_off(void){ PORTD &= ~(1<<LED_PIN); }

/* ================= ALARM ================= */
typedef enum {
    ALARM_NONE = 0,
    ALARM_HR,
    ALARM_FALL,
    ALARM_BOTH
} alarm_type_t;

typedef struct {
    uint8_t active;
    unsigned long start_ms;
    unsigned long duration_ms;
    unsigned long last_toggle;
    uint8_t buzzer_state;
} alarm_t;

static alarm_t alarm = {0};
static alarm_type_t alarm_type = ALARM_NONE;
static unsigned long time_ms = 0;

void alarm_start(unsigned long duration_ms, alarm_type_t type) {
    alarm.active = 1;
    alarm.start_ms = time_ms;
    alarm.duration_ms = duration_ms;
    alarm.last_toggle = 0;
    alarm.buzzer_state = 0;
    alarm_type = type;
}

void alarm_stop(void) {
    alarm.active = 0;
    alarm_type = ALARM_NONE;
    buzzer_off();
    led_off();
}

void alarm_update(void) {
    if (!alarm.active) return;

    if (time_ms - alarm.start_ms >= alarm.duration_ms) {
        alarm_stop();
        return;
    }

    unsigned long on_ms  = 100;
    unsigned long off_ms = 300;

    switch (alarm_type) {
        case ALARM_HR:
            on_ms  = 100;
            off_ms = 400;
            break;

        case ALARM_FALL:
            on_ms  = 100;
            off_ms = 100;
            break;

        case ALARM_BOTH:
            on_ms  = 50;
            off_ms = 50;
            break;

        default:
            break;
    }

    unsigned long interval = alarm.buzzer_state ? on_ms : off_ms;

    if (time_ms - alarm.last_toggle >= interval) {
        alarm.last_toggle = time_ms;
        alarm.buzzer_state ^= 1;

        if (alarm.buzzer_state) {
            buzzer_on();
            led_on();
        } else {
            buzzer_off();
            led_off();
        }
    }
}

/* ================= HR ================= */
int HR_MIN = 60;
int HR_MAX = 100;

MAX30105_t sensor;

/* ================= MPU ================= */
#define ACCEL_SENSITIVITY 16384.0f
#define GYRO_SENSITIVITY  16.4f

#define FREEFALL_G       0.6f
#define IMPACT_G         2.5f
#define JERK_THRESHOLD_G 1.5f

#define SAMPLE_MS        50
#define FREEFALL_SAMPLES 3
#define IMPACT_WINDOW_MS 1000

/* ================= OLED ================= */
static void oled_update_minimal(
    int hr_inst,
    int hr_stable,
    uint8_t hr_alarm_active,
    const char *fall_msg
){
    char buf[32];

    lcd_gotoxy(0,0);
    lcd_puts("Smart Assistive ");

    snprintf(buf,sizeof(buf),"Inst  : %3d bpm ",hr_inst);
    lcd_gotoxy(0,2); lcd_puts(buf);

    snprintf(buf,sizeof(buf),"Stable: %3d bpm ",hr_stable);
    lcd_gotoxy(0,3); lcd_puts(buf);

    lcd_gotoxy(0,5);
    if (hr_alarm_active) lcd_puts("STATUS: ALARM   ");
    else                lcd_puts("STATUS: NORMAL  ");

    lcd_gotoxy(0,6);
    lcd_puts(fall_msg);

    lcd_display();
}

static inline void oled_update_now(
    const char *fall_msg,
    uint8_t hr_alarm_active
){
    int hr_inst = hr_get_instant();
    int hr_stable = hr_get_stable();
    if (hr_stable == 0) hr_stable = hr_inst;

    oled_update_minimal(
        hr_inst,
        hr_stable,
        hr_alarm_active,
        fall_msg
    );
}

/* ================= GYRO CAL ================= */
static void calibrate_gyro(float out_bias[3], unsigned int samples)
{
    long sum[3] = {0};
    s16 r[3];

    for (unsigned int i=0;i<samples;i++) {
        MPU6050_getGyroRaw(r);
        sum[0]+=r[0]; sum[1]+=r[1]; sum[2]+=r[2];
        _delay_ms(5);
    }

    out_bias[0] = (float)sum[0]/samples/GYRO_SENSITIVITY;
    out_bias[1] = (float)sum[1]/samples/GYRO_SENSITIVITY;
    out_bias[2] = (float)sum[2]/samples/GYRO_SENSITIVITY;
}

/* ================= MAIN ================= */
int main(void)
{
    cli();
    init_millis(16000000UL);
    sei();

    i2c_init(100000);
    
    buzzer_init();
    led_init();
    buzzer_off();
    led_off();

    lcd_init(LCD_DISP_ON);
    lcd_clear_buffer();
    lcd_puts("Initializing...");
    lcd_display();

    MPU6050_Init();

    float gyro_bias[3]={0};
    calibrate_gyro(gyro_bias,200);


    if (!max30105_begin(&sensor)) {
        lcd_clrscr();
        lcd_puts("MAX30105 ERROR");
        lcd_display();
        while(1);
    }

    max30105_setup(&sensor,0xAF,4,2,400,411,16384);
    hr_init();

    time_ms = millis();
    unsigned long last_mpu_ms = time_ms;
    unsigned long last_display_refresh = time_ms;

    enum {IDLE, POSSIBLE_FALL, WAIT_IMPACT} state = IDLE;
    uint8_t ff_count = 0;
    unsigned long impact_window = 0;
    float prev_accel = 1.0f;

    const char *fall_msg = "FALL: none       ";
    uint8_t hr_alarm_active = 0;

    oled_update_now(fall_msg,0);

    while (1) {
        time_ms = millis();

        /* ===== HEART RATE ===== */
        max30105_check(&sensor);
        while (max30105_available(&sensor)) {
            uint32_t ir = sensor.ir[sensor.tail];
            uint32_t red = sensor.red[sensor.tail];
            sensor.tail = (sensor.tail + 1) % STORAGE_SIZE;

            if (hr_process(ir,red)) {
                uint16_t stab = hr_get_stable();

                hr_alarm_active =
                    (stab > 0) &&
                    ((int)stab < HR_MIN || (int)stab > HR_MAX);

                if (hr_alarm_active && !alarm.active) {
                    alarm_start(ALARM_DURATION_MS, ALARM_HR);
                }
            }
        }

        /* ===== MPU ===== */
        if (time_ms - last_mpu_ms >= SAMPLE_MS) {
            last_mpu_ms = time_ms;

            s16 acc[3];
            MPU6050_getAccelRaw(acc);

            float ax = acc[0]/ACCEL_SENSITIVITY;
            float ay = acc[1]/ACCEL_SENSITIVITY;
            float az = acc[2]/ACCEL_SENSITIVITY;

            float mag = sqrtf(ax*ax+ay*ay+az*az);
            float jerk = fabsf(mag - prev_accel);

            switch(state){
                case IDLE:
                    if (mag < FREEFALL_G){
                        ff_count=1;
                        state=POSSIBLE_FALL;
                        fall_msg="FALL: possible   ";
                        oled_update_now(fall_msg,hr_alarm_active);
                    }
                    break;

                case POSSIBLE_FALL:
                    if (mag < FREEFALL_G){
                        if (++ff_count>=FREEFALL_SAMPLES){
                            state=WAIT_IMPACT;
                            impact_window=0;
                            fall_msg="FALL: awaiting   ";
                            oled_update_now(fall_msg,hr_alarm_active);
                        }
                    } else {
                        state=IDLE;
                        fall_msg="FALL: none       ";
                        oled_update_now(fall_msg,hr_alarm_active);
                    }
                    break;

                case WAIT_IMPACT:
                    impact_window+=SAMPLE_MS;
                    if (mag>IMPACT_G || jerk>JERK_THRESHOLD_G){

                        if (hr_alarm_active)
                            alarm_start(ALARM_DURATION_MS, ALARM_BOTH);
                        else
                            alarm_start(ALARM_DURATION_MS, ALARM_FALL);

                        fall_msg="FALL: IMPACT!    ";
                        oled_update_now(fall_msg,hr_alarm_active);
                        state=IDLE;

                    } else if (impact_window>=IMPACT_WINDOW_MS){
                        state=IDLE;
                        fall_msg="FALL: none       ";
                        oled_update_now(fall_msg,hr_alarm_active);
                    }
                    break;
            }

            prev_accel = mag;
        }

        /* ===== AUTO RESET FALL DISPLAY ===== */
        if (!alarm.active && strcmp(fall_msg,"FALL: IMPACT!    ")==0) {
            fall_msg="FALL: none       ";
        }

        /* ===== PERIODIC OLED REFRESH ===== */
        if (time_ms - last_display_refresh >= 250) {
            last_display_refresh = time_ms;
            oled_update_now(fall_msg,hr_alarm_active);
        }

        alarm_update();
    }
}