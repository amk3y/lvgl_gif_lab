//
// Created by amk3y on 2024/12/6.
//

#include "cfg_st7789.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"

lv_disp_t* init_display_st7789(esp_lcd_panel_handle_t* ret_lcd_handle){
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


    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, ret_lcd_handle));


    esp_lcd_panel_reset(*ret_lcd_handle);
    esp_lcd_panel_init(*ret_lcd_handle);
    // turn off display before first lvgl update to avoid flickering
    esp_lcd_panel_disp_on_off(*ret_lcd_handle, false);
    // depend on hardware
    esp_lcd_panel_invert_color(*ret_lcd_handle, true);
    esp_lcd_panel_set_gap(*ret_lcd_handle, 0, 80);

    /* Add LCD screen */
    const lvgl_port_display_cfg_t disp_cfg = {
            .io_handle = io_handle,
            .panel_handle = *ret_lcd_handle,
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

    return lvgl_port_add_disp(&disp_cfg);
}