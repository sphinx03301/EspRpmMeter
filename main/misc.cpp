// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// //#define CONFIG_FREERTOS_HZ	1000
// //#define portTICK_PERIOD_MS	1
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
// #include "esp_err.h"
// #include "esp_log.h"
// #include "esp_system.h"
// #include "esp_vfs.h"
// #include "esp_spiffs.h"
// #include "esp_heap_caps.h"
// #include "esp_adc_cal.h"
// #include "esp_intr_alloc.h "
// #include "esp_log.h"
// #include "esp_http_server.h"
// #include <nvs_flash.h>
// #include "driver/adc.h"
// #include "driver/ledc.h"
// #include "lwip/err.h"
// #include "lwip/sockets.h"
// #include "lwip/sys.h"
// #include <lwip/netdb.h>
#include "misc.h"

#if(1) // SETTINGD クラス

/**
 * 設定クラスのデフォルトデータ設定メソッド
 * */
void SETTINGD::SetDefault() {
	htop = 3285;
	hbottom = 0;
	vtop = 3285;
	vbottom = 0;
	hmax = 200;
	vmax = 200;
	hmin = 100;
	vmin = 100;
	wiperon = 2000;
	wiperoff = 10000;
	nrange = 400;
	pwmlevel = 200;		// 8割程度の出力を初期値とする
	this->maxrpm = _MAX_RPM;	// 10000 rpm
	this->sangle = _SANGLE;
	this->eangle = _EANGLE;
}

void SETTINGD::Initialize() {
	this->hbottom = 99999;
	this->htop = 0;
	this->vtop = 0;
	this->vbottom = 99999;
}

void SETTINGD::InflateMinMax(int h, int v) {
	if(v > vtop)	vtop = v;
	if(v < vbottom)	vbottom = v;
	if(h > htop)	htop = h;
	if(h < hbottom)	hbottom = h;
}

// 何かしら信号があれば、オンオフトグルする
void SETTINGD::UpdateCenterLo(uint8_t flag) {
	if((flag & ER_TOP) || (flag & ER_BOTTOM)) {
		this->centerlo = (this->centerlo + 1) & 1;
	}
}

void SETTINGD::UpdateHRange(uint8_t flag) {
	if(flag & ER_TOP) {
		if((flag & ER_LEFT) && hmax > 180)
			hmax -= 5;
		if((flag & ER_RIGHT) && hmax < 250)
			hmax += 5;
	} else if(flag & ER_BOTTOM) {
		if((flag & ER_LEFT) && hmin > 50)
			hmin -= 5;
		if((flag & ER_RIGHT) && hmin < 120)
			hmin += 5;
	}
}
void SETTINGD::UpdateVRange(uint8_t flag) {
	if(flag & ER_TOP) {
		if((flag & ER_LEFT) && vmax > 180)
			vmax -= 5;
		if((flag & ER_RIGHT) && vmax < 250)
			vmax += 5;
	} else if(flag & ER_BOTTOM) {
		if((flag & ER_LEFT) && vmin > 50)
			vmin -= 5;
		if((flag & ER_RIGHT) && vmin < 120)
			vmin += 5;
	}
}
void SETTINGD::UpdateNRange(uint8_t flag) {

	if(this->nrange < 0 || this->nrange > 3215)
		this->nrange = 400;
	if((flag & ER_LEFT) || (flag & ER_BOTTOM))
		this->nrange -= 5;
	else if ((flag & ER_RIGHT) || (flag & ER_TOP))
		this->nrange += 5;
}

void SETTINGD::UpdateWRange(uint8_t flag) {
	if(flag & ER_TOP) {
		if((flag & ER_LEFT) && wiperon > 1000)
			wiperon -= 500;
		if((flag & ER_RIGHT) && wiperon < 30000)
			wiperon += 500;
	} else if(flag & ER_BOTTOM) {
		if((flag & ER_LEFT) && wiperoff > 1000)
			wiperoff -= 500;
		if((flag & ER_RIGHT) && wiperoff < 30000)
			wiperoff += 500;
	}
}

void SETTINGD::RestoreMinMax(const SETTINGD& org) {
	this->vtop = org.vtop;
	this->vbottom = org.vbottom;
	this->htop = org.htop;
	this->hbottom = org.hbottom;
}

void SETTINGD::RestoreHRange(const SETTINGD& org) {
	this->hmax = org.hmax;
	this->hmin = org.hmin;
}

void SETTINGD::RestoreVRange(const SETTINGD& org) {
	this->vmax = org.vmax;
	this->vmin = org.vmin;
}

void SETTINGD::RestoreWiper(const SETTINGD& org) {
	this->wiperon = org.wiperon;
	this->wiperoff = org.wiperoff;
}

void SETTINGD::RestoreNrange(const SETTINGD& org) {
	this->nrange = org.nrange;
}

void SETTINGD::RestoreCenterLo(const SETTINGD& org) {
	this->centerlo = org.centerlo;
}

#endif


#if(1) // JoystickLogic クラス
void JoystickLogic::SetRange(int left, int bottom, int right, int top ) {
	int width = right - left;
	int height = top - bottom;
	this->leftl = width / 3 + left;
	this->rightl =  width * 2 / 3 + left;
	this->bottoml = height / 3 + bottom;
	this->topl = height * 2 / 3 + bottom;;
	state = flag = 0;
}


void JoystickLogic::AddAction(int h, int v) {
	if(h < this->leftl)	{
		flag = (flag & 0x0C) | ER_LEFT;
	} else if (h > this->rightl) {
		flag = (flag & 0x0C) | ER_RIGHT;
	} else if (v < this->bottoml) {
		flag = (flag & 3) | ER_BOTTOM;
	} else if (v > this->topl) {	
		flag = (flag & 3) | ER_TOP;
	}	else  {
		state = flag;
		flag &= 0x0C;
	}
}

// 1:Left 2:right 4:bottom 8:top. combination
uint8_t  JoystickLogic::GetAction() {
	return state;
}

uint8_t  JoystickLogic::GetActionC() {
	if(state)
		flag = 0;
	return state;
}

#endif

#if (1)
GPIODATA::GPIODATA(void* param, void (*pcfunc)(GPIODATA*), gpio_num_t gpionum, gpio_int_type_t it1, gpio_int_type_t it2, uint8_t idxinit) {
	setValues(param, pcfunc, gpionum, it1, it2, idxinit);
}

void GPIODATA::setValues(void* param, void (*pcfunc)(GPIODATA*), gpio_num_t gpionum, gpio_int_type_t it1, gpio_int_type_t it2, uint8_t idxinit) {
	this->pData = param;
	this->pData2 = nullptr;
	this->pcallback = pcfunc;
	this->gpionum = gpionum;
	this->inttypes[0] = it1;
	this->inttypes[1] = it2;
	this->idxCur = idxinit;
}

#endif
