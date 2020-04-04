/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <unistd.h>
#include "lcd.h"

static lcd_ctl_t lcd_ctl;

uint16_t g_lcd_display_buff[LCD_Y_MAX * LCD_X_MAX];

static uint16_t gray2rgb565[64] = {
    0x0000,
    0x0020,
    0x0841,
    0x0861,
    0x1082,
    0x10a2,
    0x18c3,
    0x18e3,
    0x2104,
    0x2124,
    0x2945,
    0x2965,
    0x3186,
    0x31a6,
    0x39c7,
    0x39e7,
    0x4208,
    0x4228,
    0x4a49,
    0x4a69,
    0x528a,
    0x52aa,
    0x5acb,
    0x5aeb,
    0x630c,
    0x632c,
    0x6b4d,
    0x6b6d,
    0x738e,
    0x73ae,
    0x7bcf,
    0x7bef,
    0x8410,
    0x8430,
    0x8c51,
    0x8c71,
    0x9492,
    0x94b2,
    0x9cd3,
    0x9cf3,
    0xa514,
    0xa534,
    0xad55,
    0xad75,
    0xb596,
    0xb5b6,
    0xbdd7,
    0xbdf7,
    0xc618,
    0xc638,
    0xce59,
    0xce79,
    0xd69a,
    0xd6ba,
    0xdedb,
    0xdefb,
    0xe71c,
    0xe73c,
    0xef5d,
    0xef7d,
    0xf79e,
    0xf7be,
    0xffdf,
    0xffff,
};

void lcd_polling_enable(void)
{
    lcd_ctl.mode = 0;
}

void lcd_interrupt_enable(void)
{
    lcd_ctl.mode = 1;
}

void lcd_init(uint8_t spi, uint8_t ss, uint8_t rst, uint8_t dcx, uint32_t freq, int8_t rst_pin, int8_t dcx_pin, uint8_t dma_ch)
{
    uint8_t data = 0;

    tft_hard_init(spi, ss, rst, dcx, freq, rst_pin, dcx_pin, dma_ch);
    /*soft reset*/
    tft_write_command(SOFTWARE_RESET);
    usleep(100000);
    /*exit sleep*/
    tft_write_command(SLEEP_OFF);
    usleep(100000);
    /*pixel format*/
    tft_write_command(PIXEL_FORMAT_SET);
    data = 0x55;
    tft_write_byte(&data, 1);
    lcd_set_direction(DIR_YX_RLDU);

    /*display on*/
    tft_write_command(DISPALY_ON);
    lcd_polling_enable();
}

void lcd_set_direction(lcd_dir_t dir)
{
    //dir |= 0x08;  //excahnge RGB
    lcd_ctl.dir = dir;
    if (dir & DIR_XY_MASK)
    {
        lcd_ctl.width = LCD_Y_MAX - 1;
        lcd_ctl.height = LCD_X_MAX - 1;
    }
    else
    {
        lcd_ctl.width = LCD_X_MAX - 1;
        lcd_ctl.height = LCD_Y_MAX - 1;
    }

    tft_write_command(MEMORY_ACCESS_CTL);
    tft_write_byte((uint8_t *)&dir, 1);
}

static uint32_t lcd_freq = 20000000; // default to 20MHz
void lcd_set_freq(uint32_t freq)
{
    tft_set_clk_freq(freq);
    lcd_freq = freq;
}

uint32_t lcd_get_freq()
{
    return lcd_freq;
}

void lcd_set_area(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t data[4] = {0};

    data[0] = (uint8_t)(x1 >> 8);
    data[1] = (uint8_t)(x1);
    data[2] = (uint8_t)(x2 >> 8);
    data[3] = (uint8_t)(x2);
    tft_write_command(HORIZONTAL_ADDRESS_SET);
    tft_write_byte(data, 4);

    data[0] = (uint8_t)(y1 >> 8);
    data[1] = (uint8_t)(y1);
    data[2] = (uint8_t)(y2 >> 8);
    data[3] = (uint8_t)(y2);
    tft_write_command(VERTICAL_ADDRESS_SET);
    tft_write_byte(data, 4);

    tft_write_command(MEMORY_WRITE);
}

void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_set_area(x, y, x, y);
    tft_write_half(&color, 1);
}

void lcd_clear(uint16_t color)
{
    uint32_t data = ((uint32_t)color << 16) | (uint32_t)color;

    lcd_set_area(0, 0, lcd_ctl.width, lcd_ctl.height);
    tft_fill_data(&data, LCD_X_MAX * LCD_Y_MAX / 2);
}

void lcd_fill_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint32_t data = ((uint32_t)color << 16) | (uint32_t)color;
    lcd_set_area(x1, y1, x2, y2);
    tft_fill_data(&data, (y2+1-y1)*(x2+1-x1) );
}

void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t width, uint16_t color)
{
    uint32_t data_buf[640] = {0};
    uint32_t *p = data_buf;
    uint32_t data = color;
    uint32_t index = 0;

    data = (data << 16) | data;
    for (index = 0; index < 160 * width; index++)
        *p++ = data;

    lcd_set_area(x1, y1, x2, y1 + width - 1);
    tft_write_word(data_buf, ((x2 - x1 + 1) * width + 1) / 2);
    lcd_set_area(x1, y2 - width + 1, x2, y2);
    tft_write_word(data_buf, ((x2 - x1 + 1) * width + 1) / 2);
    lcd_set_area(x1, y1, x1 + width - 1, y2);
    tft_write_word(data_buf, ((y2 - y1 + 1) * width + 1) / 2);
    lcd_set_area(x2 - width + 1, y1, x2, y2);
    tft_write_word(data_buf, ((y2 - y1 + 1) * width + 1) / 2);
}

#define SWAP_16(x) ((x >> 8 & 0xff) | (x << 8))

void lcd_draw_picture(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, uint16_t *ptr)
{
    uint32_t i;
    uint16_t *p = ptr;
    lcd_set_area(x1, y1, x1 + width - 1, y1 + height - 1);
    for (i = 0; i < LCD_MAX_PIXELS; i += 2)
    {
        g_lcd_display_buff[i] = SWAP_16(*(p + 1));
        g_lcd_display_buff[i + 1] = SWAP_16(*(p));
        p += 2;
    }
    tft_write_word((uint32_t*)g_lcd_display_buff, width * height / 2);
}

void lcd_draw_picture_scale(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, uint16_t srcWidth, uint16_t srcHeight, uint16_t *ptr)
{
    lcd_set_area(x1, y1, x1 + width - 1, y1 + height - 1);
    for (int xx = 0; xx < width; xx += 2) {
        for (int yy = 0; yy < height; yy++) {
            uint32_t scalePos = xx + width * yy;

            uint32_t origPos = (xx * srcWidth) / width +
                               ((yy * srcHeight) / height) * srcWidth;
            uint32_t origPos1 = ((xx + 1) * srcWidth) / width +
                                ((yy * srcHeight) / height) * srcWidth;
            g_lcd_display_buff[scalePos] = SWAP_16(ptr[origPos1]);
            g_lcd_display_buff[scalePos + 1] = SWAP_16(ptr[origPos]);
        }
    }
    tft_write_word((uint32_t *)g_lcd_display_buff, width * height / 2);
}

//draw pic's roi on (x,y)
//x,y of LCD, w,h is pic; rx,ry,rw,rh is roi
void lcd_draw_pic_roi(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rx, uint16_t ry, uint16_t rw, uint16_t rh, uint32_t *ptr)
{
    int y_oft;
    uint8_t *p;
    for (y_oft = 0; y_oft < rh; y_oft++)
    { //draw line by line
        p = (uint8_t *)(ptr) + w * 2 * (y_oft + ry) + 2 * rx;
        lcd_set_area(x, y + y_oft, x + rw - 1, y + y_oft);
        tft_write_byte(p, rw * 2); //, lcd_ctl.mode ? 2 : 0);
    }
    return;
}


void lcd_draw_pic_gray(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, uint8_t *ptr)
{
    uint32_t i;
    uint16_t *p = (uint16_t*)ptr;
    lcd_set_area(x1, y1, x1 + width - 1, y1 + height - 1);
    for (i = 0; i < LCD_MAX_PIXELS; i += 2) {
        g_lcd_display_buff[i] = (gray2rgb565[(ptr[i + 1]) >> 2]);
        g_lcd_display_buff[i + 1] = (gray2rgb565[(ptr[i]) >> 2]);
        p += 2;
    }
    tft_write_word((uint32_t *)g_lcd_display_buff, width * height / 2);
}

void lcd_draw_pic_grayroi(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rx, uint16_t ry, uint16_t rw, uint16_t rh, uint8_t *ptr)
{
    int y_oft;
    uint8_t *p;
    for (y_oft = 0; y_oft < rh; y_oft++)
    { //draw line by line
        p = (uint8_t *)(ptr) + w * (y_oft + ry) + rx;
        lcd_draw_pic_gray(x, y + y_oft, rw, 1, p);
    }
    return;
}

void lcd_ram_cpyimg(char *lcd, int lcdw, char *img, int imgw, int imgh, int x, int y)
{
    int i;
    for (i = 0; i < imgh; i++)
    {
        memcpy(lcd + lcdw * 2 * (y + i) + x * 2, img + imgw * 2 * i, imgw * 2);
    }
    return;
}
