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

#include <M5AtomDisplay.h>

#define DISPLAY_WIDTH 1280
#define DISPLAY_HEIGHT 720
#define REFRESH_RATE   60

M5AtomDisplay display(DISPLAY_WIDTH, DISPLAY_HEIGHT, REFRESH_RATE);
//M5AtomDisplay tft;

extern "C" {
/* Littlevgl specific */
#include "lvgl.h"

static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf[DISPLAY_WIDTH * 10];


/*********************
 *      DEFINES
 *********************/
#define TAG "demo"
#define LV_TICK_PERIOD_MS 1

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    display.startWrite();
    //display.pushImageDMA(area->x1, area->y1, w, h, &color_p->full); // 画面表示が乱れる
    display.setAddrWindow(area->x1, area->y1, w, h);
    display.pushColors(&color_p->full, w * h, false);
    display.endWrite();

    lv_disp_flush_ready(disp);
}


/**********************
 *   APPLICATION MAIN
 **********************/
void setup_task(void)
{
    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    display.init();

    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISPLAY_WIDTH * 10);
    lv_init();

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    create_demo_application();

}

void loop_task(void) {

    lv_timer_handler();
    vTaskDelay(5/portTICK_PERIOD_MS);

}


static lv_obj_t * meter;
static lv_obj_t * meter2;

static void set_value(void * indic, int32_t v)
{
    lv_meter_set_indicator_value(meter, (lv_meter_indicator_t*)indic, v);
}

static void set_value2(void * indic2, int32_t v)
{
    lv_meter_set_indicator_value(meter2, (lv_meter_indicator_t*)indic2, v);
}

static void create_demo_application(void)
{

    meter = lv_meter_create(lv_scr_act());
    meter2 = lv_meter_create(lv_scr_act());
    // lv_obj_center(meter);
    lv_obj_align(meter, LV_ALIGN_LEFT_MID, 25, -25);
    lv_obj_set_size(meter, 300, 300);
    lv_obj_align(meter2, LV_ALIGN_RIGHT_MID, -25, -25);
    lv_obj_set_size(meter2, 300, 300);

    /*Add a scale first*/
    lv_meter_scale_t * scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_PINK));
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
    lv_meter_set_scale_ticks(meter2, scale2, 41, 2, 10, lv_palette_main(LV_PALETTE_PINK));
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

#if !defined ( ARDUINO )
  void loopTask(void*)
  {
    setup_task();
    for (;;) {
      loop_task();
    }
    vTaskDelete(NULL);
  }

  void app_main()
  {
    xTaskCreatePinnedToCore(loopTask, "loopTask", 8192, NULL, 1, NULL, 1);
  }
#endif

} // extern "C"