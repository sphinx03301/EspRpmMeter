#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "esp_adc_cal.h"
#include "esp_intr_alloc.h "
#include "esp_log.h"
#include "esp_http_server.h"
#include <nvs_flash.h>
#include "driver/adc.h"
#include "driver/ledc.h"

#include "main.h" 

//#define ESP_LOG_LEVEL 2
static const char *TAG = "MAIN";
EspRpmMeter* EspRpmMeter::pThis = nullptr;

// You have to set these CONFIG value using menuconfig.

QueueHandle_t  GPIODATA::evtQue;
#if (__USE_SPIFS__)
static void SPIFFS_Directory(const char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(__FUNCTION__,"d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}
#endif
/**--------------------------------------------------------------------
 *  IOポート割り込み
 *  PIN割り込みはチャタリング対策をする必要があるので、都度割り込みを停止する
 --------------------------------------------------------------------*/
void IRAM_ATTR gpio_XX_isr(void *arg) {

	GPIODATA	*pgpio = (GPIODATA*)arg;
	if(pgpio->pcallback != nullptr) {
		pgpio->pcallback(pgpio);
		return;
	}
	int state = gpio_get_level(pgpio->gpionum) ? 0 : 1;
    gpio_set_intr_type(pgpio->gpionum, GPIO_INTR_DISABLE);
	GPIOSTS evt;
	evt.gpio = pgpio->gpionum;
	evt.state = state;
	pgpio->idxCur = ((state + 1) & 1);
    xQueueSendFromISR(pgpio->evtQue, &evt, NULL);
}

void GPIODATA::setValues(gpio_num_t num, gpio_int_type_t type1, gpio_int_type_t type2, uint8_t idx)
{
	this->gpionum = num;
	this->idxCur = idx;
	this->inttypes[0] = type1;
	this->inttypes[1] = type2;
}

//!----------------------------------------------------------
//!
//!----------------------------------------------------------
EspRpmMeter::EspRpmMeter() {
	pThis = this;
	_settingd.SetDefault();
	mainsw = 0;
	sw1 = 0;
	sw2 = 0;
	sw3 = 0;
	_curMode =PUSHWIPER;
	_st1_request = 0;
	_st2_request = 0;
	_th_wiper = NULL;
}



void EspRpmMeter::SaveSettingTask(void *pvParameters) {
	((EspRpmMeter *)pvParameters)->SaveSettingTask();
}

void EspRpmMeter::LcdTask(void *pvParameters) {
	((EspRpmMeter *)pvParameters)->LcdTask();
}
void EspRpmMeter::MainTask(void *pvParameters) {
	((EspRpmMeter *)pvParameters)->MainTask();
}

int EspRpmMeter::loadSettingData() {
	// 設定をFLASHメモリから読み込む
	nvs_handle my_handle;
	ESP_ERROR_CHECK(nvs_flash_init());
	esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);

	if (err != ESP_OK)
	{
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		ESP_ERROR_CHECK(err);
	}
	ESP_LOGI(TAG, "Initializing  Storage");
	
	size_t paramsize = sizeof(_settingd);
	if (ESP_ERR_NVS_NOT_FOUND == nvs_get_blob(my_handle, "SETTINGD", &_settingd, &paramsize)) {
		_settingd.SetDefault();
		nvs_set_blob(my_handle, "SETTINGD", &_settingd, sizeof(SETTINGD));
		nvs_commit(my_handle);
		nvs_close(my_handle);
		return -1;
	}
	
	ESP_LOGI(TAG, "Trying to close nvs");
	nvs_close(my_handle);

	DEBUG_PRINT("Got setting data \n");
	return 0;
}

/**
 * 設定値をSPIFFSに書き込む無限待ちタスク
 * 
 * */
void EspRpmMeter::SaveSettingTask() {
	int	dummy;
	while (1) {
		if(xQueueReceive(this->ssQue, &dummy, portMAX_DELAY)) {
			nvs_handle my_handle;
			ESP_ERROR_CHECK(nvs_flash_init());
			esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
			if (err != ESP_OK) {
				ESP_LOGE("SAVE", "Savefile opening error has occured.");
			} else {
				nvs_set_blob(my_handle, "SETTINGD", &_settingd, sizeof(SETTINGD));
				nvs_commit(my_handle);
				nvs_close(my_handle);
			}
		}
	}

}


char gszWork[16];

//#define FONT24	fonts::lgfxJapanMincho_24
//#define FONT16  fonts::lgfxJapanMincho_16
//#define FONT24	fonts::FreeMono12pt7b
//#define FONT16  fonts::FreeMono9pt7b
#define FONT24	fonts::lgfxJapanMincho_16
#define FONT16  fonts::lgfxJapanMincho_16

//!--------------------------------------------------------------------------//
//! Display task on TFT screen
//!	液晶表示タスク
//!--------------------------------------------------------------------------//
void EspRpmMeter::LcdTask()
{
	_plcd = new LGFX();
	_plcd->init();

	//_plcd->setRotation(1);
	_plcd->fillScreen(TFT_WHITE);

	uint16_t xpos = 5;
	uint16_t ypos = 5;
#define BTNMERGINE		2	
#define BTNSIZE			24	
#define BTNR			(BTNSIZE / 2)
#define BTNPOSX(n)		(n*(BTNSIZE + BTNMERGINE) + (BTNSIZE / 2))
#define BTNPOSY2(n)		((n + 1)*BTNSIZE - BTNMERGINE)
#define TFT_DARKBLUE	0x0007
	_plcd->fillCircle(xpos + BTNPOSX(0), ypos + BTNR, BTNR, 0x3800 );
	
	_plcd->setFont(&FONT24);
	_plcd->setTextColor(TFT_BLACK, TFT_WHITE);
	_plcd->setCursor(xpos, ypos + 40);
		
	while (1) {
		DISPCMD  evt;
		if(xQueueReceive(dispQue, &evt, pdMS_TO_TICKS(500))) {
			DEBUG_PRINT("LCD TASK called evt.cmd=%d, subcmd=%d\n", evt.cmd, evt.subcmd);
			switch(evt.cmd) {
				case 	CMD_LED_PON:
					_plcd->fillCircle(xpos + BTNPOSX(0), ypos + BTNR, BTNR,  evt.state ? TFT_RED : 0x3800);
				break;
				case 	CMD_LED_WIPER: {
					if(evt.subcmd == 0) {
						if(_curMode >= SET_MINMAX) {
							_brinkon = (_brinkon + 1) & 1;
							_plcd->fillCircle(xpos + BTNPOSX(1), ypos + BTNR, BTNR,  TFT_GREEN);
							_plcd->fillCircle(xpos + BTNPOSX(2), ypos + BTNR, BTNR, TFT_WHITE );
							_plcd->fillCircle(xpos + BTNPOSX(3), ypos + BTNR, BTNR, TFT_WHITE );
						} else {
							_plcd->fillCircle(xpos + BTNPOSX(1), ypos + BTNR, BTNR,_curMode == PUSHWIPER ?  TFT_GREEN : TFT_WHITE );
							_plcd->fillCircle(xpos + BTNPOSX(2), ypos + BTNR, BTNR, _curMode == AUTOWIPER ?  TFT_BLUE : TFT_WHITE );
							_plcd->fillCircle(xpos + BTNPOSX(3), ypos + BTNR, BTNR, _curMode == INTERMWIPER ?  TFT_VIOLET : TFT_WHITE );
						}
						_plcd->setFont(&FONT24);
						_plcd->setTextColor(TFT_BLACK, TFT_WHITE);
						_plcd->setCursor(xpos, ypos + 40);
						_plcd->fillRect(65, 35, 115, 45, TFT_WHITE);
					} else if(evt.subcmd == 1){
						_plcd->fillRect(xpos + BTNPOSX(5), ypos + BTNR, 16, 16,  evt.state ? TFT_RED : 0x3800);
					}
				}
				break;
				case	CMD_DISP_VALUE:
					//lcdDrawString(&dev, fx24M, xpos - 35, 85, evt.strings, BLACK);
					//DEBUG_PRINT("adc value is %s\r", evt.strings);
				break;
				case CMD_DISP_RPM: {
					_plcd->fillRect(xpos + BTNPOSX(5), ypos + BTNR + 16, 16, 16, evt.state ? TFT_BLUE : TFT_DARKBLUE);
					_plcd->setFont(&FONT24);
					_plcd->setTextColor(TFT_BLACK, TFT_WHITE);
					_plcd->setCursor(70, ypos + 40);
					_plcd->print(evt.strings);
				}
				break;
			}
		} else {
		}
	}
}

/**
 * 	AD変換実行タスク
 * 他にボタン割り込みから処理も実施
 * */
void EspRpmMeter::MainTask() {
	#define RESET_TIME	3
	uint8_t  mainsw_reset = 0,
			sw1_reset = 0,
			sw2_reset = 0,
			sw3_operation = 0,
			recv_request = 0,
			send_request = 0,
			bwork,
			cplus = 0,				// クランクパルスのカウンタ。1回転で複数回あるのを考慮して使う。ACなら0or1 DCなら0～回数
			tx_buffer[32],
			rx_buffer[32];
	int 	sock;
	int		jsNOffset, iwork;
	JoystickLogic  jlogic;
	uint32_t lastTick = 0, lastRecvTick = 0;
	// 
	while(true) {
		GPIOSTS	evt;
		DISPCMD sevt;
		if(xQueueReceive(portRqQue, &evt, pdMS_TO_TICKS(10))) {
			 switch(evt.gpio) {
				 case _PIN_MAINSW:
				 	// 設定中はONにできないようにする
				 	if (_curMode < SET_MINMAX ) {
						this->mainsw = (this->mainsw + 1) & 1;
						sevt.set(1, mainsw, 0);
						tx_buffer[0] = CMD_MAINSWITCH;
						tx_buffer[1] = mainsw;
						send_request = 1;
						mainsw_reset = RESET_TIME;
						gpio_set_level(_PIN_RELAY, this->mainsw);
						// 0になったらDUTYを0にする
						if( this->mainsw == 0) {
							ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
							ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
							ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
							ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
						}
						xQueueSend(dispQue, &sevt, portTICK_PERIOD_MS);
					}
				 break;
				 case _PIN_SW1_:
					sw1_reset = RESET_TIME;
					sw1 = evt.state;
					if(evt.state) {		// ボタンが押されている時
						_tickCntSav = xTaskGetTickCount();
					} else {
						if(sw3_operation) {
							sw3_operation = 0;
							break;
						} 
						sevt.set(2, 0, 0);
						if(_curMode < SET_MINMAX) {
							uint32_t  ticks = xTaskGetTickCount() - _tickCntSav;
							// メインスイッチがoff かつ sw1 が3秒以上押された場合
							if(mainsw != 1 && ticks >= pdMS_TO_TICKS(SETTING1WAIT)) {
								_curMode = SET_MINMAX;
								_backup = _settingd;	// 現在設定退避
								_backup.Initialize();
///							} else if(ticks >= SETTING2WAIT) {
//								_curMode = SET_MINMAX;
							} else {
								ResetWiper();			// 一度ワイパーを停止する
								if(++_curMode > INTERMWIPER)
									_curMode = PUSHWIPER;
							}
							_st1_request = 0;
							_st2_request = 0;
						} else {
							if(++_curMode > SET_WINTERVAL) {
								_curMode = PUSHWIPER;
								// 設定値から、中間幅、可動域をバッファにセット
								jsNOffset = _settingd.nrange / 2;
							}
							switch (_curMode)	{
							//case SET_VRANGE: 
							case SET_CENTERM:
									jlogic.SetRange(_settingd.hbottom, _settingd.vbottom, _settingd.htop, _settingd.vtop);
								break;
							default:
								jlogic.Clear();
								break;
							}
						}
					 	xQueueSend(dispQue, &sevt, portTICK_PERIOD_MS);
					}
				 break;
				 case _PIN_SW2_: {
				 	sw2 = evt.state;
					sw2_reset = RESET_TIME;
					if(sw1) {
						if(evt.state == 0) {
							sw3_operation = 1;
							this->sw3 = (this->sw3 + 1) & 1;
							if(this->sw3) {

							}
							tx_buffer[0] = CMD_SUBSWITCH;
							tx_buffer[1] = this->sw3;
							send_request = 1;
							sevt.set(3, this->sw3, 0);
						 	xQueueSend(this->dispQue, &sevt, portTICK_PERIOD_MS);
						}
						break;
					}
					switch (_curMode)	{
					case SET_MINMAX:
						_settingd.RestoreMinMax(_backup);
						break;
					case SET_CENTERM:
						_settingd.RestoreCenterLo(_backup);
						break;
					// case SET_VRANGE: 
					// 	_settingd.RestoreVRange(_backup);
					// 	break;
					// case SET_HRANGE:
					// 	_settingd.RestoreHRange(_backup);
					// 	break;
					case SET_NRANGE:
						_settingd.RestoreNrange(_backup);
						break;
					case SET_WINTERVAL:
						_settingd.RestoreWiper(_backup);
						break;
					default:
						 WiperLogic(evt.state);
					}
					if (_curMode >= SET_MINMAX && evt.state == 0) {	// ボタン離した時
						// 保存リクエストを出す
					 	xQueueSend(this->ssQue, &val, portTICK_PERIOD_MS);
					}
				 }
				 break;
				 case _PIN_ACPULS: {
					//  現在何回転？
					// rpm＝1分当たりの回転数 = 60*1秒当たりの回転数
					// 1秒当たりの回転数 = 1秒間のカウント/パルス間のカウント
					int rpm = 60 * configTICK_RATE_HZ / _tickCnt4Plus;	
					cplus =(cplus + 1) & 1;
					sevt.cmd = CMD_DISP_RPM;
					sprintf(sevt.strings, " 5d", rpm);
					sevt.state = cplus;
					xQueueSend(this->dispQue, &sevt, portTICK_PERIOD_MS);
				    gpio_set_intr_type(_PIN_ACPULS, GPIO_INTR_NEGEDGE);		// Negative Edgeで割込み許可

				 }
				 	break;
				 case _PIN_DCPULS:
				 	break;
			 }
			 printf("intrrupt %d, %d\n", evt.gpio, evt.state);
		} 
		else  {	// QUEアイドルの処理
			//DEBUG_PRINT("on Idole\n");
			// メインスイッチオンの場合、ジョイスティックの現在位置をPPM信号(1.0 ～ 2.0ms)に
			// 変換したデータにする
			if(mainsw) {
				int v10usec, h10usec;
				sevt.cmd = 7;

				if(!_settingd.centerlo) {
					// 下側の処理
					if(voltageV < _initJSVval - jsNOffset) {
						v10usec = 150 -  ((_initJSVval - voltageV) * (150 - _settingd.vmin) / (_initJSVval - _settingd.vbottom));
					}
					// 上側の処理
					else if(voltageV > _initJSVval + jsNOffset) {
						v10usec = (voltageV - _initJSVval) * (_settingd.vmax - 150) / (_settingd.vtop - _initJSVval) + 150;
					} else {
						v10usec = 150;		// ニュートラル
					}
				} else {	//中央が最低値の場合
					// 下側の処理
					if(voltageV < _initJSVval - jsNOffset) {
						v10usec = (_initJSVval - voltageV) * (180 - _settingd.vmin) / (_initJSVval - _settingd.vbottom) + 100;
					}
					// 上側の処理
					else if(voltageV > _initJSVval + jsNOffset) {
						v10usec = (voltageV - _initJSVval) * (_settingd.vmax - 100) / (_settingd.vtop - _initJSVval) + 100;
					} else {
						v10usec = 100;		// 最低値
					}
				}
				if(v10usec > 200)
					v10usec = 200;

				// 下側の処理
				if(voltageH < _initJSHval - jsNOffset) {
					h10usec = 150 -  ((_initJSHval - voltageH) * (150 - _settingd.hmin) / (_initJSHval - _settingd.hbottom));
					//h10usec = voltageH * (150 - _settingd.vmin) / (_initJSHval - _settingd.hbottom);
				}
				// 上側の処理
				else if(voltageH > _initJSHval + jsNOffset) {
					h10usec = (voltageH - _initJSHval) * (_settingd.hmax - 150) / (_settingd.htop - _initJSHval) + 150;
					//h10usec = voltageH * (_settingd.hmax - 150) / (_settingd.htop - _initJSHval);
				} else {
					h10usec = 150;		// ニュートラル
				}

				// ここでESC信号のDUTY比をセットする.11bit resolutionなので2048を2000μ秒=2m秒として
				ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, v10usec * 2048 / 2000 + 1);
				ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
				ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, h10usec * 2048 / 2000 + 1);
				ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);

				tx_buffer[0] = CMD_DUTYSEND;
				tx_buffer[2] = (uint8_t)v10usec;
				tx_buffer[1] = 3;				// CH1 = 1 CH2 = 2 のデータを送信するので3
				tx_buffer[3] = (uint8_t)h10usec;
				send_request = 1;
			} else if( sw3) {
				// 5段階程度の＋－でPWM値を調整する
				bwork = _settingd.pwmlevel;
				if(voltageV < _initJSVval - jsNOffset) {
					if(_settingd.pwmlevel > 0)
						_settingd.pwmlevel--;
				}
				// 上側の処理
				else if(voltageV > _initJSVval + jsNOffset) {
					if(_settingd.pwmlevel < 255)
						_settingd.pwmlevel++;
				}
				
				if(bwork != _settingd.pwmlevel) {
					tx_buffer[0] = CMD_SUBDUTY;
					tx_buffer[1] = _settingd.pwmlevel;			
					send_request = 1;
				}
			} else { // メインスイッチOFFロジック
				sevt.cmd = 0;
				if (_curMode >= SET_MINMAX ) {
					switch (_curMode)	{
					case SET_MINMAX: {
						_backup.InflateMinMax(voltageH, voltageV);
						}
						break;
					case SET_CENTERM:
						jlogic.AddAction(voltageH, voltageV);
						_backup.UpdateCenterLo(jlogic.GetActionC());
						break;
					// case SET_VRANGE: {
					// 		jlogic.AddAction(voltageH, voltageV);
					// 		_backup.UpdateVRange( jlogic.GetAction());
					// 	}
					// 	break;
					// case SET_HRANGE: {
					// 		jlogic.AddAction(voltageH, voltageV);
					// 		_backup.UpdateHRange( jlogic.GetAction());
					// 	}
					// 	break;
					case SET_NRANGE: {
							jlogic.AddAction(voltageH, voltageV);
							_backup.UpdateNRange( jlogic.GetActionC());
						}
						break;
					case SET_WINTERVAL: {
							jlogic.AddAction(voltageH, voltageV);
							_backup.UpdateWRange( jlogic.GetAction());
						}
						break;
					}
				} else {
					if(!_st1_request && sw1) {
						uint32_t  ticks = xTaskGetTickCount() - _tickCntSav;
						if(ticks >= pdMS_TO_TICKS(SETTING1WAIT)) {
							_st1_request = true;
						}
					}
				}
			} // end if (mainsw)

			
#if(1)		// チャタリング対処ロジック -->			
			if(mainsw_reset) {
				if((--mainsw_reset) == 0) {
					gpio_set_intr_type(_PIN_MAINSW, GPIO_INTR_POSEDGE);
				//_gpdt_mainsw.idxCur = 0;
				}
			}
			// if(sw1_reset){
			// 	if((--sw1_reset) == 0) {
			// 	gpio_set_intr_type(_PIN_SW1_, _gpdt_sw1.inttypes[_gpdt_sw1.idxCur & 1]);
			// 	}
			// }
			// if(sw2_reset){
			// 	if((--sw2_reset) == 0) {
			// 	gpio_set_intr_type(_PIN_SW2_, _gpdt_sw2.inttypes[_gpdt_sw2.idxCur & 1]);
			// 	}
			// }
			//<-- チャタリング対処
			int state_sw1 = gpio_get_level(_PIN_SW1_);
			int state_sw2 = gpio_get_level(_PIN_SW2_);
			if(sw1 == state_sw1) {	// ONでボタンが離された場合(HI) or OFFで押された場合
				GPIOSTS evt;
				evt.gpio = _gpdt_sw1.gpionum;
				_gpdt_sw1.idxCur = state_sw1;
				evt.state = (sw1 + 1) & 1;
			    xQueueSend(_gpdt_sw1.evtQue, &evt, NULL);
			}
			if(sw2 == state_sw2) {	// ONでボタンが離された場合(HI) or OFFで押された場合
				GPIOSTS evt;
				evt.gpio = _gpdt_sw2.gpionum;
				_gpdt_sw2.idxCur = state_sw2;
				evt.state = (sw2 + 1) & 1;
			    xQueueSend(_gpdt_sw2.evtQue, &evt, NULL);
			}
#endif
		}

	}
}

/** 
 * Clank pulse interrupt routine implement
 * クランクパルスの割込み処理本体
*/
void  EspRpmMeter::PulseCallbackImpl(GPIODATA* pgpio) {


	// DC pulse is recent type to get clank position, and it gives several pulses in a rotation
	if(pgpio->gpionum == _PIN_DCPULS) {
		// DC pulse don't generate wrong signal, so we don't have to stop intrrupt.
		// DCパルスの場合不正な割込みはかからないので割込みは停止しない(台形波となっている)

		// need to calc which pulse is for fire, otherwith should return.

	} else {
		// Here is for AC pulse, and for old type. it gives AC waves 1 time in a rotation.

		// At first, we should stop interrupt on this pin to deal with reverse side wave of AC
		// AC波だとジャダーが発生して複数割込みかかるので、しばし割込み停止する。20000rpmでも
		// 3m秒ごとの割込みなので1ミリ秒ほど停止しても問題ない
	    gpio_set_intr_type(pgpio->gpionum, GPIO_INTR_DISABLE);
		int ncurrent = xTaskGetTickCountFromISR();
		_tickCnt4Plus = ncurrent  - _lastTickCnt4Plus;
		_lastTickCnt4Plus = ncurrent;



	}

	// finally we should send something to main loop to show information like RPM
	// 最終的に描画のためにメインループで何かしら処理をするようキューにつめておく
	GPIOSTS evt;
	evt.gpio = pgpio->gpionum;
	evt.state = 1;
    xQueueSendFromISR(pgpio->evtQue, &evt, NULL);
}



void  EspRpmMeter::PulseCallback(GPIODATA* p) {
	EspRpmMeter::getInstance()->PulseCallbackImpl(p);
}

/**--------------------------------------------------------------------
 *  GPIOの設定。
 --------------------------------------------------------------------*/
void EspRpmMeter::gpio_setting() {

    gpio_config_t io_conf;

    // 入力ポート設定
 	//  io_conf.intr_type = GPIO_INTR_LOW_LEVEL;	// Loで割り込み
    io_conf.intr_type = GPIO_INTR_DISABLE;	
    io_conf.pin_bit_mask = (1ULL<<_PIN_MAINSW) | (1ULL<<_PIN_SW1_) | (1ULL<<_PIN_SW2_) | (1ULL<<_PIN_ACPULS) | (1ULL<<_PIN_DCPULS);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // 出力ポート設定
	// 現段階ではなし
    
    //gpio_isr_register(gpio_XX_isr, NULL, ESP_INTR_FLAG_LEVELMASK, &isr_handle);
    gpio_install_isr_service(0);
	DEBUG_PRINT("gpio_install_isr_service done\n");
	_gpdt_mainsw.setValues(_PIN_MAINSW, GPIO_INTR_POSEDGE, GPIO_INTR_DISABLE, 0);
	_gpdt_sw1.setValues(_PIN_SW1_, GPIO_INTR_NEGEDGE, GPIO_INTR_POSEDGE, 0);
	_gpdt_sw2.setValues(_PIN_SW2_, GPIO_INTR_NEGEDGE, GPIO_INTR_POSEDGE, 0);
	_gpdt_acpulse.setValues(_PIN_ACPULS, GPIO_INTR_NEGEDGE, GPIO_INTR_NEGEDGE, 0);
	_gpdt_dcpulse.setValues(_PIN_DCPULS, GPIO_INTR_NEGEDGE, GPIO_INTR_NEGEDGE, 0);
	_gpdt_acpulse.pcallback = &EspRpmMeter::PulseCallback;
	_gpdt_dcpulse.pcallback = &EspRpmMeter::PulseCallback;
    gpio_isr_handler_add(_PIN_MAINSW, gpio_XX_isr, &_gpdt_mainsw);
    gpio_isr_handler_add(_PIN_ACPULS, gpio_XX_isr, &_gpdt_acpulse);
    gpio_isr_handler_add(_PIN_DCPULS, gpio_XX_isr, &_gpdt_dcpulse);
    //gpio_isr_handler_add(_PIN_SW1_, gpio_XX_isr, &_gpdt_sw1);
    //gpio_isr_handler_add(_PIN_SW2_, gpio_XX_isr,& _gpdt_sw2);
	gpio_set_intr_type(_PIN_MAINSW, GPIO_INTR_POSEDGE);
	gpio_set_intr_type(_PIN_ACPULS, GPIO_INTR_NEGEDGE);
	gpio_set_intr_type(_PIN_DCPULS, GPIO_INTR_NEGEDGE);
	//gpio_set_intr_type(_PIN_SW1_, GPIO_INTR_NEGEDGE);
	//gpio_set_intr_type(_PIN_SW2_, GPIO_INTR_NEGEDGE);

}


extern "C" void app_main(void);

void app_main(void) { 
	EspRpmMeter *pMain = new EspRpmMeter();
	pMain->main();
}

void EspRpmMeter::main() {
	ESP_LOGI(TAG, "Initializing GPIOS");
	gpio_setting();
    portRqQue = xQueueCreate(10, sizeof(GPIOSTS));
	dispQue = xQueueCreate(5, sizeof(DISPCMD));
	ssQue = xQueueCreate(5, sizeof(int));
	GPIODATA::evtQue = portRqQue;

#if (__USE_SPIFS__)
	ESP_LOGI(TAG, "Initializing SPIFFS");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 12,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t spiffs_ret =esp_vfs_spiffs_register(&conf);

	if (spiffs_ret != ESP_OK) {
		if (spiffs_ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (spiffs_ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(spiffs_ret));
		}
	} else {
		size_t total = 0, used = 0;
		esp_err_t ret = esp_spiffs_info(NULL, &total,&used);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
		} else {
			ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
		}
		SPIFFS_Directory("/spiffs/");
	}
#endif
	// セッティングファイル読み込み
	loadSettingData();
	
	xTaskCreate( LcdTask, "LcdTask", 1024*6, this, tskIDLE_PRIORITY + 1, NULL);
	DEBUG_PRINT("TaskCreat LcdTask \n");
    xTaskCreatePinnedToCore(MainTask, "AdTask", 1024*6, this, tskIDLE_PRIORITY + 1, NULL, 1);
	DEBUG_PRINT("TaskCreat MainTask \n");
	xTaskCreate( SaveSettingTask, "SsTask", 4096, this, tskIDLE_PRIORITY + 1, NULL);
	DEBUG_PRINT("TaskCreat SaveSettingTask \n");

}
