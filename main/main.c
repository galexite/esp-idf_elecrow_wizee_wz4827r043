/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "hal/gpio_types.h"
#include "hal/lv_hal_indev.h"
#include "hal/spi_types.h"
#include "lv_api_map.h"
#include "misc/lv_mem.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "demos/widgets/lv_demo_widgets.h"
#include "soc/clk_tree_defs.h"

#include "esp_nvs_tc.h"
#include "lv_tc.h"
#include "lv_tc_screen.h"
#include "xpt2046.h"

// Pin numbers and timing for the LCD and touch screen controller.
#include "hw_config.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "example";
static void *buf1 = NULL;
static void *buf2 = NULL;

static xpt2046_handle_t xpt_handle;

// we use two semaphores to sync the VSYNC event and the LVGL task, to avoid potential tearing effect
#if CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM
SemaphoreHandle_t sem_vsync_end;
SemaphoreHandle_t sem_gui_ready;
#endif

extern void example_lvgl_demo_ui(lv_disp_t *disp);

static bool example_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
#if CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM
    if (xSemaphoreTakeFromISR(sem_gui_ready, &high_task_awoken) == pdTRUE) {
        xSemaphoreGiveFromISR(sem_vsync_end, &high_task_awoken);
    }
#endif
    return high_task_awoken == pdTRUE;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    ESP_LOGV(TAG, "Flush display from (%d, %d) to (%d, %d)", offsetx1, offsety1, offsetx2 + 1, offsety2 + 1);
#if CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM
    xSemaphoreGive(sem_gui_ready);
    xSemaphoreTake(sem_vsync_end, portMAX_DELAY);
#endif
    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

static void example_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    // xpt2046_handle_t xpt_handle = (xpt2046_handle_t) indev_drv->user_data;
    xpt2046_coord_t position;
    int16_t pressure;
    if (gpio_get_level(EXAMPLE_XPT2046_PIN_NUM_IRQ) == 0 &&
        xpt2046_update(xpt_handle, &position, &pressure) == ESP_OK) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = position.x;
        data->point.y = position.y;
        ESP_LOGD(TAG, "Touch at (%d, %d), pressure %d", position.x, position.y, pressure);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void example_ready_cb(lv_event_t *ev)
{
    ESP_LOGI(TAG, "Display LVGL demo widgets");
    lv_demo_widgets();
}

void app_main(void)
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions
    static lv_indev_drv_t indev_drv;

#if CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM
    ESP_LOGI(TAG, "Create semaphores");
    sem_vsync_end = xSemaphoreCreateBinary();
    assert(sem_vsync_end);
    sem_gui_ready = xSemaphoreCreateBinary();
    assert(sem_gui_ready);
#endif

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif

    ESP_LOGI(TAG, "Install RGB LCD panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t panel_config = {
        .data_width = 16, // RGB565 in parallel mode, thus 16bit in width
        .psram_trans_align = 64,
        .num_fbs = EXAMPLE_LCD_NUM_FB,
#if CONFIG_EXAMPLE_USE_BOUNCE_BUFFER
        .bounce_buffer_size_px = 10 * EXAMPLE_LCD_H_RES,
#endif
        .clk_src = LCD_CLK_SRC_PLL160M,
        .disp_gpio_num = EXAMPLE_PIN_NUM_DISP_EN,
        .pclk_gpio_num = EXAMPLE_PIN_NUM_PCLK,
        .vsync_gpio_num = EXAMPLE_PIN_NUM_VSYNC,
        .hsync_gpio_num = EXAMPLE_PIN_NUM_HSYNC,
        .de_gpio_num = EXAMPLE_PIN_NUM_DE,
        .data_gpio_nums = {
            EXAMPLE_PIN_NUM_DATA0,
            EXAMPLE_PIN_NUM_DATA1,
            EXAMPLE_PIN_NUM_DATA2,
            EXAMPLE_PIN_NUM_DATA3,
            EXAMPLE_PIN_NUM_DATA4,
            EXAMPLE_PIN_NUM_DATA5,
            EXAMPLE_PIN_NUM_DATA6,
            EXAMPLE_PIN_NUM_DATA7,
            EXAMPLE_PIN_NUM_DATA8,
            EXAMPLE_PIN_NUM_DATA9,
            EXAMPLE_PIN_NUM_DATA10,
            EXAMPLE_PIN_NUM_DATA11,
            EXAMPLE_PIN_NUM_DATA12,
            EXAMPLE_PIN_NUM_DATA13,
            EXAMPLE_PIN_NUM_DATA14,
            EXAMPLE_PIN_NUM_DATA15,
        },
        .timings = {
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
            .h_res = EXAMPLE_LCD_H_RES,
            .v_res = EXAMPLE_LCD_V_RES,
            // The following parameters should refer to LCD spec
            .hsync_back_porch = EXAMPLE_HSYNC_BACK_PORCH,
            .hsync_front_porch = EXAMPLE_HSYNC_FRONT_PORCH,
            .hsync_pulse_width = EXAMPLE_HSYNC_PULSE_WIDTH,
            .vsync_back_porch = EXAMPLE_VSYNC_BACK_PORCH,
            .vsync_front_porch = EXAMPLE_VSYNC_FRONT_PORCH,
            .vsync_pulse_width = EXAMPLE_VSYNC_PULSE_WIDTH,
            .flags.hsync_idle_low = EXAMPLE_HSYNC_IDLE_LOW,
            .flags.vsync_idle_low = EXAMPLE_VSYNC_IDLE_LOW,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true, // allocate frame buffer in PSRAM
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    ESP_LOGI(TAG, "Register event callbacks");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = example_on_vsync_event,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, &disp_drv));

    ESP_LOGI(TAG, "Initialize RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
#if CONFIG_EXAMPLE_DOUBLE_FB
    ESP_LOGI(TAG, "Use frame buffers as LVGL draw buffers");
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, &buf1, &buf2));
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES);
#else
    ESP_LOGI(TAG, "Allocate separate LVGL draw buffers from PSRAM");
    buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1);
    buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 100);
#endif // CONFIG_EXAMPLE_DOUBLE_FB

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
#if CONFIG_EXAMPLE_DOUBLE_FB
    disp_drv.full_refresh = true; // the full_refresh mode can maintain the synchronization between the two frame buffers
#endif
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Register touch screen driver with LVGL");
    spi_bus_config_t xpt_bus_config = {
        .sclk_io_num = EXAMPLE_XPT2046_PIN_NUM_SCLK,
        .mosi_io_num = EXAMPLE_XPT2046_PIN_NUM_MOSI,
        .miso_io_num = EXAMPLE_XPT2046_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &xpt_bus_config, SPI_DMA_CH_AUTO));

    gpio_reset_pin(EXAMPLE_XPT2046_PIN_NUM_CS);
    gpio_set_direction(EXAMPLE_XPT2046_PIN_NUM_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(EXAMPLE_XPT2046_PIN_NUM_CS, 1);

    gpio_reset_pin(EXAMPLE_XPT2046_PIN_NUM_IRQ);
    gpio_set_direction(EXAMPLE_XPT2046_PIN_NUM_IRQ, GPIO_MODE_INPUT);
    gpio_set_pull_mode(EXAMPLE_XPT2046_PIN_NUM_IRQ, GPIO_PULLUP_ONLY);

    xpt2046_config_t xpt_config = {
        .spi_config = {
            .host = SPI2_HOST,
            .clock_speed_hz = 1000000,
            .cs_io_num = EXAMPLE_XPT2046_PIN_NUM_CS,
        },
        .screen_config = {
            .size = {.x = EXAMPLE_LCD_H_RES, .y = EXAMPLE_LCD_V_RES},
            .flip_xy = true,
        },
        .oversample_count = 4,
        .moving_average_count = 1,
    };
    ESP_ERROR_CHECK(xpt2046_initialize(&xpt_handle, &xpt_config));

    // lv_indev_drv_init(&indev_drv);
    // indev_drv.type = LV_INDEV_TYPE_POINTER;
    // indev_drv.read_cb = example_touch_read_cb;
    // indev_drv.user_data = xpt_handle;
    lv_tc_indev_drv_init(&indev_drv, example_touch_read_cb);
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Load touchscreen calibration information (if present)");
    lv_tc_register_coeff_save_cb(esp_nvs_tc_coeff_save_cb);
    lv_obj_t *calibration_scr = lv_tc_screen_create();
    lv_obj_add_event_cb(calibration_scr, example_ready_cb, LV_EVENT_READY, NULL);
    if (esp_nvs_tc_coeff_init()) {
        example_ready_cb(NULL);
    } else {
        ESP_LOGI(TAG, "Touchscreen calibration information is not present, show calibration screen");
        lv_disp_load_scr(calibration_scr);
        lv_tc_screen_start(calibration_scr);
    }

    // example_ready_cb(NULL);

    ESP_LOGI(TAG, "End of setup");

    while (1) {
        // raise the task priority of LVGL and/or reduce the handler period can improve the performance
        vTaskDelay(pdMS_TO_TICKS(10));
        // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
        lv_timer_handler();
    }
}