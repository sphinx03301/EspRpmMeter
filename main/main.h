#ifndef __MAIN_H__
#define __MAIN_H__
#include "config.h"
#include "esp_system.h"
#include "freertos/timers.h"
#include "hal/spi_types.h"
#define LOVYANGFX_CONFIG_HPP_
#include "LovyanGFX.hpp"
#include "LGFX_Config_MyCustom.hpp"
#include "misc.h"

#ifdef __cplusplus
extern "C" {
#endif
//------------------------------------------------------
// 	メインクラス
//------------------------------------------------------
class EspEscCtrlr {
friend  EspEscCtrlr;
public:
	EspEscCtrlr();
	static void SaveSettingTask(void *pvParameters);
	static void LcdTask(void *pvParameters);
	static void MainTask(void *pvParameters);
	void  main();
	static EspEscCtrlr* getInstance() {  return pThis; }
protected:
	static EspEscCtrlr* pThis;				// 一部のESP関数はパラメータを渡せないので、参照用に
	SETTINGD	_settingd,
				_backup;
	uint8_t		mainsw;
	uint8_t		sw1, 				// スイッチが押されているかどうかのフラグ
				sw2, 
				sw3;				// 同時押しの場合こちら
	uint8_t		_st1_request, 		// 設定モードへの変更要求フラグ
				_st2_request,
				_wiper_status, _wiper_stop;
	uint8_t		_curMode;
	SemaphoreHandle_t hsemwifi;
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
	QueueHandle_t  dispQue;
	QueueHandle_t  ssQue;
	LGFX 		*_plcd;
	TimerHandle_t _th_wiper;
	StaticTimer_t _tb_wiper;
	WifiHandler	*pwifi;
protected:
	int loadSettingData();
	int saveSettingData();
	void SaveSettingTask();
	void LcdTask();
	void MainTask();
	void gpio_setting();
	void pwm_setting();
	int ConnectToServer();
	void WiperLogic(int state);
	void ResetWiper();
	static void WiperCallback( TimerHandle_t );
	void PulseCallbackImpl(GPIODATA*);
	static void PulseCallback(GPIODATA*);
private:	
	uint8_t  _brinkon;
};

inline void dummyfunc(const char*fmt, ...) {}
#if DEBUG_MAIN
#define DEBUG_PRINT(fmt, ...) printf(fmt,  ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) dummyfunc(fmt,  ##__VA_ARGS__)
#endif


#ifdef __cplusplus
}
#endif


#endif