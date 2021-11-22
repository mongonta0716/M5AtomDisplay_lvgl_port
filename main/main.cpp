/* LVGL Example project
 *
 * Basic project to test LVGL on ESP32 based projects.
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "axp192.h"
#include "i2c_manager.h"

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "lvgl_helpers.h"

#define CONFIG_LV_USE_DEMO_MUSIC

#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
#if defined CONFIG_LV_USE_DEMO_WIDGETS
#include "lv_demos/src/lv_demo_widgets/lv_demo_widgets.h"
#elif defined CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER
#include "lv_demos/src/lv_demo_keypad_encoder/lv_demo_keypad_encoder.h"
#elif defined CONFIG_LV_USE_DEMO_BENCHMARK
#include "lv_demos/src/lv_demo_benchmark/lv_demo_benchmark.h"
#elif defined CONFIG_LV_USE_DEMO_STRESS
#include "lv_demos/src/lv_demo_stress/lv_demo_stress.h"
#elif defined CONFIG_LV_USE_DEMO_MUSIC
#include "lv_demos/src/lv_demo_music/lv_demo_music.h"
#else
#error "No demo application selected."
#endif
#endif

/*********************
 *      DEFINES
 *********************/
#define TAG "demo"
#define LV_TICK_PERIOD_MS 1
#define I2C_0 i2c_hal(0)

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main()
{
    /* Initialize your hardware. */
    axp192_init(I2C_0);
    axp192_ioctl(I2C_0, AXP192_GPIO0_SET_LEVEL, AXP192_HIGH);  // M-Bus Power
    axp192_ioctl(I2C_0, AXP192_GPIO1_SET_LEVEL, AXP192_LOW);   // Enable LED
    axp192_ioctl(I2C_0, AXP192_GPIO2_SET_LEVEL, AXP192_LOW);   // Disable speaker
    axp192_ioctl(I2C_0, AXP192_LDO2_SET_VOLTAGE, 3300);        // Set LDO2 LCD&TP voltage
    axp192_ioctl(I2C_0, AXP192_LDO2_ENABLE);                   // Enable LDO2
    axp192_ioctl(I2C_0, AXP192_GPIO4_SET_LEVEL, AXP192_LOW);   // LCD&TP Reset
    vTaskDelay(100 / portTICK_PERIOD_MS);
    axp192_ioctl(I2C_0, AXP192_GPIO4_SET_LEVEL, AXP192_HIGH);  // LCD&TP Hold
    vTaskDelay(100 / portTICK_PERIOD_MS);

    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 10, NULL, 0, NULL, 0);
}

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

static void guiTask(void *pvParameter)
{
    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t *buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    /* Use double buffered when not working with monochrome displays */
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    lv_color_t *buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);
#else
    static lv_color_t *buf2 = NULL;
#endif

    static lv_disp_draw_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820         \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A    \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D     \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;
#endif

    /* Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays. */
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.flush_cb = disp_driver_flush;

    /* When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb */
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;
#endif

    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
#endif

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    /* Create the demo application */
    create_demo_application();

    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    /* A task should NEVER return */
    free(buf1);
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    free(buf2);
#endif
    vTaskDelete(NULL);
}
//static void btn_event_cb(lv_event_t * e)
//{
    //lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_t * btn = lv_event_get_target(e);
    //if(code == LV_EVENT_CLICKED) {
        //static uint8_t cnt = 0;
        //cnt++;

        ///*Get the first child of the button which is the label and change its text*/
        //lv_obj_t * label = lv_obj_get_child(btn, 0);
        //lv_label_set_text_fmt(label, "Button: %d", cnt);
    //}
//}
static lv_obj_t * meter;
static lv_obj_t * meter2;

static void set_value(void * indic, int32_t v)
{
    lv_meter_set_indicator_value(meter, indic, v);
}

static void set_value2(void * indic2, int32_t v)
{
    lv_meter_set_indicator_value(meter2, indic2, v);
}

static void create_demo_application(void)
{

//    lv_obj_t * btn = lv_btn_create(lv_scr_act());     /*Add a button the current screen*/
    //lv_obj_set_pos(btn, 10, 10);                            /*Set its position*/
    //lv_obj_set_size(btn, 120, 50);                          /*Set its size*/
    //lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);           /*Assign a callback to the button*/

    //lv_obj_t * label = lv_label_create(btn);          /*Add a label to the button*/
    //lv_label_set_text(label, "n");                     /*Set the labels text*/
    //lv_obj_center(label);

    meter = lv_meter_create(lv_scr_act());
    meter2 = lv_meter_create(lv_scr_act());
    // lv_obj_center(meter);
    lv_obj_align(meter, LV_ALIGN_LEFT_MID, 25, -25);
    lv_obj_set_size(meter, 125, 125);
    lv_obj_align(meter2, LV_ALIGN_RIGHT_MID, -25, -25);
    lv_obj_set_size(meter2, 125, 125);

    /*Add a scale first*/
    lv_meter_scale_t * scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, scale, 8, 4, 15, lv_color_black(), 10);

    lv_meter_indicator_t * indic;

    /*Add a blue arc to the start*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 20);

    /*Make the tick lines blue at the start of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 20);

    /*Add a red arc to the end*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic, 80);
    lv_meter_set_indicator_end_value(meter, indic, 100);

    /*Make the tick lines red at the end of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 80);
    lv_meter_set_indicator_end_value(meter, indic, 100);

    /*Add a needle line indicator*/
    indic = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), -10);

    /*Add a scale first*/
    lv_meter_scale_t * scale2 = lv_meter_add_scale(meter2);
    lv_meter_set_scale_ticks(meter2, scale2, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter2, scale2, 8, 4, 15, lv_color_black(), 10);

    lv_meter_indicator_t * indic2;

    /*Add a blue arc to the start*/
    indic2 = lv_meter_add_arc(meter2, scale2, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter2, indic2, 0);
    lv_meter_set_indicator_end_value(meter2, indic2, 20);

    /*Make the tick lines blue at the start of the scale*/
    indic2 = lv_meter_add_scale_lines(meter2, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), false, 0);
    lv_meter_set_indicator_start_value(meter2, indic2, 0);
    lv_meter_set_indicator_end_value(meter2, indic2, 20);

    /*Add a red arc to the end*/
    indic2 = lv_meter_add_arc(meter2, scale2, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter2, indic2, 80);
    lv_meter_set_indicator_end_value(meter2, indic2, 100);

    /*Make the tick lines red at the end of the scale*/
    indic2 = lv_meter_add_scale_lines(meter2, scale2, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false, 0);
    lv_meter_set_indicator_start_value(meter2, indic2, 80);
    lv_meter_set_indicator_end_value(meter2, indic2, 100);

    /*Add a needle line indicator*/
    indic2 = lv_meter_add_needle_line(meter2, scale2, 4, lv_palette_main(LV_PALETTE_GREY), -10);

    lv_obj_t * label1 = lv_label_create(lv_scr_act());
    lv_label_set_long_mode(label1, LV_LABEL_LONG_WRAP);     /*Break the long lines*/
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_20, 0);
    lv_label_set_recolor(label1, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_text(label1, LV_SYMBOL_LOOP);
    lv_obj_set_width(label1, 150);  /*Set smaller width to make the lines wrap*/
    lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label1, LV_ALIGN_BOTTOM_MID, 0, -40);

    /*Create an animation to set the value*/
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_value);
    lv_anim_set_var(&a, indic);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

   /*Create an animation to set the value*/
    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_exec_cb(&a2, set_value2);
    lv_anim_set_var(&a2, indic2);
    lv_anim_set_values(&a2, 100, 0);
    lv_anim_set_time(&a2, 2000);
    lv_anim_set_repeat_delay(&a2, 100);
    lv_anim_set_playback_time(&a2, 500);
    lv_anim_set_playback_delay(&a2, 100);
    lv_anim_set_repeat_count(&a2, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a2);
    

}

static void lv_tick_task(void *arg)
{
    (void) arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}
