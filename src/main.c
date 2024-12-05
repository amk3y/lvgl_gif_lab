#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"

#include "esp_log.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"

#include "math.h"

#define MATH_PI 3.1415926

#define DISP_WIDTH 240
#define DISP_HEIGHT 240
#define DISP_DRAW_BUFFER_HEIGHT 40
#define DISP_GPIO_RES GPIO_NUM_0
#define DISP_GPIO_DC GPIO_NUM_1
#define DISP_SCLK GPIO_NUM_4
#define DISP_MOSI GPIO_NUM_6

static esp_lcd_panel_handle_t main_lcd_panel_handle;
static lv_disp_t* lvgl_main_display_handle;

static lv_obj_t* img;

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


void init_lvgl_disp(){
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    /* LCD IO */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num = DISP_GPIO_DC,
            .cs_gpio_num = GPIO_NUM_NC,
            .pclk_hz = 40 * 1000 * 1000,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            // this varies depending on hardware
            // bugfix ref: https://github.com/Bodmer/TFT_eSPI/issues/163
            .spi_mode = 3,
            .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) SPI2_HOST, &io_config, &io_handle));

    /* LCD driver initialization */

    const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = DISP_GPIO_RES,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &main_lcd_panel_handle));

    esp_lcd_panel_reset(main_lcd_panel_handle);
    esp_lcd_panel_init(main_lcd_panel_handle);
    // turn off display before first lvgl update to avoid flickering
    esp_lcd_panel_disp_on_off(main_lcd_panel_handle, false);
    // depend on hardware
    esp_lcd_panel_invert_color(main_lcd_panel_handle, true);
    esp_lcd_panel_set_gap(main_lcd_panel_handle, 0, 80);

    /* Add LCD screen */
    const lvgl_port_display_cfg_t disp_cfg = {
            .io_handle = io_handle,
            .panel_handle = main_lcd_panel_handle,
            .buffer_size = DISP_HEIGHT * DISP_DRAW_BUFFER_HEIGHT * sizeof(uint16_t),
            .double_buffer = true,
            .hres = DISP_WIDTH,
            .vres = DISP_HEIGHT,
            .monochrome = false,
            .color_format = LV_COLOR_FORMAT_RGB565,
            .rotation = {
                    .swap_xy = false,
                    .mirror_x = true,
                    .mirror_y = true,
            },
            .flags = {
                    .buff_dma = true,
                    .swap_bytes = true,
            }
    };

    lvgl_main_display_handle = lvgl_port_add_disp(&disp_cfg);
}

void init_lvgl_scene(void){
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);

    LV_IMAGE_DECLARE(image);
    img = lv_gif_create(lv_screen_active());
    lv_gif_set_src(img, &image);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}

float curve_ease_in_out_sine(float x){
    return (float) -(cos(MATH_PI * x) - 1) / 2;
}

_Noreturn void task_animate_image(void* pvParameters){
    uint32_t time = 0;
    uint32_t duration = 150;
    float half_duration = (float) duration / 2.0f;
    float offset = 0;

    while (1){
        vTaskDelay(10 / portTICK_PERIOD_MS);
        offset = 48 * curve_ease_in_out_sine((float) time / half_duration);
        lv_obj_set_y(img, 32 - (uint32_t) offset);
        if (time++ >= duration){
            time = 0;
        }
    }
}

void app_main() {

    init_spi_bus();
    init_lvgl_disp();
    init_lvgl_scene();

    vTaskDelay(40 / portTICK_PERIOD_MS);
    esp_lcd_panel_disp_on_off(main_lcd_panel_handle, true);

    xTaskCreate(&task_animate_image, "task_animate_image", 2048, NULL, 2, NULL);
}
