#define USE_ST7735 1

#ifdef USE_ST7789
#include "cfg_st7789.h"
#elif USE_ST7735
#include "cfg_st7735.h"
#elif
#pragma message ("no display available")
#endif

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_random.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"

#include "esp_lcd_panel_st7789.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"

#define MATH_PI 3.1415926

#define PERF_MEM_USAGE 0

#define DESIGN_RESOLUTION_WIDTH 240
#define DESIGN_RESOLUTION_HEIGHT 240

#define PARTICLE_DIAMOND_POOL_SIZE 4
#define PARTICLE_EMERALD_POOL_SIZE 4
#define PARTICLE_IRON_INGOT_POOL_SIZE 4
#define PARTICLE_GOLD_INGOT_POOL_SIZE 4

LV_IMAGE_DECLARE(image_logo);
LV_IMAGE_DECLARE(image_diamond_pickaxe);
LV_IMAGE_DECLARE(image_diamond);
LV_IMAGE_DECLARE(image_emerald);
LV_IMAGE_DECLARE(image_iron_ingot);
LV_IMAGE_DECLARE(image_gold_ingot);

#define DISP_SCLK GPIO_NUM_4
#define DISP_MOSI GPIO_NUM_6

static esp_lcd_panel_handle_t main_lcd_panel_handle;
static lv_disp_t* lvgl_main_display_handle;

static uint32_t display_resolution_width = 0;
static uint32_t display_resolution_height = 0;
static uint32_t safe_area_width = 0;
static uint32_t safe_area_height = 0;
static float display_scale_factor = 0;

static lv_obj_t* lv_image_diamond_pickaxe;
static lv_obj_t** lv_image_diamonds;
static lv_obj_t** lv_image_emeralds;
static lv_obj_t** lv_image_iron_ingots;
static lv_obj_t** lv_image_gold_ingots;

void init_spi_bus(){
    const spi_bus_config_t buscfg = {
            .sclk_io_num = DISP_SCLK,
            .mosi_io_num = DISP_MOSI,
            .miso_io_num = GPIO_NUM_NC,
            .quadwp_io_num = GPIO_NUM_NC,
            .quadhd_io_num = GPIO_NUM_NC,
            .max_transfer_sz = DISP_HEIGHT * DISP_DRAW_BUFFER_HEIGHT * sizeof(uint16_t)
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

void anim_move_particle_randomly_on_start(lv_anim_t* animation) {
    uint32_t half_width = display_resolution_width / 2;
    uint32_t half_height = display_resolution_height / 2;
    int32_t x = (int32_t) (-half_width + (esp_random() % display_resolution_width));
    int32_t y = (int32_t) (-half_height + (esp_random() % display_resolution_height));

    lv_obj_align(animation->var, LV_ALIGN_CENTER, x, y);
}

void anim_cb_set_opa(lv_anim_t* anim, int32_t value){
    lv_obj_set_style_image_opa(anim->var, value, 0);
}

lv_obj_t** init_particle_pool(size_t size, const lv_image_dsc_t* dsc){
    lv_obj_t** pool = calloc(size, sizeof(lv_obj_t*));
    for (int i = 0; i < size; ++i) {
        pool[i] = lv_image_create(lv_screen_active());
        lv_image_set_src(pool[i], dsc);
        lv_image_set_scale(pool[i], (uint32_t) (256 * display_scale_factor));
        lv_obj_align(pool[i], LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_image_opa(pool[i], 0, 0);

        lv_anim_t fade_in_anim;
        lv_anim_init(&fade_in_anim);
        lv_anim_set_var(&fade_in_anim, pool[i]);
        lv_anim_set_duration(&fade_in_anim, 3000);
        lv_anim_set_values(&fade_in_anim, 0, 255);
        lv_anim_set_path_cb(&fade_in_anim, lv_anim_path_ease_in_out);
        lv_anim_set_start_cb(&fade_in_anim, anim_move_particle_randomly_on_start);
        lv_anim_set_custom_exec_cb(&fade_in_anim, anim_cb_set_opa);

        lv_anim_t fade_out_anim;
        lv_anim_init(&fade_out_anim);
        lv_anim_set_var(&fade_out_anim, pool[i]);
        lv_anim_set_duration(&fade_out_anim, 3000);
        lv_anim_set_values(&fade_out_anim, 255, 0);
        lv_anim_set_path_cb(&fade_out_anim, lv_anim_path_ease_in_out);
        lv_anim_set_custom_exec_cb(&fade_out_anim, anim_cb_set_opa);

        lv_anim_timeline_t* timeline = lv_anim_timeline_create();
        lv_anim_timeline_add(timeline, esp_random() % 2048, &fade_in_anim);
        lv_anim_timeline_add(timeline, lv_anim_timeline_get_playtime(timeline), &fade_out_anim);
        lv_anim_timeline_set_repeat_delay(timeline, esp_random() % 1024);
        lv_anim_timeline_set_repeat_count(timeline, LV_ANIM_REPEAT_INFINITE);

        lv_anim_timeline_start(timeline);
    }
    return pool;
}

void anim_intro_end(lv_anim_t* anim){


    lv_image_diamonds = init_particle_pool(PARTICLE_DIAMOND_POOL_SIZE, &image_diamond);
    lv_image_emeralds = init_particle_pool(PARTICLE_EMERALD_POOL_SIZE, &image_emerald);
    lv_image_iron_ingots = init_particle_pool(PARTICLE_IRON_INGOT_POOL_SIZE, &image_iron_ingot);
    lv_image_gold_ingots = init_particle_pool(PARTICLE_GOLD_INGOT_POOL_SIZE, &image_gold_ingot);

    lv_obj_t * mask = lv_obj_create(lv_screen_active());
    lv_obj_set_size(mask , lv_pct(100), lv_pct(100));
    lv_obj_align(mask, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(mask ,lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(mask ,LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(mask ,160, 0);

    lv_image_diamond_pickaxe = lv_gif_create(lv_screen_active());
    lv_gif_set_src(lv_image_diamond_pickaxe, &image_diamond_pickaxe);
    lv_image_set_scale(lv_image_diamond_pickaxe, (uint32_t) (256 * display_scale_factor));
    lv_obj_set_size(lv_image_diamond_pickaxe, lv_pct(80),lv_pct(80));
    lv_obj_align(lv_image_diamond_pickaxe, LV_ALIGN_CENTER, 0, 0);

    lv_anim_t fade_in_anim;
    lv_anim_init(&fade_in_anim);
    lv_anim_set_duration(&fade_in_anim, 1000);
    lv_anim_set_var(&fade_in_anim, lv_image_diamond_pickaxe);
    lv_anim_set_values(&fade_in_anim,0, 255);
    lv_anim_set_path_cb(&fade_in_anim, lv_anim_path_ease_in_out);
    lv_anim_set_custom_exec_cb(&fade_in_anim, anim_cb_set_opa);

    lv_anim_start(&fade_in_anim);

    lv_obj_delete_async(anim->var);
}

void init_lvgl_scene(void){
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);
}

void enter_lvgl_scene(void){

    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* lv_image_logo = lv_image_create(lv_screen_active());
    lv_obj_set_style_image_opa(lv_image_logo, LV_OPA_TRANSP, 0);
    lv_image_set_src(lv_image_logo, &image_logo);
    lv_image_set_scale(lv_image_logo, (uint32_t) (256 * display_scale_factor));
    lv_obj_set_size(lv_image_logo, lv_pct(80),lv_pct(80));
    lv_obj_align(lv_image_logo, LV_ALIGN_CENTER, 0, 0);

    lv_anim_t intro_fade_in_anim;
    lv_anim_init(&intro_fade_in_anim);
    lv_anim_set_duration(&intro_fade_in_anim, 1000);
    lv_anim_set_var(&intro_fade_in_anim, lv_image_logo);
    lv_anim_set_values(&intro_fade_in_anim,0, 255);
    lv_anim_set_path_cb(&intro_fade_in_anim, lv_anim_path_ease_in_out);
    lv_anim_set_custom_exec_cb(&intro_fade_in_anim, anim_cb_set_opa);

    lv_anim_t intro_fade_out_anim;
    lv_anim_init(&intro_fade_out_anim);
    lv_anim_set_duration(&intro_fade_out_anim, 1000);
    lv_anim_set_var(&intro_fade_out_anim, lv_image_logo);
    lv_anim_set_values(&intro_fade_out_anim,255, 0);
    lv_anim_set_path_cb(&intro_fade_out_anim, lv_anim_path_ease_in_out);
    lv_anim_set_custom_exec_cb(&intro_fade_out_anim, anim_cb_set_opa);
    lv_anim_set_completed_cb(&intro_fade_out_anim, anim_intro_end);

    lv_anim_timeline_t* timeline = lv_anim_timeline_create();
    lv_anim_timeline_add(timeline, 10, &intro_fade_in_anim);
    lv_anim_timeline_add(timeline, 2000, &intro_fade_out_anim);

    lv_anim_timeline_start(timeline);
}

void app_main() {
    init_spi_bus();
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));
#ifdef USE_ST7735
    lvgl_main_display_handle = init_display_st7735(&main_lcd_panel_handle);
#elif USE_ST7789
    lvgl_main_display_handle = init_display_st7789(&main_lcd_panel_handle);
#endif

    display_resolution_width = lv_display_get_horizontal_resolution(lvgl_main_display_handle);
    display_resolution_height = lv_display_get_vertical_resolution(lvgl_main_display_handle);
    display_scale_factor = (float) display_resolution_height / DESIGN_RESOLUTION_HEIGHT;
    safe_area_width = (uint32_t) ((float) display_resolution_width * 0.8f);
    safe_area_height = (uint32_t) ((float) display_resolution_height * 0.8f);

    init_lvgl_scene();
    vTaskDelay(40 / portTICK_PERIOD_MS);
    esp_lcd_panel_disp_on_off(main_lcd_panel_handle, true);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    enter_lvgl_scene();

#if PERF_MEM_USAGE == 1
    while (1) {
        lv_mem_monitor_t lv_mem_info;
        lv_mem_monitor(&lv_mem_info);
        ESP_LOGI("perf", "heap: %lu | lvgl: %zu/%zu",
                 esp_get_free_heap_size(),
                 lv_mem_info.free_size,
                 lv_mem_info.total_size
        );
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
#endif
}
