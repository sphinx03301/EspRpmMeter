#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_log.h"
#include "misc.h"
#include "LcdTask.h"
#include "math.h"

static constexpr float _PI_ = 3.141592f;
static constexpr int TFT_DARKRED = 0x3800;
static constexpr int TFT_DARKBLUE = 0x0007;
static constexpr int TFT_DARKERGREEN = 0x1E00;
static constexpr int TFT_DARKORANGE = 0x03860;
static constexpr int TFT_DARKYELLOW = 0x039E0;
static constexpr int color_on[] = {TFT_RED, TFT_BLUE, TFT_GREEN, TFT_ORANGE, TFT_YELLOW};
constexpr int color_off[] = {TFT_DARKRED, TFT_DARKBLUE, TFT_DARKERGREEN, TFT_DARKORANGE, TFT_DARKYELLOW };
constexpr int sizecolors = sizeof(color_off) / sizeof(int);

static float ang2rad(int angle) {
	angle = (360 - angle) % 360;
	return _PI_ * 2 * angle  /  360;
}

LcdTask* LcdTask::_pThis = nullptr;

LcdTask::LcdTask(int priority) { 
	_pThis = this; 
	dispQue = xQueueCreate(5, sizeof(DISPCMD));
	xTaskCreate( DoTask, "LcdTask", 4096, this, priority, NULL);
	this->_maxRpm = 100;
	this->_sAngle = 225;
	this->_eAngle = -45;
}

char gszWork[16];

//#define FONT24	fonts::lgfxJapanMincho_24
//#define FONT16  fonts::lgfxJapanMincho_16
//#define FONT24	fonts::FreeMono12pt7b
//#define FONT16  fonts::FreeMono9pt7b
#define FONT24	fonts::lgfxJapanMincho_24
#define FONT16  fonts::lgfxJapanMincho_24

void LcdTask::DoTask(void* pthis) {  ((LcdTask *)pthis)->DoTask();  }

void  LcdTask::SetRpmParam(int maxrpm, int starta, int enda) {
	this->_maxRpm = maxrpm;
	this->_sAngle = starta;
	this->_eAngle = enda;
}

/**
 * @brief Draw rpm meter base (arc, and rpm values)
 * 
 */
void LcdTask::DrawMeterBase() {
	int width = _plcd->getPanel()->getWidth();
	int height = _plcd->getPanel()->getHeight();
	int ofx = 0, ofy = 0, cx, cy,  r;
	char szValue[16];
	if(width > height) {
		ofx = (width - height) / 2;
		width = height;
	} else {
		ofy = (height - width) / 2;
		height = width;
	}
	r = width / 2;
	cx = ofx + r;
	cy = ofx + r;
	_plcd->fillCircle(cx, cy,  r, TFT_WHITE);	// Fill white inside square region to draw meter
	// library's angle is iverted from real dimention, so need to adjust (360 - value)
	_plcd->drawArc(cx, cy,  r - 2, r - 2, this->_sAngle - 90, this->_eAngle + 90, TFT_BLACK);
	//_plcd->drawArc(ofx + r, ofy + r,  r - 2, r - 2, 135, 45, TFT_BLACK);

	int endDrgree = _eAngle > 270 ? _eAngle - 360 : _eAngle;
	int range = this->_sAngle - endDrgree;
	int r2 = r * 9 / 10;
	int r3 = r * 8 / 10;
	float frange = _PI_ * range / 180;
	int nUnit =  this->_maxRpm / 10;		// unit  for each 1000 rpm	1000回転単位で何マスできる？
	float fpitch = frange  / nUnit;				// pitch for each 1000 rpm
	float fangle = ang2rad(_sAngle);
	printf("maxrpm=%d, degree1=%d, degree2=%d\n", _maxRpm, _sAngle, endDrgree);
	_plcd->setFont(&FONT24);
	_plcd->setTextColor(TFT_BLACK, TFT_WHITE);
	for(int i = 0; i <= nUnit ; i++) {
		float yval = sin(fangle);
		float xval = cos(fangle);

		int  x1 = (int)(xval * r) + cx; 
		int  y1 = (int)(yval * r) + cy; 
		int  x2 = (int)(xval * r2) + cx; 
		int  y2 = (int)(yval * r2) + cy; 
		int  x3 = (int)(xval * r3) + cx - 10; 
		int  y3 = (int)(yval * r3) + cy - 10; 

		_plcd->drawLine(x1, y1, x2, y2, TFT_BLACK);

		sprintf(szValue, "%d", i);
		_plcd->setCursor(x3, y3);
		_plcd->print(szValue);

		fangle += fpitch;
	}
}

///!--------------------------------------------------------------------------
//!
//!
//!
///!--------------------------------------------------------------------------
void LcdTask::DrawMeterNeedle(int rpm, int prev, int cx, int cy, int r) {
	int eAngle = _eAngle > 270 ? _eAngle - 360 : _eAngle;
	int range = this->_sAngle - eAngle;		// angle range by degree 。　角度範囲、°単位
	int oldAngle = this->_sAngle - range * prev / this->_maxRpm / 100;
	int newAngle = this->_sAngle - range * rpm /  this->_maxRpm / 100;
	float oldfangle = ang2rad(oldAngle);
	float oldfa2 = oldfangle + _PI_ / 2;
	float oldfa3 = oldfangle - _PI_ / 2;
	float newfangle = ang2rad(newAngle);
	float newfa2 = oldfangle + _PI_ / 2;
	float newfa3 = oldfangle - _PI_ / 2;

	int needle_len = r * 2 / 3;
	int lenbttom = r / 12;

	int  x = cos(oldfangle) * needle_len + cx;
	int  y = sin(oldfangle) * needle_len + cy;
	int  x2 = cos(oldfa2) * lenbttom + cx;
	int  y2 = sin(oldfa2) * lenbttom + cy;
	int  x3 = cos(oldfa3) * lenbttom + cx;
	int  y3 = sin(oldfa3) * lenbttom + cy;

	_plcd->fillTriangle(x, y, x2, y2, x3, y3, TFT_WHITE);

	x = cos(newfangle) * needle_len + cx;
	y = sin(newfangle) * needle_len + cy;
	x2 = cos(newfa2) * lenbttom + cx;
	y2 = sin(newfa2) * lenbttom + cy;
	x3 = cos(newfa3) * lenbttom + cx;
	y3 = sin(newfa3) * lenbttom + cy;

	_plcd->fillTriangle(x, y, x2, y2, x3, y3, TFT_BLUE);
}

//!--------------------------------------------------------------------------//
//! Display task on TFT screen
//!	液晶表示タスク
//!--------------------------------------------------------------------------//
void LcdTask::DoTask() {

	constexpr int LEDOFS = 5;
	constexpr int  LEDMERGINE = 2;
	constexpr int  LEDSIZE	= 24;
	constexpr int  LEDR = (LEDSIZE / 2);

	_plcd = new LGFX();
	_plcd->init();
	//_plcd->setRotation(1);
	_plcd->fillScreen(TFT_WHITE);

	int width = _plcd->getPanel()->getWidth();
	int height = _plcd->getPanel()->getHeight();
	int mwidth = width, mheight = height;
	int ofx = 0, ofy = 0, cx, cy,  r, rpm = 0, prevrpm = 0, work;
	char szValue[16];
	uint32_t tick1s = 0;		// idle count

	if(width > height) {
		ofx = (width - height) / 2;
		mwidth = mheight;
	} else {
		ofy = (height - width) / 2;
		mheight = mwidth;
	}
	r = mwidth / 2;				// radius for meter . メーターの半径
	cx = ofx + r;
	cy = ofx + r;

	DrawMeterBase();
	DrawMeterNeedle(rpm, prevrpm, cx, cy, r);

#define LEDPOSX(n)		(n > 1 ? (n - 2)*(LEDSIZE + LEDMERGINE) + LEDR : n == 1 ? (mwidth - LEDOFS - LEDR) : LEDOFS + LEDR)
#define LEDPOSY(n)		(n  < 2 ? LEDR + LEDOFS : height - LEDOFS - LEDR)

	_plcd->fillCircle(LEDPOSX(0), LEDPOSY(0), LEDR, color_off[0] );
	_plcd->fillCircle(LEDPOSX(1), LEDPOSY(1), LEDR, color_off[1] );
	DEBUG_PRINT("first draw was done\n");	
	while (1) {
		DISPCMD  evt;
		if(xQueueReceive(dispQue, &evt, pdMS_TO_TICKS(1000))) {
//			DEBUG_PRINT("LCD TASK called evt.cmd=%d, subcmd=%d\n", evt.cmd, evt.subcmd);
			switch(evt.cmd) {
				case 	CMD_LED: {
					_plcd->fillCircle(LEDPOSX(evt.subcmd), LEDPOSY(evt.subcmd), LEDR,  evt.state ? color_on[evt.coloridx % sizecolors] : color_off[evt.coloridx % sizecolors] );
				}
					break;
				case CMD_DISP_RPM: {
					rpm = evt.state;
					if(rpm != prevrpm) {
						DrawMeterNeedle(rpm, prevrpm, cx, cy, r);
						sprintf(szValue, "% 5d", rpm);
						_plcd->setFont(&FONT24);
						_plcd->setTextColor(TFT_BLACK, TFT_WHITE);
						_plcd->setCursor(cx - 25, height - 40);
						_plcd->print(szValue);
						tick1s = 0;
						prevrpm = rpm;
					}
				}
					break;
			}
		} else {
			tick1s++;
			if(rpm > 0 && tick1s > 10)
				rpm = 0;
			if(rpm != prevrpm)
				DrawMeterNeedle(rpm, prevrpm, cx, cy, r);
				/*
			sprintf(szValue, "% 5d", tick1s);
			_plcd->setFont(&FONT24);
			_plcd->setTextColor(TFT_BLACK, TFT_WHITE);
			_plcd->setCursor(cx - 25, height - 40);
			_plcd->print(szValue);
			*/
			prevrpm = rpm;
		}
	}
}


void  LcdTask::ShowRpmValue(int rpm) {
	DISPCMD sevt;
	sevt.state = rpm;
	sevt.cmd = CMD_DISP_RPM;
    xQueueSend(dispQue, &sevt, portTICK_PERIOD_MS);
}