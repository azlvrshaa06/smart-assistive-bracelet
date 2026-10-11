#include "heartrate.h"
#include "millis.h"

// Algorithm constants
#define BASE_IIR_ALPHA    32
#define DYN_MIN_OFFSET    100
#define DYN_DIV_FACTOR    400
#define HYSTERESIS_OFFSET 120
#define REFRACTORY_MS     250
#define RR_WINDOW         5
#define MIN_AMPLITUDE     50
#define RED_CORR_MIN_DIV  6

// State variables
static uint32_t base_level = 0;
static uint8_t base_init = 0;
static uint8_t beat_state = 0;
static uint32_t last_beat_ms = 0;

static uint32_t rr_buf[RR_WINDOW];
static uint8_t rr_count = 0;
static uint8_t rr_idx = 0;

static uint16_t last_instant = 0;

// --- internal functions ---
static void push_rr(uint32_t rr);
static uint32_t median_rr(void);
static uint32_t trimmed_mean_rr(void);
static uint16_t rpm_from_rr(uint32_t rr_ms);
static uint32_t compute_threshold(uint32_t ir);
static uint8_t red_correlation_ok(uint32_t ir_prev, uint32_t ir_now, uint32_t red_prev, uint32_t red_now);
static uint8_t detect_and_count(uint32_t ir_now, uint32_t red_now, uint32_t ir_prev, uint32_t red_prev);

// --- public API ---
void hr_init(void) {
    base_level = 0;
    base_init = 0;
    beat_state = 0;
    last_beat_ms = 0;
    rr_count = 0;
    rr_idx = 0;
    last_instant = 0;
}

uint8_t hr_process(uint32_t ir_now, uint32_t red_now) {
    static uint32_t ir_prev = 0, red_prev = 0;

    uint8_t beat = detect_and_count(ir_now, red_now, ir_prev, red_prev);

    ir_prev = ir_now;
    red_prev = red_now;

    return beat;
}

uint16_t hr_get_instant(void) {
    return last_instant;
}

uint16_t hr_get_stable(void) {
    if (rr_count < 2) return 0;

    uint32_t avg = trimmed_mean_rr();
    return rpm_from_rr(avg);
}

// ---------- IMPLEMENTASI ALGORITHM ----------
static void push_rr(uint32_t rr) {
    if (rr < 300 || rr > 2000) return;
    if (rr_count < RR_WINDOW) rr_buf[rr_count++] = rr;
    else {
        rr_buf[rr_idx] = rr;
        rr_idx = (rr_idx + 1) % RR_WINDOW;
    }
}

static uint32_t median_rr(void) {
    if (rr_count == 0) return 0;
    uint32_t tmp[RR_WINDOW];

    for (uint8_t i=0;i<rr_count;i++) tmp[i]=rr_buf[i];

    for (uint8_t i=1;i<rr_count;i++){
        uint32_t v=tmp[i]; int j=i-1;
        while (j>=0 && tmp[j]>v) { tmp[j+1]=tmp[j]; j--; if (j<0) break; }
        tmp[j+1]=v;
    }

    return tmp[rr_count/2];
}

static uint32_t trimmed_mean_rr(void) {
    if (rr_count == 0) return 0;
    uint32_t med = median_rr();
    uint32_t sum = 0; uint8_t used = 0;

    for (uint8_t i=0;i<rr_count;i++){
        uint32_t rr = rr_buf[i];
        uint32_t diff = (rr>med)? rr-med : med-rr;
        if (diff * 100 <= 20u * med) { // within ±20%
            sum += rr; used++;
        }
    }
    if (used==0) return med;
    return (sum + used/2) / used;
}

static uint16_t rpm_from_rr(uint32_t rr_ms) {
    if (rr_ms == 0) return 0;
    uint32_t bpm = (60000UL + rr_ms/2) / rr_ms;
    if (bpm < 30 || bpm > 220) return 0;
    return bpm;
}

static uint32_t compute_threshold(uint32_t ir) {
    if (!base_init) { base_level = ir; base_init = 1; }
    else base_level = ((base_level * (BASE_IIR_ALPHA - 1)) + ir) / BASE_IIR_ALPHA;

    uint32_t dyn = base_level / DYN_DIV_FACTOR;
    if (dyn < DYN_MIN_OFFSET) dyn = DYN_MIN_OFFSET;
    return base_level + dyn;
}

static uint8_t red_correlation_ok(uint32_t ir_prev, uint32_t ir_now, uint32_t red_prev, uint32_t red_now) {
    int32_t d_ir = (int32_t)ir_now - (int32_t)ir_prev;
    int32_t d_red = (int32_t)red_now - (int32_t)red_prev;

    if (d_ir == 0) return 0;
    if ((d_ir > 0 && d_red > 0) || (d_ir < 0 && d_red < 0)) {
        int32_t abs_ir = d_ir>0?d_ir:-d_ir;
        int32_t abs_red = d_red>0?d_red:-d_red;
        if (abs_red * RED_CORR_MIN_DIV >= abs_ir) return 1;
        if (abs_ir > 1000) return 1;
    }
    return 0;
}

#define IR_CUTOFF_MIN 10000UL

static uint8_t detect_and_count(uint32_t ir_now, uint32_t red_now, uint32_t ir_prev, uint32_t red_prev) {
    uint32_t threshold = compute_threshold(ir_now);
    uint32_t now = millis();

    if (ir_now < IR_CUTOFF_MIN) {
        last_instant = 0;
        rr_count = 0;
        rr_idx = 0;
        beat_state = 0;
        last_beat_ms = 0;
        return 0;
    }

    if (!beat_state) {
        if (ir_now > threshold && ir_now > ir_prev && (now - last_beat_ms) > REFRACTORY_MS) {

            uint32_t absd = (ir_now > ir_prev) ? (ir_now - ir_prev) : (ir_prev - ir_now);
            if (absd < MIN_AMPLITUDE) return 0;

            if (!red_correlation_ok(ir_prev, ir_now, red_prev, red_now)) {
                if (absd < 200) return 0;
            }

            uint32_t rr = (last_beat_ms == 0) ? 0 : (now - last_beat_ms);
            if (last_beat_ms != 0 && rr >= 300 && rr <= 2000) push_rr(rr);

            last_beat_ms = now;
            beat_state = 1;

            last_instant = rpm_from_rr(rr);
            return 1;
        }
    } else {
        if (ir_now < (threshold - HYSTERESIS_OFFSET)) {
            beat_state = 0;
        }
    }
    return 0;
}
