# Hướng Dẫn Thay Đổi Màn Hình Chuẩn

Tài liệu này hướng dẫn cách thay đổi màn hình hiển thị trong dự án XiaoZhi AI, bao gồm: thay đổi độ phân giải, thay đổi driver màn hình (ST7789, ILI9341, ...), cấu hình các thông số màn hình (mirror, swap, offset), và chuyển đổi giữa các loại màn hình khác nhau (LCD, OLED, Emote).

---

## 1. Tổng Quan Về Hệ Thống Hiển Thị

### 1.1 Các Loại Màn Hình Được Hỗ Trợ

| Loại | Lớp | Mô tả |
|------|-----|-------|
| **SPI LCD** | `SpiLcdDisplay` | Màn hình LCD giao tiếp qua SPI (ST7789, ILI9341, ...) |
| **RGB LCD** | `RgbLcdDisplay` | Màn hình LCD giao tiếp qua RGB song song |
| **MIPI LCD** | `MipiLcdDisplay` | Màn hình LCD giao tiếp qua MIPI DSI |
| **OLED** | `OledDisplay` | Màn hình OLED đơn sắc 128x32, 128x64 |
| **Emote** | `EmoteDisplay` | Hiển thị emoji động (expression_emote) |
| **Không có màn hình** | `NoDisplay` | Board không có màn hình |

### 1.2 Kiến Trúc Phân Lớp

```
Display (lớp cơ sở)
  └── LvglDisplay (tích hợp LVGL)
        └── LcdDisplay (LCD)
              ├── SpiLcdDisplay  ← SPI LCD
              ├── RgbLcdDisplay  ← RGB LCD
              └── MipiLcdDisplay ← MIPI LCD
  └── OledDisplay (OLED)
  └── EmoteDisplay (Emote)
  └── NoDisplay (không màn hình)
```

### 1.3 Các Driver LCD Được Hỗ Trợ

- **ST7789** (SPI) - phổ biến nhất
- **ILI9341** (SPI) - dùng trong ESP-Box
- **GC9503** (RGB) - màn hình độ phân giải cao
- **SH8601** (QSPI)
- **ST7796**, **ST7701**, **GC9A01**, **ST7735** và nhiều loại khác

---

## 2. Cấu Hình Trong `config.h`

Mỗi board có file `config.h` chứa các macro cấu hình màn hình. Đây là nơi đầu tiên cần thay đổi khi đổi màn hình.

### 2.1 Thay Đổi Độ Phân Giải

```c
#define DISPLAY_WIDTH   320   // Chiều rộng pixel
#define DISPLAY_HEIGHT  240   // Chiều cao pixel
```

### 2.2 Cấu Hình Mirror (Lật Ảnh)

```c
#define DISPLAY_MIRROR_X true  // Lật ngang: true = lật, false = bình thường
#define DISPLAY_MIRROR_Y false // Lật dọc: true = lật, false = bình thường
```

### 2.3 Cấu Hình Swap XY (Xoay 90°)

```c
#define DISPLAY_SWAP_XY true  // true = xoay 90°, false = bình thường
```

> **Lưu ý**: Khi `DISPLAY_SWAP_XY = true`, chiều rộng và chiều cao sẽ bị đảo ngược.

### 2.4 Cấu Hình Offset (Dịch Chỉnh)

Dùng khi màn hình có vùng hiển thị không khớp với bộ nhớ:

```c
#define DISPLAY_OFFSET_X  0  // Dịch ngang (pixel)
#define DISPLAY_OFFSET_Y  0  // Dịch dọc (pixel)
```

### 2.5 Cấu Hình Chân SPI LCD

```c
#define DISPLAY_SPI_SCK_PIN   GPIO_NUM_xx  // Chân CLK
#define DISPLAY_SPI_MOSI_PIN  GPIO_NUM_xx  // Chân MOSI (DATA)
#define DISPLAY_DC_PIN        GPIO_NUM_xx  // Chân Data/Command
#define DISPLAY_SPI_CS_PIN    GPIO_NUM_xx  // Chân Chip Select
#define DISPLAY_SPI_RST_PIN   GPIO_NUM_xx  // Chân Reset (có thể là GPIO_NUM_NC)
```

### 2.6 Cấu Hình Backlight (Đèn nền)

```c
#define DISPLAY_BACKLIGHT_PIN            GPIO_NUM_xx
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true  // true = invert tín hiệu
```

---

## 3. Khởi Tạo Màn Hình Trong File Board `.cc`

### 3.1 SPI LCD (ST7789)

```cpp
void InitializeDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    // 1. Khởi tạo bus SPI
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. Cấu hình panel IO
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = 2;
    io_config.pclk_hz = 80 * 1000 * 1000;  // 80MHz
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

    // 3. Khởi tạo driver ST7789
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_SPI_RST_PIN;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

    // 4. Cấu hình màn hình
    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, true);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

    // 5. Tạo đối tượng display
    display_ = new SpiLcdDisplay(panel_io, panel,
                DISPLAY_WIDTH, DISPLAY_HEIGHT,
                DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
}
```

### 3.2 ILI9341 (ESP-Box)

ILI9341 có cách khởi tạo tương tự ST7789 nhưng cần thêm cấu hình vendor-specific init commands:

```cpp
#include "esp_lcd_ili9341.h"

// Các lệnh khởi tạo vendor-specific cho ILI9341
static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0xC8, (uint8_t[]){0xFF, 0x93, 0x42}, 3, 0},
    {0xC0, (uint8_t[]){0x0E, 0x0E}, 2, 0},
    {0xC5, (uint8_t[]){0xD0}, 1, 0},
    {0xB1, (uint8_t[]){0x00, 0x1B}, 2, 0},
    {0x36, (uint8_t[]){0x08}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0, (uint8_t[]){0}, 0xff, 0},
};

void InitializeIli9341Display() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = 0;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

    const ili9341_vendor_config_t vendor_config = {
        .init_cmds = vendor_specific_init,
        .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
    };

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_SPI_RST_PIN;
    panel_config.flags.reset_active_high = 0;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.vendor_config = (void *)&vendor_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_disp_on_off(panel, true);

    display_ = new SpiLcdDisplay(panel_io, panel, ...);
}
```

### 3.3 RGB LCD

Dùng cho màn hình RGB song song (thường là GC9503):

```cpp
void InitializeRgbDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    // Cấu hình RGB panel IO
    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.data_width = 16;
    panel_config.psram_trans_align = 64;
    panel_config.num_fbs = 2;
    panel_config.bounce_buffer_size_px = DISPLAY_WIDTH * 50;
    panel_config.clk_src = LCD_CLK_SRC_PLL160M;
    panel_config.disp_gpio_num = GPIO_NUM_NC;
    panel_config.data_gpio_nums = {
        GPIO_NUM_8, GPIO_NUM_3, GPIO_NUM_46, GPIO_NUM_9,  // R
        GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_47, GPIO_NUM_48, // G
        GPIO_NUM_45, GPIO_NUM_21, GPIO_NUM_1, GPIO_NUM_2,   // B
        GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40, GPIO_NUM_39, // R
    };
    panel_config.sync_gpio_num = GPIO_NUM_38;  // VSYNC
    panel_config.pclk_gpio_num = GPIO_NUM_7;    // PCLK
    panel_config.de_gpio_num = GPIO_NUM_6;      // DE
    panel_config.hsync_gpio_num = GPIO_NUM_5;   // HSYNC

    // Khởi tạo driver GC9503
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9503(panel_io, &panel_config, &panel));

    display_ = new RgbLcdDisplay(panel_io, panel, ...);
}
```

### 3.4 QSPI LCD

Dùng cho màn hình SPI 4-lane (WaveShare 1.85 inch):

```cpp
// Cấu hình QSPI trong config.h
#define QSPI_LCD_HOST           SPI2_HOST
#define QSPI_PIN_NUM_LCD_PCLK   GPIO_NUM_40
#define QSPI_PIN_NUM_LCD_CS     GPIO_NUM_21
#define QSPI_PIN_NUM_LCD_DATA0  GPIO_NUM_46
#define QSPI_PIN_NUM_LCD_DATA1  GPIO_NUM_45
#define QSPI_PIN_NUM_LCD_DATA2  GPIO_NUM_42
#define QSPI_PIN_NUM_LCD_DATA3  GPIO_NUM_41

void InitializeQspiDisplay() {
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = QSPI_PIN_NUM_LCD_PCLK;
    buscfg.data4_io_num = QSPI_PIN_NUM_LCD_DATA0;  // 4-lane data
    buscfg.max_transfer_sz = QSPI_LCD_H_RES * QSPI_LCD_V_RES * 2;
    ESP_ERROR_CHECK(spi_bus_initialize(QSPI_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = QSPI_PIN_NUM_LCD_RST;
    panel_config.bits_per_pixel = QSPI_LCD_BIT_PER_PIXEL;
    // ... tạo panel và display
}
```

### 3.5 OLED Display

```cpp
#include "display/oled_display.h"

void InitializeOledDisplay() {
    display_ = new OledDisplay(DISPLAY_WIDTH, DISPLAY_HEIGHT);
}
```

### 3.6 Emote Display

```cpp
#include "display/emote_display.h"

void InitializeEmoteDisplay() {
    display_ = new emote::EmoteDisplay(panel, panel_io, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}
```

### 3.7 Không Có Màn Hình

```cpp
display_ = new NoDisplay();
```

---

## 4. Đăng Ký Display Trong Board Class

Sau khi khởi tạo, cần override phương thức `GetDisplay()` trong class board:

```cpp
class MyBoard : public WifiBoard {
private:
    LcdDisplay* display_;

public:
    MyBoard() {
        InitializeDisplay();
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(MyBoard);
```

---

## 5. Cấu Hình Font Trong `CMakeLists.txt`

Khi thay đổi độ phân giải màn hình, cần chọn kích thước font phù hợp trong `main/CMakeLists.txt`:

```cmake
elseif(CONFIG_BOARD_TYPE_MY_BOARD)
    set(BOARD_TYPE "my-board")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)    # Font chữ
    set(BUILTIN_ICON_FONT font_awesome_20_4)        # Font icon
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)         # Bộ emoji (32 hoặc 64)
```

### Hướng dẫn chọn font theo độ phân giải:

| Độ phân giải | Font text | Font icon | Emoji |
|-------------|-----------|-----------|-------|
| 128x64 (OLED nhỏ) | `font_puhui_basic_14_1` | `font_awesome_14_1` | `twemoji_32` |
| 240x240 | `font_puhui_basic_16_4` | `font_awesome_16_4` | `twemoji_32` |
| 240x320 | `font_puhui_basic_20_4` | `font_awesome_20_4` | `twemoji_64` |
| 320x480 | `font_puhui_basic_24_4` | `font_awesome_24_4` | `twemoji_64` |
| 480x320+ | `font_puhui_basic_30_4` | `font_awesome_30_4` | `twemoji_64` |

---

## 6. Thêm Driver LCD Mới

### 6.1 Thêm Kconfig Entry

Trong `main/Kconfig.projbuild`:

```kconfig
config BOARD_TYPE_MY_BOARD
    bool "My Custom Board"
    depends on IDF_TARGET_ESP32S3
```

### 6.2 Thêm Build Configuration

Trong `main/CMakeLists.txt`:

```cmake
elseif(CONFIG_BOARD_TYPE_MY_BOARD)
    set(BOARD_TYPE "my-board")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
```

### 6.3 Các Hàm ESP-LCD Thường Dùng

```cpp
// Reset màn hình
esp_lcd_panel_reset(panel);

// Khởi tạo panel
esp_lcd_panel_init(panel);

// Đảo màu (tùy driver)
esp_lcd_panel_invert_color(panel, true);

// Hoán đổi X/Y (xoay 90°)
esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);

// Lật ảnh
esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

// Bật/tắt hiển thị
esp_lcd_panel_disp_on_off(panel, true);

// Thiết lập offset
esp_lcd_panel_set_gap(panel, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
```

---

## 7. Ví Dụ Thực Tế: Thay Đổi Từ ST7789 Sang ILI9341

### Bước 1: Thay đổi `config.h`

```c
// Thêm chân reset nếu chưa có
#define DISPLAY_SPI_RST_PIN   GPIO_NUM_48

// Giữ nguyên các thông số khác
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
```

### Bước 2: Thay đổi file `.cc`

```cpp
#include "esp_lcd_ili9341.h"

// Thêm vendor-specific init commands cho ILI9341
static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0xC8, (uint8_t[]){0xFF, 0x93, 0x42}, 3, 0},
    {0xC0, (uint8_t[]){0x0E, 0x0E}, 2, 0},
    // ... các lệnh khởi tạo khác
};

// Thay đổi hàm khởi tạo
void InitializeIli9341Display() {
    // ... SPI bus init ...

    const ili9341_vendor_config_t vendor_config = {
        .init_cmds = vendor_specific_init,
        .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
    };

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.vendor_config = (void *)&vendor_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

    // ... các bước còn lại giữ nguyên ...
}
```

---

## 8. Ví Dụ: Thay Đổi Độ Phân Giải

### Từ 240x240 lên 320x240

**Bước 1: Sửa `config.h`**

```c
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
// Có thể cần điều chỉnh offset
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
```

**Bước 2: Sửa `CMakeLists.txt`**

```cmake
set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)  // Tăng kích thước font
```

**Bước 3: Điều chỉnh buffer SPI**

```cpp
buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
```

---

## 9. Xử Lý Lỗi Thường Gặp

### 9.1 Màn Hình Đen (Không Hiển Thị Gì)

Đây là lỗi phổ biến nhất. Thứ tự kiểm tra:

**Bước 1: Kiểm tra GPIO**
Mỗi board có GPIO khác nhau. Luôn đối chiếu với board cùng loại đã hoạt động. Ví dụ thực tế - board Otto-Robot dùng GPIO khác với xingzhi-cube:

| Chân | Otto-Robot (hoạt động) | xingzhi-cube (sai) |
|------|------------------------|-------------------|
| MOSI (SDA) | GPIO10 | GPIO10 ✓ |
| CLK (SCL) | GPIO9 | GPIO9 ✓ |
| DC | **GPIO46** | GPIO8 ❌ |
| CS | **GPIO12** | GPIO14 ❌ |
| RST | **GPIO11** | GPIO18 ❌ |
| BL | **GPIO3** | GPIO13 ❌ |

> **Quy tắc**: Khi chuyển sang board mới, luôn đối chiếu GPIO với board cùng phần cứng đã hoạt động.

**Bước 2: Kiểm tra SPI Mode**

| Driver | SPI Mode thường dùng |
|--------|---------------------|
| ST7789 | **Mode 3** (CPOL=1, CPHA=1) |
| ST7789 (7-pin) | Mode 2 |
| ILI9341 | Mode 0 |
| GC9A01 | Mode 0 |

```c
#define DISPLAY_SPI_MODE 3  // ST7789 thường dùng mode 3
```

**Bước 3: Kiểm tra INVERT_COLOR**

Có 2 loại màn hình: IPS và Non-IPS, cần giá trị khác nhau:

| Loại màn hình | INVERT_COLOR |
|--------------|-------------|
| IPS (phổ biến) | `true` |
| Non-IPS | `false` |

```c
#define DISPLAY_INVERT_COLOR    true   // IPS
// hoặc
#define DISPLAY_INVERT_COLOR    false  // Non-IPS
```

> **Mẹo**: Nếu màn hình đen, thử đổi ngược giá trị INVERT_COLOR.

**Bước 4: Kiểm tra Backlight**

```c
// Đảm bảo backlight được khởi tạo đúng
GetBacklight()->RestoreBrightness();

// Nếu đèn nền không sáng, thử đổi invert
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true
```

### 9.2 Màn Hình Bị Lộn Ngược/Lệch

```c
// Trong config.h, điều chỉnh:
#define DISPLAY_MIRROR_X true   // Thử true/false
#define DISPLAY_MIRROR_Y true   // Thử true/false
#define DISPLAY_SWAP_XY  true   // Thử true/false nếu xoay 90°
```

### 9.3 Màn Hình Bị Dịch Chỉnh (có vùng đen)

```c
#define DISPLAY_OFFSET_X  0   // Thử giá trị khác (0, 20, 40, ...)
#define DISPLAY_OFFSET_Y  0   // Thử giá trị khác
```

### 9.4 Màu Sắc Bị Đảo Ngược

```cpp
// Trong hàm khởi tạo:
esp_lcd_panel_invert_color(panel, false);  // Đổi true/false
```

### 9.5 Font Bị Vỡ/Hình Ảnh Không Đúng

Kiểm tra và cập nhật `CMakeLists.txt`:

```cmake
set(BUILTIN_TEXT_FONT font_puhui_basic_XX_Y)   // Chọn kích thước phù hợp
set(DEFAULT_EMOJI_COLLECTION twemoji_XX)       // Chọn 32 hoặc 64
```

---

## 10. Tham Khảo GPIO Các Board

### 10.1 Otto-Robot (Non-Camera)

| Chức năng | GPIO |
|-----------|------|
| MOSI | GPIO10 |
| CLK | GPIO9 |
| DC | GPIO46 |
| CS | GPIO12 |
| RST | GPIO11 |
| Backlight | GPIO3 |
| Boot Button | GPIO0 |
| Mic WS | GPIO4 |
| Mic SCK | GPIO5 |
| Mic DIN | GPIO6 |
| SPK DOUT | GPIO7 |
| SPK BCLK | GPIO15 |
| SPK LRCK | GPIO16 |
| Charge Detect | GPIO21 |
| Volume Up | GPIO40 |
| Volume Down | GPIO39 |

### 10.2 Thông Số ST7789 của Otto-Robot

```c
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_RGB_ORDER LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 3
```

### 10.3 Quy Trình Debug Màn Hình

1. **Đối chiếu GPIO** với board cùng phần cứng
2. **Thử SPI Mode** (0, 2, 3)
3. **Đổi INVERT_COLOR** (true ↔ false)
4. **Kiểm tra Backlight** (GPIO và invert)
5. **Rebuild sạch**: `idf.py fullclean && idf.py build`

---

## 11. Bảng Tra Cứu Nhanh

### Các Macro Cấu Hình

| Macro | Mô tả | Ví dụ |
|-------|-------|-------|
| `DISPLAY_WIDTH` | Chiều rộng pixel | `320` |
| `DISPLAY_HEIGHT` | Chiều cao pixel | `240` |
| `DISPLAY_MIRROR_X` | Lật ngang | `true/false` |
| `DISPLAY_MIRROR_Y` | Lật dọc | `true/false` |
| `DISPLAY_SWAP_XY` | Hoán đổi X/Y | `true/false` |
| `DISPLAY_OFFSET_X` | Dịch ngang | `0` |
| `DISPLAY_OFFSET_Y` | Dịch dọc | `0` |
| `DISPLAY_BACKLIGHT_PIN` | Chân đèn nền | `GPIO_NUM_2` |
| `DISPLAY_BACKLIGHT_OUTPUT_INVERT` | Đảo tín hiệu BL | `true/false` |

### Các Driver LCD

| Driver | Giao tiếp | Hàm khởi tạo |
|--------|-----------|--------------|
| ST7789 | SPI | `esp_lcd_new_panel_st7789()` |
| ILI9341 | SPI | `esp_lcd_new_panel_ili9341()` |
| GC9503 | RGB | `esp_lcd_new_panel_gc9503()` |
| ST7701 | RGB/MIPI | `esp_lcd_new_panel_st7701()` |
| GC9A01 | SPI | `esp_lcd_new_panel_gc9a01()` |
| ST7735 | SPI | `esp_lcd_new_panel_st7735()` |

### Các Lớp Display

| Lớp | Mục đích |
|-----|----------|
| `Display` | Lớp cơ sở |
| `LvglDisplay` | Tích hợp LVGL |
| `LcdDisplay` | LCD LVGL |
| `SpiLcdDisplay` | LCD SPI |
| `RgbLcdDisplay` | LCD RGB |
| `MipiLcdDisplay` | LCD MIPI |
| `OledDisplay` | OLED LVGL |
| `EmoteDisplay` | Hiển thị emoji |
| `NoDisplay` | Không có màn hình |

---

## 12. Tài Nguyên Tham Khảo

- [ESP-LCD Documentation](https://docs.espressif.com/projects/esp-idf/)
- [LVGL Documentation](https://docs.lvgl.io/)
- [Custom Board Guide](./custom-board.md)
- [MCP Usage](./mcp-usage.md)
