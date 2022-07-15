#ifndef LOVYANGFX_MY_CONFIG_HPP_
#define LOVYANGFX_MY_CONFIG_HPP_
#include "config.h"

namespace lgfx
{
// for ESP32
  struct LGFX_Config
  {
// 使用するSPIを VSPI_HOST または HSPI_HOST で設定します。
    static constexpr spi_host_device_t spi_host = VSPI_HOST;

// 使用するDMAチャンネルを 1か2で設定します。
// 使用しない場合は省略するか0を設定します。
    static constexpr int dma_channel = 1;

// SPIのSCLKのピン番号を設定します。
    static constexpr int spi_sclk = _PIN_SCLK;

// SPIのMOSIのピン番号を設定します。
    static constexpr int spi_mosi = _PIN_MOSI;

// SPIのMISOのピン番号を設定します。
// SDカード等と共通のSPIバスを使う場合はMISOも必ず設定してください。
// 使わない場合は省略するか-1を設定します。
    static constexpr int spi_miso = -1;

// SPI通信のデータ長を指定します。
// RaspberryPi用のLCD等を使用する場合に16を指定します。
// 省略時は 8 です。大抵のパネルは8ですので、基本的には省略してください。
    static constexpr int spi_dlen = 8;
  };


/* 
// for SAMD51
  struct LGFX_Config
  {
// 使用するSPIのSERCOM番号を設定します。
    static constexpr int sercom_index = 7;

// SERCOMのクロックソースを設定します。
// -1を指定した場合、クロックソースを設定せずに動作しますので別途設定を行ってください。
    static constexpr int sercom_clksrc = 0;   // -1=notchange / 0=select GCLK0

// 上記で設定したクロックソースの動作周波数を設定します。
// Harmony等で行った設定値をそのまま設定してください。
    static constexpr int sercom_clkfreq = 120000000;

// SPIのSCLKのピン番号を設定します。 PORTA=0x000 / PORTB=0x100 / PORTC=0x200 / PORTD=0x300…
    static constexpr int spi_sclk = 0x0100 | 20; // PORTB 20 (PORTB=0x0100)

// SPIのMOSIのピン番号を設定します。
    static constexpr int spi_mosi = 0x0100 | 19; // PORTB 19 (PORTB=0x0100)

// SPIのMISOのピン番号を設定します。
    static constexpr int spi_miso = 0x0100 | 18; // PORTB 18 (PORTB=0x0100)

// SPIで使用するTX Padを設定します。
    static constexpr SercomSpiTXPad pad_mosi = SPI_PAD_3_SCK_1;  // PAD_SPI3_TX;

// SPIで使用するRX Padを設定します。
    static constexpr SercomRXPad    pad_miso = SERCOM_RX_PAD_2;  // PAD_SPI3_RX;
  };
//*/

}

class LGFX : public lgfx::LGFX_SPI<lgfx::LGFX_Config>
{
// Panelクラスのインスタンスを作成します。使用するパネルにあったクラスをコメントアウトしてください。

//  ★LCD一体型製品の対応機種の場合はこちらから選択できます。
//  lgfx::Panel_DDUINO32_XS panel;
//  lgfx::Panel_LoLinD32 panel;
//  lgfx::Panel_M5Stack panel;
//  lgfx::Panel_M5StickC panel;
//  lgfx::Panel_ODROID_GO panel;
//  lgfx::Panel_TTGO_TS panel;
//  lgfx::Panel_TTGO_TWatch panel;
//  lgfx::Panel_WioTerminal panel;

//  ★対応機種以外の場合はこちらから選択できます。
//  lgfx::Panel_HX8357B panel;
//  lgfx::Panel_HX8357D panel;
//  lgfx::Panel_ILI9163 panel;
//  lgfx::Panel_ILI9341 panel;
//  lgfx::Panel_ILI9342 panel;
//  lgfx::Panel_ILI9486 panel;
//  lgfx::Panel_ILI9488 panel;
//  lgfx::Panel_SSD1351 panel;
//  lgfx::Panel_ST7789 panel;
  lgfx::Panel_ST7735S panel;

public:
  LGFX(void) : lgfx::LGFX_SPI<lgfx::LGFX_Config>()
  {
// パネルクラスに各種設定値を代入していきます。
// （LCD一体型製品のパネルクラスを選択した場合は、
//   製品に合った初期値が設定されているので設定は不要です）

// 通常動作時のSPIクロックを設定します。
// ESP32のSPIは80MHzを整数で割った値のみ使用可能です。
// 設定した値に一番近い設定可能な値が使用されます。
    panel.freq_write = 20000000;

// 単色の塗り潰し処理時のSPIクロックを設定します。
// 基本的にはfreq_writeと同じ値を設定しますが、
// より高い値を設定しても動作する場合があります。
    panel.freq_fill  = 20000000;

// LCDから画素データを読取る際のSPIクロックを設定します。
    panel.freq_read  = 16000000;

// SPI通信モードを0~3から設定します。
    panel.spi_mode = 0;

// データ読み取り時のSPI通信モードを0~3から設定します。
    panel.spi_mode_read = 0;

// 画素読出し時のダミービット数を設定します。
// 画素読出しでビットずれが起きる場合に調整してください。
    panel.len_dummy_read_pixel = 8;

// データの読取りが可能なパネルの場合はtrueを、不可の場合はfalseを設定します。
// （例：CSピンのないST7789等は読出し不可のためfalseを設定します。）
// 省略時はtrueになります。
    panel.spi_read = false;

// データの読取りMOSIピンで行うパネルの場合はtrueを設定します。
// （例：M5StackやT-Watchはtrue、ESP-WROVER-KITはfalseを設定します。）
// 省略時はfalseになります。
    panel.spi_3wire = false;

// LCDのCSを接続したピン番号を設定します。
// 使わない場合は省略するか-1を設定します。
    panel.spi_cs = _PIN_CS;

// LCDのDCを接続したピン番号を設定します。
    panel.spi_dc = _PIN_DC;

// LCDのRSTを接続したピン番号を設定します。
// 使わない場合は省略するか-1を設定します。
    panel.gpio_rst = _PIN_RESET;

// LCDのバックライトを接続したピン番号を設定します。
// 使わない場合は省略するか-1を設定します。
    panel.gpio_bl  = -1;

// バックライト使用時、輝度制御に使用するPWMチャンネル番号を設定します。
// PWM輝度制御を使わない場合は省略するか-1を設定します。
    panel.pwm_ch_bl = -1;

// バックライト点灯時の出力レベルがローかハイかを設定します。
// 省略時は true。true=HIGHで点灯 / false=LOWで点灯になります。
    panel.backlight_level = true;

// invertDisplayの初期値を設定します。trueを設定すると反転します。
// 省略時は false。画面の輝度が反転している場合は設定を変更してください。
    panel.invert = true;

// パネルの色順がを設定します。  RGB=true / BGR=false
// 省略時はfalse。赤と青が入れ替わっている場合は設定を変更してください。
    panel.rgb_order = false;

// LCDコントローラのメモリ上のピクセル数（幅と高さ）を設定します。
// 設定が合っていない場合、setRotationを使用した際の座標がずれます。
// （例：ST7735は 132x162 / 128x160 / 132x132 の３通りが存在します）
    panel.memory_width  = 132;
    panel.memory_height = 162;

// パネルの実際に表示可能なピクセル数（幅と高さ）を設定します。
// 省略時はパネルクラスのデフォルト値が使用されます。
    panel.panel_width  = _CONFIG_WIDTH;
    panel.panel_height = _CONFIG_HEIGHT;

// パネルのオフセット量を設定します。
// 省略時はパネルクラスのデフォルト値が使用されます。
    panel.offset_x = _CONFIG_OFFSETX;
    panel.offset_y = _CONFIG_OFFSETY;

// setRotationの初期化直後の値を設定します。
    panel.rotation = 0;

// setRotationを使用した時の向きを変更したい場合、offset_rotationを設定します。
// setRotation(0)での向きを 1の時の向きにしたい場合、 1を設定します。
    panel.offset_rotation = 0;



// 最後に、用意したパネル設定をセットします。
    setPanel(&panel);
  }
};

#endif
