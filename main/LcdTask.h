#ifndef _LCDTASK_H_
#define  _LCDTASK_H_
#define LOVYANGFX_CONFIG_HPP_
#include "LovyanGFX.hpp"
#include "LGFX_Config_MyCustom.hpp"
#ifdef __cplusplus
extern "C" {
#endif

class LcdTask {
private:
	uint8_t  _brinkon;
	static LcdTask	*_pThis;
protected:
	uint8_t		_curMode;
	QueueHandle_t  dispQue;
	LGFX 		*_plcd;
	int			_maxRpm,			// Maximum rotation per minutes (100 rpm unit)
				_sAngle,			// Rotaion angle for 0 rpm
				_eAngle;			// Rotaion angle for max rpm
	void DoTask();
	void DrawMeterBase();
	void DrawMeterNeedle(int rpm, int prev, int cx, int cy, int r);
	static void DoTask(void*);
public:
	LcdTask(int priority);
	void ShowRpmValue(int rpm);
	void SetRpmParam(int maxrpm, int starta, int enda);
	void Update();
};
#ifdef __cplusplus
}
#endif

#endif