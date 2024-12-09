//
// Created by amk3y on 2024/12/6.
//

#ifndef LVGL_GIF_LAB_CFG_ST7789_H
#define LVGL_GIF_LAB_CFG_ST7789_H

#include "lvgl.h"
#include "esp_lcd_types.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"

#define DISP_WIDTH 240
#define DISP_HEIGHT 240
#define DISP_DRAW_BUFFER_HEIGHT 40
#define DISP_GPIO_RES GPIO_NUM_0
#define DISP_GPIO_DC GPIO_NUM_1


lv_disp_t* init_display_st7789(esp_lcd_panel_handle_t* ret_lcd_handle);



#endif //LVGL_GIF_LAB_CFG_ST7789_H
