#ifndef __MAIN_H__
#define __MAIN_H__
#include "config.h"
#include "esp_system.h"
#include "freertos/timers.h"
#include "hal/spi_types.h"
#include "misc.h"

#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------
// 	メインクラス
//------------------------------------------------------
class EspRpmMeter {
friend class EspRpmMeter;
public:
	EspRpmMeter();
	void  main();
	static EspRpmMeter* getInstance() {  return pThis; }
protected:
	static EspRpmMeter* pThis;				// 一部のESP関数はパラメータを渡せないので、参照用に
	SETTINGD	_settingd,
				_backup;
	uint8_t		mainsw;
	uint8_t		sw1, 				// スイッチが押されているかどうかのフラグ
				sw2, 
				sw3;				// 同時押しの場合こちら
	uint8_t		_st1_request, 		// 設定モードへの変更要求フラグ
				_st2_request;
	uint8_t		_curMode;
	uint32_t	_initJSVval,		// JoyStick 垂直方向AD変換初期値(mv)
				_initJSHval,
				_tickCnt4Plus,
				_lastTickCnt4Plus,
				_tickCntSav;
	GPIODATA	_gpdt_mainsw;
	GPIODATA	_gpdt_sw1;
	GPIODATA	_gpdt_sw2;
	GPIODATA	_gpdt_acpulse;
	GPIODATA	_gpdt_dcpulse;
	QueueHandle_t  portRqQue;
	QueueHandle_t  ssQue;
	void *		_pLcdTask;
	bool		_is_intest;
	TaskHandle_t _ht_Test;

protected:
	int loadSettingData();
	int saveSettingData();
	void SaveSettingTask();
	void MainTask();
	void gpio_setting();
	void TestTask();
	void pwm_setting();
	int ConnectToServer();
	void PulseCallbackImpl(GPIODATA*);
	static void PulseCallback(GPIODATA*);
	static void SaveSettingTask(void *pvParameters);
	static void MainTask(void *pvParameters);
	static void TestTask(void *);
};

#ifdef __cplusplus
}
#endif


#endif