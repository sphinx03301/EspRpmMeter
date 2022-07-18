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
				 case _PIN_ACPULS: {
					//  現在何回転？
					// rpm＝1分当たりの回転数 = 60*1秒当たりの回転数
					// 1秒当たりの回転数 = 1秒間のカウント/パルス間のカウント
					int rpm = 60 * configTICK_RATE_HZ / _tickCnt4Plus;	
					cplus =(cplus + 1) & 1;
					((LcdTask*)this->_pLcdTask)->ShowRpmValue(rpm);
				    gpio_set_intr_type(_PIN_ACPULS, GPIO_INTR_NEGEDGE);		// Negative Edgeで割込み許可
				 }
				 	break;
				 case _PIN_DCPULS:
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

			if(mainsw) {
			} else if( sw3) {
			} else { // メインスイッチOFFロジック
			} // end if (mainsw)

			
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
 * Setting IO Ports.
 * GPIOの設定。
 --------------------------------------------------------------------*/
void EspRpmMeter::gpio_setting() {

    gpio_config_t io_conf;

    // 入力ポート設定
 	//  io_conf.intr_type = GPIO_INTR_LOW_LEVEL;	// Loで割り込み
    io_conf.intr_type = GPIO_INTR_DISABLE;	
    io_conf.pin_bit_mask = (1ULL<<_PIN_MAINSW) | (1ULL<<_PIN_SW1_) | (1ULL<<_PIN_SW2_) | (1ULL<<_PIN_ACPULS) | (1ULL<<_PIN_DCPULS) | (1ULL<<_PIN_TESTSW) ;
	io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // 出力ポート設定
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
	/*
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
	gpio_set_intr_type(_PIN_MAINSW, GPIO_INTR_POSEDGE);
	gpio_set_intr_type(_PIN_ACPULS, GPIO_INTR_NEGEDGE);
	gpio_set_intr_type(_PIN_DCPULS, GPIO_INTR_NEGEDGE);
*/
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
	ssQue = xQueueCreate(5, sizeof(int));
	GPIODATA::evtQue = portRqQue;

	// セッティングファイル読み込み
	loadSettingData();

	_pLcdTask = new LcdTask(1 + tskIDLE_PRIORITY);
	((LcdTask *)_pLcdTask)->SetRpmParam(_settingd.maxrpm, _settingd.startdgree, _settingd.enddgree);
	DEBUG_PRINT("TaskCreat LcdTask \n");
    xTaskCreatePinnedToCore(MainTask, "AdTask", 1024*6, this, tskIDLE_PRIORITY + 2, NULL, 1);
	DEBUG_PRINT("TaskCreat MainTask \n");
	xTaskCreate( SaveSettingTask, "SsTask", 4096, this, tskIDLE_PRIORITY + 1, NULL);
	DEBUG_PRINT("TaskCreat SaveSettingTask \n");
}
