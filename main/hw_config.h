#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     7000000
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL (!EXAMPLE_LCD_BK_LIGHT_ON_LEVEL)

#define EXAMPLE_PIN_NUM_BK_LIGHT       2
#define EXAMPLE_PIN_NUM_HSYNC          39
#define EXAMPLE_PIN_NUM_VSYNC          41
#define EXAMPLE_PIN_NUM_DE             40
#define EXAMPLE_PIN_NUM_PCLK           42
#define EXAMPLE_PIN_NUM_DATA0          8  // B0
#define EXAMPLE_PIN_NUM_DATA1          3  // B1
#define EXAMPLE_PIN_NUM_DATA2          46 // B2
#define EXAMPLE_PIN_NUM_DATA3          9  // B3
#define EXAMPLE_PIN_NUM_DATA4          1  // B4
#define EXAMPLE_PIN_NUM_DATA5          5  // G0
#define EXAMPLE_PIN_NUM_DATA6          6  // G1
#define EXAMPLE_PIN_NUM_DATA7          7  // G2
#define EXAMPLE_PIN_NUM_DATA8          15 // G3
#define EXAMPLE_PIN_NUM_DATA9          16 // G4
#define EXAMPLE_PIN_NUM_DATA10         4  // G5
#define EXAMPLE_PIN_NUM_DATA11         45 // R0
#define EXAMPLE_PIN_NUM_DATA12         48 // R1
#define EXAMPLE_PIN_NUM_DATA13         47 // R2
#define EXAMPLE_PIN_NUM_DATA14         21 // R3
#define EXAMPLE_PIN_NUM_DATA15         14 // R4
#define EXAMPLE_PIN_NUM_DISP_EN        -1

#define EXAMPLE_HSYNC_BACK_PORCH       43
#define EXAMPLE_HSYNC_FRONT_PORCH      8
#define EXAMPLE_HSYNC_PULSE_WIDTH      2
#define EXAMPLE_HSYNC_IDLE_LOW         1

#define EXAMPLE_VSYNC_BACK_PORCH       12
#define EXAMPLE_VSYNC_FRONT_PORCH      8
#define EXAMPLE_VSYNC_PULSE_WIDTH      2
#define EXAMPLE_VSYNC_IDLE_LOW         1

// HxV resolution in pixels
#define EXAMPLE_LCD_H_RES              480
#define EXAMPLE_LCD_V_RES              272

#if CONFIG_EXAMPLE_DOUBLE_FB
#define EXAMPLE_LCD_NUM_FB             2
#else
#define EXAMPLE_LCD_NUM_FB             1
#endif // CONFIG_EXAMPLE_DOUBLE_FB

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

// Touch screen controller
#define EXAMPLE_XPT2046_PIN_NUM_SCLK   12
#define EXAMPLE_XPT2046_PIN_NUM_MOSI   11
#define EXAMPLE_XPT2046_PIN_NUM_MISO   13
#define EXAMPLE_XPT2046_PIN_NUM_IRQ    36
#define EXAMPLE_XPT2046_PIN_NUM_CS     0
#define EXAMPLE_XPT2046_MIN_PRESSURE   5
