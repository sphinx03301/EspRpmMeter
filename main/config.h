#ifndef __CONFIG_H__
#define __CONFIG_H__
#define __TEST_MODE__	1
#define __USE_SPIFS__	0

#if(__TEST_MODE__)
#define _PIN_TESTSW GPIO_NUM_0	// テスト用のボタンとする
#define _PIN_MAINSW GPIO_NUM_14
#define _PIN_SW1_   GPIO_NUM_35
#define _PIN_SW2_   GPIO_NUM_27
#define	_PIN_DCPULS GPIO_NUM_17
#define	_PIN_ACPULS GPIO_NUM_13
// ここから下は液晶用 (from here, pin for TFT)
#define _PIN_DC		GPIO_NUM_26
#define _PIN_RESET	GPIO_NUM_25
#define _PIN_MISO	-1		// 使わない
#define _PIN_MOSI	GPIO_NUM_33
#define _PIN_SCLK	GPIO_NUM_32
#else
// for not test
#endif

// タコメータに関する設定( setting for rpm meters)
#define _MAX_RPM	100
#define _SANGLE		225
#define _EANGLE		-45
#define _DCPCNT		7		// クランク角度センの1回転あたり信号数(HONDAは7)
#define _ACPCNT		1		// クランクセンサの1回転当たりの信号数(通常1)


#define LCD_0P96INCH 	96 	// ST7765の液晶
#define LCD_1P30INCH	130	// ST7789の液晶
#define MY_LCD		LCD_1P30INCH

#if MY_LCD == LCD_0P96INCH
#define _CONFIG_WIDTH  80
#define _CONFIG_HEIGHT 160
#define _CONFIG_INVERSION 1
#define _CONFIG_OFFSETX 25
#define _CONFIG_OFFSETY 1
#define _CONFIG_FMEM_WIDTH	132
#define _CONFIG_FMEM_HEIGHT	162
#define _CONFIG_SPIMODE	0
#elif MY_LCD == LCD_1P30INCH
#define _CONFIG_WIDTH  240
#define _CONFIG_HEIGHT 240
#define _CONFIG_INVERSION 1
#define _CONFIG_OFFSETX 25
#define _CONFIG_OFFSETY 1
#define _CONFIG_FMEM_WIDTH	240
//#define _CONFIG_FMEM_HEIGHT	240
#define _PIN_CS 	-1		// 使わない
#define _CONFIG_FMEM_HEIGHT	320
#define _CONFIG_SPIMODE	3
#endif

#define DEBUG_MAIN  1
#define DEBUG_LCD   0

#if (__USE_SPIFS__)
#define GSZSETTINGFILE  "/spiffs/savedata.dat"
#endif
#define SETTING1WAIT	5000		// 長押し時間１
#define COM_TIMEOUT		5000		// 通信が切断されたとみなす時間(ms)
#define ALIVE_INTERVAL	3000		// 生存確認用電文送信間隔
#define SETTING2WAIT	6000		// 長押し時間２

#define SERVERPORT 5055

#endif