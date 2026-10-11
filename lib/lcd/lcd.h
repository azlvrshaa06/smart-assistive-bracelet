#ifndef LCD_H
#define LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#if (__GNUC__ * 100 + __GNUC_MINOR__) < 303
#error "This library requires AVR-GCC 3.3 or later, update to newer AVR-GCC compiler !"
#endif

#include <inttypes.h>
#include <avr/pgmspace.h>

/* Bus: choose I2C or SPI */
#define I2C                     /* use I2C by default */

/* Controller: choose SSD1306 or SH1106 */
#define SSD1306                 /* set SSD1306 (common 128x64 modules) */

/* Display mode: TEXTMODE or GRAPHICMODE */
#define GRAPHICMODE             /* enable graphic mode (needed for 128x64 graphics) */

/* Font to use (must be defined in font.h/c) */
#define FONT            ssd1306oled_font

/* I2C 7-bit address for SSD1306 (0x3C typical) */
#define LCD_I2C_ADR         (0x3C)    /* 7-bit address */

#ifdef I2C
#include "i2c.h"
#elif defined SPI
/* SPI pins default for AVR UNO - change if needed */
#define LCD_PORT    PORTB
#define LCD_DDR     DDRB
#define RES_PIN     PB0
#define DC_PIN      PB1
#define CS_PIN      PB2
#endif

#ifndef YES
#define YES        1
#endif

#define NORMALSIZE 1
#define DOUBLESIZE 2

#define LCD_DISP_OFF        0xAE
#define LCD_DISP_ON         0xAF

#define WHITE               0x01
#define BLACK               0x00

#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64

/* Public functions */
void lcd_command(uint8_t cmd[], uint8_t size);
void lcd_data(uint8_t data[], uint16_t size);
void lcd_init(uint8_t dispAttr);
void lcd_home(void);
void lcd_invert(uint8_t invert);
void lcd_sleep(uint8_t sleep);
void lcd_set_contrast(uint8_t contrast);
void lcd_puts(const char* s);
void lcd_puts_p(const char* progmem_s);
void lcd_clrscr(void);
void lcd_gotoxy(uint8_t x, uint8_t y);       /* x = character col, y = page/line */
void lcd_goto_xpix_y(uint8_t x, uint8_t y);  /* x = pixel, y = page/line */
void lcd_putc(char c);
void lcd_charMode(uint8_t mode);
void lcd_flip(uint8_t flipping);

#if defined GRAPHICMODE
uint8_t lcd_drawPixel(uint8_t x, uint8_t y, uint8_t color);
uint8_t lcd_drawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
uint8_t lcd_drawRect(uint8_t px1, uint8_t py1, uint8_t px2, uint8_t py2, uint8_t color);
uint8_t lcd_fillRect(uint8_t px1, uint8_t py1, uint8_t px2, uint8_t py2, uint8_t color);
uint8_t lcd_drawCircle(uint8_t center_x, uint8_t center_y, uint8_t radius, uint8_t color);
uint8_t lcd_fillCircle(uint8_t center_x, uint8_t center_y, uint8_t radius, uint8_t color);
uint8_t lcd_drawBitmap(uint8_t x, uint8_t y, const uint8_t picture[], uint8_t width, uint8_t height, uint8_t color);
void lcd_display(void);
void lcd_clear_buffer(void);
uint8_t lcd_check_buffer(uint8_t x, uint8_t y);
void lcd_display_block(uint8_t x, uint8_t line, uint8_t width);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LCD_H */