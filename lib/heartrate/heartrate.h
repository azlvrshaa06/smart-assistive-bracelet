#ifndef _HEARTRATE_H_
#define _HEARTRATE_H_

#include <stdint.h>

void hr_init(void);
uint8_t hr_process(uint32_t ir_now, uint32_t red_now);
uint16_t hr_get_instant(void);
uint16_t hr_get_stable(void);


#endif
