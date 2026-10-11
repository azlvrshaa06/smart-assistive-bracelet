#include "lcd.h"
#include "font.h"
#include <string.h>
#include <avr/pgmspace.h>

#if defined SPI
#include <util/delay.h>
#endif

static struct {
    uint8_t x; /* pixel coordinate */
    uint8_t y; /* page (line) */
} cursorPosition;

static uint8_t charMode = NORMALSIZE;

#if defined GRAPHICMODE
#include <stdlib.h>
static uint8_t displayBuffer[DISPLAY_HEIGHT/8][DISPLAY_WIDTH];
#elif defined TEXTMODE
#else
#error "No valid displaymode! Refer lcd.h"
#endif

const uint8_t init_sequence [] PROGMEM = {
    LCD_DISP_OFF,    // Display OFF (sleep mode)
    0x20, 0b00,      // Set Memory Addressing Mode
    0xB0,            // Set Page Start Address for Page Addressing Mode, 0-7
    0xC8,            // Set COM Output Scan Direction
    0x00,            // --set low column address
    0x10,            // --set high column address
    0x40,            // --set start line address
    0x81, 0x3F,      // Set contrast control register
    0xA1,            // Set Segment Re-map
    0xA6,            // Set display mode. A6=Normal; A7=Inverse
    0xA8, DISPLAY_HEIGHT-1, // Set multiplex ratio(1 to 64)
    0xA4,            // Output RAM to Display
    0xD3, 0x00,      // Set display offset. 00 = no offset
    0xD5,            // set display clock divide ratio/oscillator frequency
    0xF0,            // set divide ratio
    0xD9, 0x22,      // Set pre-charge period
#if DISPLAY_HEIGHT==64
    0xDA, 0x12,
#elif DISPLAY_HEIGHT==32
    0xDA, 0x02,
#endif
    0xDB,            // --set vcomh
    0x20,            // 0x20,0.77xVcc
    0x8D, 0x14       // Set DC-DC enable
};

#pragma mark LCD COMMUNICATION
void lcd_command(uint8_t cmd[], uint8_t size) {
#if defined I2C
    i2c_start((LCD_I2C_ADR << 1) | 0);
    i2c_write(0x00);    // 0x00 for command
    for (uint8_t i=0; i<size; i++) {
        i2c_write(cmd[i]);
    }
    i2c_stop();
#elif defined SPI
    LCD_PORT &= ~(1 << CS_PIN);
    LCD_PORT &= ~(1 << DC_PIN);
    for (uint8_t i=0; i<size; i++) {
        SPDR = cmd[i];
        while(!(SPSR & (1<<SPIF)));
    }
    LCD_PORT |= (1 << CS_PIN);
#endif
}

void lcd_data(uint8_t data[], uint16_t size) {
#if defined I2C
    i2c_start((LCD_I2C_ADR << 1) | 0);
    i2c_write(0x40);    // 0x40 for data
    for (uint16_t i = 0; i<size; i++) {
        i2c_write(data[i]);
    }
    i2c_stop();
#elif defined SPI
    LCD_PORT &= ~(1 << CS_PIN);
    LCD_PORT |= (1 << DC_PIN);
    for (uint16_t i = 0; i<size; i++) {
        SPDR = data[i];
        while(!(SPSR & (1<<SPIF)));
    }
    LCD_PORT |= (1 << CS_PIN);
#endif
}

#pragma mark GENERAL FUNCTIONS
void lcd_init(uint8_t dispAttr){
#if defined I2C
    // i2c_init();
#elif defined SPI
    DDRB |= (1 << PB2)|(1 << PB3)|(1 << PB5);
    SPCR = (1 << SPE)|(1<<MSTR)|(1<<SPR0);
    LCD_DDR |= (1 << CS_PIN)|(1 << DC_PIN)|(1 << RES_PIN);
    LCD_PORT |= (1 << CS_PIN)|(1 << DC_PIN)|(1 << RES_PIN);
    LCD_PORT &= ~(1 << RES_PIN);
    _delay_ms(10);
    LCD_PORT |= (1 << RES_PIN);
#endif

    uint8_t commandSequence[sizeof(init_sequence)+1];
    for (uint8_t i = 0; i < sizeof(init_sequence); i++) {
        commandSequence[i] = (uint8_t)pgm_read_byte(&init_sequence[i]);
    }
    commandSequence[sizeof(init_sequence)] = dispAttr;
    lcd_command(commandSequence, sizeof(commandSequence));
    lcd_clrscr();
}

void lcd_gotoxy(uint8_t x, uint8_t y){
    /* x is column index (character), convert to pixel */
    x = x * (uint8_t)sizeof(FONT[0]);
    lcd_goto_xpix_y(x,y);
}

void lcd_goto_xpix_y(uint8_t x, uint8_t y){
    if( x > (DISPLAY_WIDTH) || y > (DISPLAY_HEIGHT/8-1)) return; /* out of display */
    cursorPosition.x = x;
    cursorPosition.y = y;
#if defined (SSD1306) || defined (SSD1309)
    uint8_t commandSequence[] = {0xb0 + y, 0x21, x, 0x7f};
#elif defined SH1106
    uint8_t commandSequence[] = {0xb0 + y, 0x21, 0x00 + ((2 + x) & 0x0f), 0x10 + (((2 + x) & 0xf0) >> 4), 0x7f};
#endif
    lcd_command(commandSequence, sizeof(commandSequence));
}

void lcd_clrscr(void){
#ifdef GRAPHICMODE
    for (uint8_t i = 0; i < DISPLAY_HEIGHT/8; i++){
        memset(displayBuffer[i], 0x00, sizeof(displayBuffer[i]));
        lcd_gotoxy(0,i);
        lcd_data(displayBuffer[i], sizeof(displayBuffer[i]));
    }
#elif defined TEXTMODE
    uint8_t displayBuffer[DISPLAY_WIDTH];
    memset(displayBuffer, 0x00, sizeof(displayBuffer));
    for (uint8_t i = 0; i < DISPLAY_HEIGHT/8; i++){
        lcd_gotoxy(0,i);
        lcd_data(displayBuffer, sizeof(displayBuffer));
    }
#endif
    lcd_home();
}

void lcd_home(void){
    lcd_gotoxy(0, 0);
}

void lcd_invert(uint8_t invert){
    uint8_t commandSequence[1];
    commandSequence[0] = (invert != YES) ? 0xA6 : 0xA7;
    lcd_command(commandSequence, 1);
}

void lcd_sleep(uint8_t sleep){
    uint8_t commandSequence[1];
    commandSequence[0] = (sleep != YES) ? 0xAF : 0xAE;
    lcd_command(commandSequence, 1);
}

void lcd_set_contrast(uint8_t contrast){
    uint8_t commandSequence[2] = {0x81, contrast};
    lcd_command(commandSequence, sizeof(commandSequence));
}

void lcd_putc(char c){
    uint8_t CHAR_W = sizeof(FONT[0]);
    uint8_t charIndex = cursorPosition.x / CHAR_W; /* current column (character index) */

    switch (c) {
        case '\b': {
            /* backspace: move cursor back one character (or two if double size), clear and reposition */
            uint8_t step = charMode;
            if (charIndex >= step) {
                charIndex -= step;
            } else {
                charIndex = 0;
            }
            lcd_gotoxy(charIndex, cursorPosition.y);
            /* print space to erase */
            lcd_putc(' ');
            /* go back to start of erased char */
            lcd_gotoxy(charIndex, cursorPosition.y);
            break;
        }
        case '\t': {
            /* tab: move forward 4 characters * charMode */
            uint8_t step = charMode * 4;
            uint8_t maxCols = DISPLAY_WIDTH / CHAR_W;
            if ((charIndex + step) < maxCols) {
                lcd_gotoxy(charIndex + step, cursorPosition.y);
            } else {
                lcd_gotoxy(maxCols - 1, cursorPosition.y);
            }
            break;
        }
        case '\n': {
            /* linefeed */
            if (cursorPosition.y < (DISPLAY_HEIGHT/8-1)) {
                uint8_t newY = cursorPosition.y + charMode;
                lcd_gotoxy(charIndex, newY);
            }
            break;
        }
        case '\r':
            /* carriage return */
            lcd_gotoxy(0, cursorPosition.y);
            break;
        default: {
            /* print character */
            /* check fits in line, cursorPosition.x is pixel coordinate */
            if ((cursorPosition.x >= DISPLAY_WIDTH - CHAR_W) || (c < ' ')) break;

            /* mapping char to font index */
            uint8_t idx = (uint8_t)c - (uint8_t)' ';
            /* check special char table threshold to skip table search when unnecessary */
            uint8_t first_special_pos = (uint8_t)pgm_read_byte(&special_char[0][1]);
            if (idx >= first_special_pos) {
                uint8_t temp = idx;
                idx = 0xFF;
                for (uint8_t i = 0; ; i++) {
                    uint8_t sc0 = pgm_read_byte(&special_char[i][0]);
                    uint8_t sc1 = pgm_read_byte(&special_char[i][1]);
                    if (sc0 == 0xFF && sc1 == 0xFF) break;
                    if ((uint8_t)(sc0 - (uint8_t)' ') == temp) {
                        idx = sc1;
                        break;
                    }
                }
                if (idx == 0xFF) break; /* not found in special table */
            }

#ifdef GRAPHICMODE
            if (charMode == DOUBLESIZE) {
                /* double-size character; expand vertically and horizontally */
                uint16_t doubleChar[sizeof(FONT[0])];
                uint8_t dChar;
                if ((cursorPosition.x + 2 * CHAR_W) > DISPLAY_WIDTH) break;

                for (uint8_t i = 0; i < sizeof(FONT[0]); i++) {
                    doubleChar[i] = 0;
                    dChar = (uint8_t)pgm_read_byte(&(FONT[(uint8_t)idx][i]));
                    for (uint8_t j = 0; j < 8; j++) {
                        if ((dChar & (1 << j))) {
                            /* set two vertical bits (j*2 and j*2+1) */
                            doubleChar[i] |= (1u << (j * 2));
                            doubleChar[i] |= (1u << (j * 2 + 1));
                        }
                    }
                }
                /* copy into buffer (2 pages high, 2 columns wide per original column) */
                for (uint8_t i = 0; i < sizeof(FONT[0]); i++) {
                    uint8_t low = (uint8_t)(doubleChar[i] & 0xFF);
                    uint8_t high = (uint8_t)((doubleChar[i] >> 8) & 0xFF);
                    displayBuffer[cursorPosition.y + 1][cursorPosition.x + (2 * i)]     = high;
                    displayBuffer[cursorPosition.y + 1][cursorPosition.x + (2 * i) + 1] = high;
                    displayBuffer[cursorPosition.y][cursorPosition.x + (2 * i)]         = low;
                    displayBuffer[cursorPosition.y][cursorPosition.x + (2 * i) + 1]     = low;
                }
                cursorPosition.x += CHAR_W * 2;
            } else {
                if ((cursorPosition.x + CHAR_W) > DISPLAY_WIDTH) break;
                for (uint8_t i = 0; i < sizeof(FONT[0]); i++) {
                    displayBuffer[cursorPosition.y][cursorPosition.x + i] = (uint8_t)pgm_read_byte(&(FONT[(uint8_t)idx][i]));
                }
                cursorPosition.x += CHAR_W;
            }
#elif defined TEXTMODE
            if (charMode == DOUBLESIZE) {
                uint16_t doubleChar[sizeof(FONT[0])];
                uint8_t dChar;
                if ((cursorPosition.x + 2 * CHAR_W) > DISPLAY_WIDTH) break;

                for (uint8_t i = 0; i < sizeof(FONT[0]); i++) {
                    doubleChar[i] = 0;
                    dChar = (uint8_t)pgm_read_byte(&(FONT[(uint8_t)idx][i]));
                    for (uint8_t j = 0; j < 8; j++) {
                        if ((dChar & (1 << j))) {
                            doubleChar[i] |= (1u << (j * 2));
                            doubleChar[i] |= (1u << (j * 2 + 1));
                        }
                    }
                }
                uint8_t data[sizeof(FONT[0]) * 2];
                for (uint8_t i = 0; i < sizeof(FONT[0]); i++) {
                    data[i << 1] = (uint8_t)(doubleChar[i] & 0xFF);
                    data[(i << 1) + 1] = (uint8_t)(doubleChar[i] & 0xFF);
                }
                lcd_data(data, sizeof(FONT[0]) * 2);
#if defined (SSD1306) || defined (SSD1309)
                uint8_t commandSequence[] = {0xb0 + cursorPosition.y + 1, 0x21, cursorPosition.x, 0x7f};
#elif defined SH1106
                uint8_t commandSequence[] = {0xb0 + cursorPosition.y + 1, 0x21, 0x00 + ((2 + cursorPosition.x) & 0x0f), 0x10 + (((2 + cursorPosition.x) & 0xf0) >> 4), 0x7f};
#endif
                lcd_command(commandSequence, sizeof(commandSequence));

                for (uint8_t i = 0; i < sizeof(FONT[0]); i++) {
                    data[i << 1] = (uint8_t)((doubleChar[i] >> 8) & 0xFF);
                    data[(i << 1) + 1] = (uint8_t)((doubleChar[i] >> 8) & 0xFF);
                }
                lcd_data(data, sizeof(FONT[0]) * 2);

                commandSequence[0] = 0xb0 + cursorPosition.y;
#if defined (SSD1306) || defined (SSD1309)
                commandSequence[2] = cursorPosition.x + (2 * CHAR_W);
#elif defined SH1106
                commandSequence[2] = 0x00 + ((2 + cursorPosition.x + (2 * CHAR_W)) & 0x0f);
                commandSequence[3] = 0x10 + (((2 + cursorPosition.x + (2 * CHAR_W)) & 0xf0) >> 4);
#endif
                lcd_command(commandSequence, sizeof(commandSequence));
                cursorPosition.x += CHAR_W * 2;
            } else {
                uint8_t data[sizeof(FONT[0])];
                if ((cursorPosition.x + CHAR_W) > DISPLAY_WIDTH) break;
                for (uint8_t i = 0; i < sizeof(FONT[0]); i++) {
                    data[i] = (uint8_t)pgm_read_byte(&(FONT[(uint8_t)idx][i]));
                }
                lcd_data(data, sizeof(FONT[0]));
                cursorPosition.x += CHAR_W;
            }
#endif
            break;
        }
    }
}

void lcd_charMode(uint8_t mode){
    if (mode == NORMALSIZE || mode == DOUBLESIZE) charMode = mode;
}

void lcd_flip(uint8_t flipping){
    uint8_t command[2] = {0xC8, 0xA1};
    switch(flipping){
        case 0:
            command[0] = 0xC8;
            command[1] = 0xA1;
            lcd_command(command, sizeof(command));
            break;
        case 1:
            command[0] = 0xC0;
            command[1] = 0xA0;
            lcd_command(command, sizeof(command));
            break;
        case 2:
            command[0] = 0xC0;
            lcd_command(command, 1);
            break;
        case 3:
            command[1] = 0xA0;
            lcd_command(command, sizeof(command));
            break;
        default:
            break;
    }
}

void lcd_puts(const char* s){
    while (*s) {
        lcd_putc(*s++);
    }
}

void lcd_puts_p(const char* progmem_s){
    register uint8_t c;
    while ((c = (uint8_t)pgm_read_byte(progmem_s++))) {
        lcd_putc((char)c);
    }
}

#ifdef GRAPHICMODE
#pragma mark GRAPHIC FUNCTIONS
uint8_t lcd_drawPixel(uint8_t x, uint8_t y, uint8_t color){
    if (x > DISPLAY_WIDTH - 1 || y > (DISPLAY_HEIGHT - 1)) return 1;
    if (color == WHITE) {
        displayBuffer[(y / 8)][x] |= (1 << (y % 8));
    } else {
        displayBuffer[(y / 8)][x] &= ~(1 << (y % 8));
    }
    return 0;
}

uint8_t lcd_drawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color){
    uint8_t result = 0;
    int dx =  abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        result = lcd_drawPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > dy) { err += dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
    return result;
}

uint8_t lcd_drawRect(uint8_t px1, uint8_t py1, uint8_t px2, uint8_t py2, uint8_t color){
    uint8_t result = 0;
    result = lcd_drawLine(px1, py1, px2, py1, color);
    result = lcd_drawLine(px2, py1, px2, py2, color);
    result = lcd_drawLine(px2, py2, px1, py2, color);
    result = lcd_drawLine(px1, py2, px1, py1, color);
    return result;
}

uint8_t lcd_fillRect(uint8_t px1, uint8_t py1, uint8_t px2, uint8_t py2, uint8_t color){
    uint8_t result = 0;
    if (px1 > px2) {
        uint8_t temp = px1; px1 = px2; px2 = temp;
        temp = py1; py1 = py2; py2 = temp;
    }
    for (uint8_t i = 0; i <= (py2 - py1); i++) {
        result = lcd_drawLine(px1, py1 + i, px2, py1 + i, color);
    }
    return result;
}

uint8_t lcd_drawCircle(uint8_t center_x, uint8_t center_y, uint8_t radius, uint8_t color){
    uint8_t result = 0;
    int16_t f = 1 - radius;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t x = 0;
    int16_t y = radius;

    lcd_drawPixel(center_x, center_y + radius, color);
    lcd_drawPixel(center_x, center_y - radius, color);
    lcd_drawPixel(center_x + radius, center_y, color);
    lcd_drawPixel(center_x - radius, center_y, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        lcd_drawPixel(center_x + x, center_y + y, color);
        lcd_drawPixel(center_x - x, center_y + y, color);
        lcd_drawPixel(center_x + x, center_y - y, color);
        lcd_drawPixel(center_x - x, center_y - y, color);
        lcd_drawPixel(center_x + y, center_y + x, color);
        lcd_drawPixel(center_x - y, center_y + x, color);
        lcd_drawPixel(center_x + y, center_y - x, color);
        lcd_drawPixel(center_x - y, center_y - x, color);
    }
    return result;
}

uint8_t lcd_fillCircle(uint8_t center_x, uint8_t center_y, uint8_t radius, uint8_t color) {
    uint8_t result = 0;
    for (uint8_t i = 0; i <= radius; i++) {
        result = lcd_drawCircle(center_x, center_y, i, color);
    }
    return result;
}

uint8_t lcd_drawBitmap(uint8_t x, uint8_t y, const uint8_t *picture, uint8_t width, uint8_t height, uint8_t color){
    uint8_t result = 0, i, j, byteWidth = (width + 7) / 8;
    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            if (pgm_read_byte(picture + j * byteWidth + i / 8) & (128 >> (i & 7))) {
                result = lcd_drawPixel(x + i, y + j, color);
            } else {
                result = lcd_drawPixel(x + i, y + j, !color);
            }
        }
    }
    return result;
}

void lcd_display() {
#if defined (SSD1306) || defined (SSD1309)
    lcd_gotoxy(0,0);
    lcd_data(&displayBuffer[0][0], DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);
#elif defined SH1106
    for (uint8_t i = 0; i < DISPLAY_HEIGHT/8; i++){
        lcd_gotoxy(0,i);
        lcd_data(displayBuffer[i], sizeof(displayBuffer[i]));
    }
#endif
}

void lcd_clear_buffer() {
    for (uint8_t i = 0; i < DISPLAY_HEIGHT/8; i++){
        memset(displayBuffer[i], 0x00, sizeof(displayBuffer[i]));
    }
}

uint8_t lcd_check_buffer(uint8_t x, uint8_t y) {
    if (x > DISPLAY_WIDTH - 1 || y > (DISPLAY_HEIGHT - 1)) return 0; /* out of Display */
    /* Use same mapping as lcd_drawPixel */
    return (displayBuffer[(y / 8)][x] & (1 << (y % 8))) ? 1 : 0;
}

void lcd_display_block(uint8_t x, uint8_t line, uint8_t width) {
    if (line > (DISPLAY_HEIGHT/8-1) || x > DISPLAY_WIDTH - 1) { return; }
    if (x + width > DISPLAY_WIDTH) {
        width = DISPLAY_WIDTH - x;
    }
    lcd_goto_xpix_y(x, line);
    lcd_data(&displayBuffer[line][x], width);
}
#endif /* GRAPHICMODE */