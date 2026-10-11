#ifndef _FONT_H_
#define _FONT_H_

#include <avr/pgmspace.h>
#include <inttypes.h>

/* font data stored in PROGMEM */
extern const uint8_t ssd1306oled_font[][6] PROGMEM;
/* special_char: pairs of {char_code, position_in_font}
   char_code is the 8-bit character code that should map to the glyph
   position_in_font is the index in ssd1306oled_font (0-based)
   table is terminated by {0xFF, 0xFF} */
extern const uint8_t special_char[][2] PROGMEM;

#endif /* _FONT_H_ */