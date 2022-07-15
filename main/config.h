#ifndef __CONFIG_H__
#define __CONFIG_H__
#define __TEST_MODE__	1
#define __USE_SPIFS__	0

#if(__TEST_MODE__)
#define _PIN_MAINSW GPIO_NUM_34
#define _PIN_SW1_   GPIO_NUM_35
#define _PIN_SW2_   GPIO_NUM_27
#define	_PIN_DCPULS GPIO_NUM_17
#define	_PIN_ACPULS GPIO_NUM_16
// ここから下は液晶用
#define _PIN_CS 	-1		// 使わない
#define _PIN_DC		GPIO_NUM_26
#define _PIN_RESET	GPIO_NUM_25
#define _PIN_MISO	-1		// 使わない
#define _PIN_MOSI	GPIO_NUM_33
#define _PIN_SCLK	GPIO_NUM_32
#endif

#define LCD_0P96INCH 0
#define LCD_1P30INCH 1
#ifdef LCD_0P96INCH
#define _CONFIG_WIDTH  80
#define _CONFIG_HEIGHT 160
#define _CONFIG_INVERSION 1
#define _CONFIG_OFFSETX 25
#define _CONFIG_OFFSETY 1
#define _CONFIG_FMEM_WIDTH	132
#define _CONFIG_FMEM_HEIGHT	162
#endif
#ifdef LCD_1P30INCH
#define _CONFIG_WIDTH  240
#define _CONFIG_HEIGHT 240
#define _CONFIG_INVERSION 1
#define _CONFIG_OFFSETX 25
#define _CONFIG_OFFSETY 1
#define _CONFIG_FMEM_WIDTH	240
#define _CONFIG_FMEM_HEIGHT	320
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