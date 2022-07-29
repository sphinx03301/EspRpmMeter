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
#include "esp_timer.h"
#include "LcdTask.h"
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
//	gpio_intr_disable(pgpio->gpionum);
	if(pgpio->pcallback != nullptr) {
		pgpio->pcallback(pgpio);
		return;
	}
	int state = gpio_get_level(pgpio->gpionum) ? 0 : 1;
	GPIOSTS evt;
	evt.gpio = pgpio->gpionum;
	evt.state = state;
	pgpio->idxCur = ((state + 1) & 1);
    xQueueSendFromISR(pgpio->evtQue, &evt, NULL);
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
	_is_intest = false;
}

void EspRpmMeter::SaveSettingTask(void *pvParameters) {
	((EspRpmMeter *)pvParameters)->SaveSettingTask();
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
		ESP_LOGE(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		ESP_ERROR_CHECK(err);
	}
	
	size_t paramsize = sizeof(_settingd);
	if (ESP_ERR_NVS_NOT_FOUND == nvs_get_blob(my_handle, "SETTINGD", &_settingd, &paramsize)) {
		_settingd.SetDefault();
		nvs_set_blob(my_handle, "SETTINGD", &_settingd, sizeof(SETTINGD));
		nvs_commit(my_handle);
		nvs_close(my_handle);
		return -1;
	}
	
	nvs_close(my_handle);

	if(_settingd.maxrpm < 0 || _settingd.maxrpm > 200)
		_settingd.SetDefault();


	DEBUG_PRINT("Got setting data \n");
	return 0;
}

/**
 * 設定値をSPIFFSに書き込む無限待ちタスク
 * 
 * */
void EspRpmMeter::SaveSettingTask() {
	int	dummy;
	DEBUG_PRINT("first SS was done\n");	
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

void EspRpmMeter::TestTask(void* p) {
	((EspRpmMeter*)p)->TestTask();
}

void EspRpmMeter::TestTask() {
	// 0 to maxrpm within 2;
	int  maxrpm = _settingd.maxrpm * 100;
	for(int i = 0; i < maxrpm; i += 100) {
		((LcdTask *)_pLcdTask)->ShowRpmValue(i);
		vTaskDelay(pdMS_TO_TICKS(20));
	}
	// maxrpm to 0 within 2;
	for(int i = maxrpm; i >= 0; i -= 100) {
		((LcdTask *)_pLcdTask)->ShowRpmValue(i);
		vTaskDelay(pdMS_TO_TICKS(20));
	}
	// 0 to maxrpm within 10;
	for(int i = 0; i < maxrpm; i += 10) {
		((LcdTask *)_pLcdTask)->ShowRpmValue(i);
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	// maxrpm to 0 within 2;
	for(int i = maxrpm; i >= 0; i -= 100) {
		((LcdTask *)_pLcdTask)->ShowRpmValue(i);
		vTaskDelay(pdMS_TO_TICKS(20));
	}
	_is_intest = false;
	vTaskDelete(_ht_Test);
}

/**
 * 	AD変換実行タスク
 * 他にボタン割り込みから処理も実施
 * */
void EspRpmMeter::MainTask() {
	#define RESET_TIME	3
	uint8_t  cplus = 0;				// クランクパルスのカウンタ。1回転で複数回あるのを考慮して使う。ACなら0or1 DCなら0～回数
	uint8_t  sw_test = 0;			// 
	uint8_t  sw_main = 0, sw_1 = 0, sw_2 = 0;			// 
	bool 	sw_mainOn = false;
	int		 irpm = 0;
	DEBUG_PRINT("first main was done\n");	
	char	szText[32];
// 
	while(true) {
		GPIOSTS	evt;
		DISPCMD sevt;
		if(xQueueReceive(portRqQue, &evt, pdMS_TO_TICKS(100))) {
			 switch(evt.gpio) {
				 case _PIN_MAINSW:
					 break;
				 case _PIN_SW1_:
					 break;
				 case _PIN_SW2_: 
				 	sw2 = evt.state;
					 break;
				 case _PIN_DCPULS: 
				 case _PIN_ACPULS: {
					//  現在何回転？
					// rpm＝1分当たりの回転数 = 60*1秒当たりの回転数
					// 1秒当たりの回転数 = 1秒間のカウント/パルス間のカウン
					int base = evt.state < 1 ? 1 : evt.state;
					printf("base=%d\n", base);
					int rpm = 1000000 / base * 60;		// esp_timerの値はus単位
					cplus =(cplus + 1) & 1;
					((LcdTask*)this->_pLcdTask)->ShowRpmValue(rpm);
				 }
				 	break;
			 }
		} 
		else  {	// QUEアイドルの処理
			if(sw_test) {
				if(gpio_get_level(_PIN_TESTSW) == 1)
					DEBUG_PRINT("GPIO0 OFF\n");	
					sw_test = 0;
			} else {
				if(gpio_get_level(_PIN_TESTSW) == 0) {
					DEBUG_PRINT("GPIO0 ON\n");	
					sw_test = 1;
					if(!_is_intest) {
						_is_intest = true;
						xTaskCreate( TestTask, "TestTask", 4096, this, tskIDLE_PRIORITY + 1, &_ht_Test);
					}
				}
			}
			sw_main = gpio_get_level(_PIN_MAINSW);
			if(sw_main == 0 && !sw_mainOn) {
				// trigger push
				DEBUG_PRINT("GPIO0 ON\n");	
				sw_mainOn = true;
			} else if(sw_main == 1 && sw_mainOn) {
				// trigger click
				sw_mainOn = false;
				DEBUG_PRINT("GPIO0 OFF\n");	
                                             			}

			if(mainsw) {
			} else if( sw3) {
			} else { // メインスイッチOFFロジック
			} // end if (mainsw)

			// アイドルということは回転信号がきていない。この場合、最後のシグナル間隔を補正する
			_lastPeriod = 200000 / _ACPCNT;		// 300回転程度だったことにする
			
		}

	}
}

/** 
 * Clank pulse interrupt routine implement
 * クランクパルスの割込み処理本体
*/
void  EspRpmMeter::PulseCallbackImpl(GPIODATA* pgpio) {
	static int clunkplus = 0;
	GPIOSTS evt;
	// removing noise. calc perid between previous int and current, if it was less than 1/10 of usual, it must be noize;
	// 割込み間の時間が通常の割込みの1/10以下の時間ならノイズであるとして無視する
	int64_t ncurrent = esp_timer_get_time(), nprev = _lastIsrTime;
	_lastIsrTime = ncurrent;
	if(ncurrent -  nprev < _lastPeriod / 10) {
		return; 
	}

	uint32_t period  = ncurrent  - _lastTickCnt4Puls;	// パルス間隔時間
	uint32_t periodprev = _lastPeriod;
	_lastTickCnt4Puls = ncurrent;
	_lastPeriod = period;

	// DC pulse is recent type to get clank position, and it gives several pulses in a rotation
	if(pgpio->gpionum == _PIN_DCPULS) {
		// DC pulse don't generate wrong signal, so we don't have to stop intrrupt.
		// DCパルスの場合不正な割込みはかからないので割込みは停止しない(台形波となっている)

		// judge if now is beginning point of a rotation
		// ここで前回の間隔と今回間隔から、起点かどうか判定できるはず
		if(periodprev * 3 / 2 < period) {	
			// 今回の間隔が、前回の1.5倍以上あれば多分今回が起点のはず
			pgpio->idxCur = 0;
		} else {
			pgpio->idxCur++;
		}

		// need to calc which pulse is for fire, otherwith should return.
		if((++clunkplus % _DCPCNT) != 0)
			return;
	} else {
		// Here is for AC pulse, and for old type. it gives AC waves 1 time in a rotation.

		// judge if now is beginning point of a rotation
		if(_ACPCNT > 1) {

		} else {
			pgpio->idxCur = 0;			// always zero if 1 signal in 1 rotation.
		}

		// determin wether the pulse is noize or not. True pulse is longer duration.
		// 複数パルスで1回転の場合の判断をする
		if((++clunkplus % _ACPCNT) != 0)
			return;
		// ここまで来たら1回転。
		evt.state = period;
	}



	// finally we should send something to main loop to show information like RPM
	// 最終的にメインループで何かしら処理をするようキューにつめておく(割込み処理でなく、
	// 一般処理で行う) 
	evt.gpio = pgpio->gpionum;
    xQueueSendFromISR(pgpio->evtQue, &evt, NULL);
	//if(ncurrent -  nprev  > 100) {
	//    xQueueSendFromISR(portIsrQue, pgpio, NULL);
    //	gpio_intr_disable(pgpio->gpionum);
	//	esp_timer_start_once(_th_IsrWait, 100);
	//}
}


/**
 * static function (callback) from ISR port intrrupt
 * ポート割り込みからのコールバック関数
 * */
 void  EspRpmMeter::PulseCallback(GPIODATA* p) {
	((EspRpmMeter *)p->pData)->PulseCallbackImpl(p);
}

/**
 * Timer callback function
 * Allow intrrupt on the gpio
 * */
void EspRpmMeter::vTimerCallback(void* pparam) {
	EspRpmMeter *pThis = (EspRpmMeter *)pparam;
	esp_timer_stop(pThis->_th_IsrWait);
	GPIODATA	gpiodata;
	xQueueReceive(pThis->portIsrQue, &gpiodata, 99999999);
    gpio_intr_enable(gpiodata.gpionum);
}
/**--------------------------------------------------------------------
 * Setting IO Ports.
 * GPIOの設定。
 * Don't have to use IRQ for slow switch's like push button, only 
 * signal need to use IRQ, like clank pulse
 * スピードが必要ないものは割込みを使わない。クランクパルスなどの信号のみ。
 --------------------------------------------------------------------*/
void EspRpmMeter::gpio_setting() {

    gpio_config_t io_conf;

    // 入力ポート設定
    io_conf.intr_type = GPIO_INTR_DISABLE;	
    io_conf.pin_bit_mask = (1ULL<<_PIN_MAINSW) | (1ULL<<_PIN_SW1_) | (1ULL<<_PIN_SW2_) | (1ULL<<_PIN_ACPULS) | (1ULL<<_PIN_DCPULS) | (1ULL<<_PIN_TESTSW) ;
	io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

  // 出力ポート設定
    io_conf.intr_type = GPIO_INTR_DISABLE;		
    io_conf.pin_bit_mask =  (1ULL<<_PIN_DC) |  (1ULL<<_PIN_RESET) |  (1ULL<<_PIN_MOSI)|  (1ULL<<_PIN_SCLK);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    //gpio_isr_register(gpio_XX_isr, NULL, ESP_INTR_FLAG_LEVELMASK, &isr_handle);
    gpio_install_isr_service(0);
	DEBUG_PRINT("gpio_install_isr_service done\n");

	gpio_isr_handler_add(_PIN_ACPULS, gpio_XX_isr, &_gpdt_acpulse);
    gpio_isr_handler_add(_PIN_DCPULS, gpio_XX_isr, &_gpdt_dcpulse);
	// _gpdt_acpulse.setValues(this,  &EspRpmMeter::PulseCallback,  _PIN_ACPULS, GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL, 0);
	// _gpdt_dcpulse.setValues(this,  &EspRpmMeter::PulseCallback, _PIN_DCPULS, GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL, 0);
	// gpio_set_intr_type(_PIN_ACPULS, GPIO_INTR_LOW_LEVEL);
	// gpio_set_intr_type(_PIN_DCPULS, GPIO_INTR_LOW_LEVEL);
	_gpdt_acpulse.setValues(this,  &EspRpmMeter::PulseCallback,  _PIN_ACPULS, GPIO_INTR_NEGEDGE, GPIO_INTR_POSEDGE, 0);
	_gpdt_dcpulse.setValues(this,  &EspRpmMeter::PulseCallback, _PIN_DCPULS, GPIO_INTR_NEGEDGE, GPIO_INTR_POSEDGE, 0);
	gpio_set_intr_type(_PIN_ACPULS, GPIO_INTR_NEGEDGE);
	gpio_set_intr_type(_PIN_DCPULS, GPIO_INTR_NEGEDGE);
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
	portIsrQue = xQueueCreate(5, sizeof(GPIODATA));
	ssQue = xQueueCreate(5, sizeof(int));
	GPIODATA::evtQue = portRqQue;

	// セッティングファイル読み込み
	loadSettingData();

	// Creating a timer for waiting reset interrupt
	// 割込みを最有効にするためのタイマ作成(100us単位)
	_th_IsrWait = NULL;

    const esp_timer_create_args_t oneshot_timer_args = {
            .callback = &vTimerCallback,
            /* argument specified here will be passed to timer callback function */
            .arg = (void*)this,
            .name = "one-shot"
    };
    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &_th_IsrWait));
	_lastTickCnt4Puls = esp_timer_get_time();
	_lastIsrTime = _lastTickCnt4Puls;
	_lastPeriod = 0;

	_pLcdTask = new LcdTask(1 + tskIDLE_PRIORITY);
	((LcdTask *)_pLcdTask)->SetRpmParam(_settingd.maxrpm, _settingd.sangle, _settingd.eangle);
	DEBUG_PRINT("TaskCreat LcdTask \n");
    xTaskCreatePinnedToCore(MainTask, "AdTask", 1024*6, this, tskIDLE_PRIORITY + 2, NULL, 1);
	DEBUG_PRINT("TaskCreat MainTask \n");
	xTaskCreate( SaveSettingTask, "SsTask", 4096, this, tskIDLE_PRIORITY + 1, NULL);
	DEBUG_PRINT("TaskCreat SaveSettingTask \n");
}
