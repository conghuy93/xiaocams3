
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

// ============================================================
// DISPLAY GPIO CONFIG - Cấu hình theo Otto-Robot (Non-Camera)
// ============================================================
#ifndef DISPLAY_SDA
#define DISPLAY_SDA GPIO_NUM_10
#endif
#ifndef DISPLAY_SCL
#define DISPLAY_SCL GPIO_NUM_9
#endif
#ifndef DISPLAY_DC
#define DISPLAY_DC GPIO_NUM_46   // Otto: GPIO46
#endif
#ifndef DISPLAY_CS
#define DISPLAY_CS GPIO_NUM_12   // Otto: GPIO12
#endif
#ifndef DISPLAY_RES
#define DISPLAY_RES GPIO_NUM_11  // Otto: GPIO11
#endif
#ifndef DISPLAY_BACKLIGHT_PIN
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_3  // Otto: GPIO3
#endif

// ============================================================
// DISPLAY PARAMETERS - linh hoạt như bread-compact
// ============================================================
// Nếu chưa define, dùng ST7789 240x240 mặc định (giữ nguyên tương thích)

#ifdef CONFIG_LCD_ST7789_240X240
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true   // Otto dùng true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 3   // Otto dùng mode 3
#endif

#elif defined(CONFIG_LCD_ST7789_240X240_7PIN)
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 2
#endif

#elif defined(CONFIG_LCD_ST7789_240X320)
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 0
#endif

#elif defined(CONFIG_LCD_ST7789_240X280)
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  280
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  20
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 0
#endif

#elif defined(CONFIG_LCD_ST7789_240X135)
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  135
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  40
#define DISPLAY_OFFSET_Y  53
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 0
#endif

#elif defined(CONFIG_LCD_ILI9341_240X320)
#define LCD_TYPE_ILI9341_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 0
#endif

#elif defined(CONFIG_LCD_GC9A01_240X240)
#define LCD_TYPE_GC9A01_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 0
#endif

#elif defined(CONFIG_LCD_ST7735_128X160)
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  160
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    false
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 0
#endif

#else
// ============================================================
// DEFAULT: ST7789 240x240 - cấu hình Otto-Robot
// ============================================================
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_INVERT_COLOR    true   // Otto dùng true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#ifndef DISPLAY_SPI_MODE
#define DISPLAY_SPI_MODE 3   // Otto dùng mode 3
#endif
#ifndef BACKLIGHT_INVERT
#define BACKLIGHT_INVERT false
#endif

#endif // LCD type configs

// Backlight invert (dùng cho cả default và config)
#ifndef BACKLIGHT_INVERT
#define BACKLIGHT_INVERT false
#endif

#endif // _BOARD_CONFIG_H_
