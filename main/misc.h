#ifndef _MISC_H_
#define _MISC_H_
#ifdef __cplusplus
#include "config.h"
#include "esp_system.h"
#include "hal/gpio_types.h"
extern "C" {
#endif
/**
 * IOポート制御リクエスト用データ
 */
typedef struct GPIOSTS{
	int			gpio;
	int			state;
	int			dummy;
}GPIOSTS, *PGPIOSTS;

/**
 * @brief DISPCMD.cmdのための定数
 * 
 */
enum DISPCMD_CMD {
	CMD_LED = 1,
	CMD_SLED,
	CMD_DISP_RPM,
};

class DISPCMD{
public:	
	int16_t	cmd;    	// @see DISPCMD_CMD
	int16_t	state;  	// for example, LED ON or OFF
	int16_t subcmd;
	int16_t coloridx;	//
	const char*	strings;   // String value to display when cmd 6 or 7;
	void set(int c, int s, int sub) { cmd = c;  state = s;  subcmd = sub;   }
	DISPCMD() { strings = nullptr; }
};
typedef DISPCMD *PDISPCMD;

/**
 * 	ジョイスティックとその他の設定値を保存するためのクラス
 * ジョイスティックは標準のPWMで50Hz、1.0ms ～ 2.0ms, 1.5ms をニュートラルという仕様に準ずる
 * ただし設定でニュートラル=0で、2.0ms ～ 1.0ms ～ 1.5ms という中央を最小値にも変更できるものとする
 * */
#ifdef __cplusplus
class SETTINGD {
public:
#else
typedef struct  SETTINGD {
#endif	
	int		htop;		// Joystic水平方向キャリブレーションの最大値(3285(mv)デフォルト)
	int		hbottom;	// (0 default)
	int		vtop;		// Joystic垂直方向キャリブレーションの最大値
	int		vbottom;	// (0 default)
	int		hmax;		// 水平方向最大時間(10μ秒単位) 180 ～ 250 (デフォルト200)
	int		hmin;		// 水平方向最少時間(10μ秒単位) 50 ～ 120 (デフォルト100)
	int		vmax;		// 垂直方向最大時間(10μ秒単位) 180 ～ 250 (デフォルト200)
	int		vmin;		// 垂直方向最少時間(10μ秒単位) 50 ～ 120 (デフォルト100)
	int 	nrange;		// ニュートラルの幅(mv単位)
	int		wiperon;	// ワイパーオン時間。1m秒単位(設定は100ミリ秒単位)
	int		wiperoff;	// ワイパーオフ時間。
	int     maxrpm;
	int     sangle;
	int     eangle;
	uint8_t pwmlevel;	// PWM出力のDUTY。8bit
	uint8_t centerlo;
	uint8_t dummy[2];
#ifdef __cplusplus
	SETTINGD() = default;
	SETTINGD(const SETTINGD& src) = default;
	void SetDefault();
	SETTINGD& operator =(const SETTINGD&) = default;
	void Initialize();
	void InflateMinMax(int v, int h);
	void RestoreMinMax(const SETTINGD& org);
	void RestoreCenterLo(const SETTINGD& org);
	void RestoreHRange(const SETTINGD& org);
	void RestoreVRange(const SETTINGD& org);
	void RestoreWiper(const SETTINGD& org);
	void RestoreNrange(const SETTINGD& org);
	void UpdateHRange(uint8_t flag);
	void UpdateVRange(uint8_t flag);
	void UpdateNRange(uint8_t flag);
	void UpdateWRange(uint8_t flag);
	void UpdateCenterLo(uint8_t flag);
};	
#else
} SETTINGD, *PSETTINGD;
#endif

#define	PORT_ON		0
#define	PORT_OFF	1

/**
 * 	現在の動作モードを示す定数
 * Constants represents current mode
 * */
enum FUNCMODE {
	NONE = 0,
	PUSHWIPER,
	AUTOWIPER,
	INTERMWIPER,
	SET_MINMAX,
	SET_CENTERM,
//	SET_VRANGE,
//	SET_HRANGE,
	SET_NRANGE,
	SET_WINTERVAL,
};

// サーバ側へ通信する時の１バイト目のコマンド
enum CMDMODE {
	CMD_MAINSWITCH = 1,		// メインスイッチのオンオフ指定コマンド
	CMD_SUBSWITCH = 2,		// PWM補助出力のオンオフ指定コマンド
	CMD_DUTYSEND = 7,		//　メインのPPM信号出力コマンド
	CMD_SUBDUTY = 8			// PWM出力のDUTY指定。8ビットresolution
};

/**
 * @brief   IOPortSettingData
 * Data structure for handling IO Ports
 */
typedef struct GPIODATA {
	// for relating data, or this for C++
	// 関連データを保持するための変数
	void *pData;
	void *pData2;		// ex 	
	// to keep address of callback function after interrupt was called
	// 割り込み処理から呼び出す関数のアドレス	
	void (*pcallback)(GPIODATA*);
	gpio_num_t	gpionum;
	// to specify witch intrrupt type
	// 割込みのエッジを指定する。再開するときに使う
	gpio_int_type_t	inttypes[2];
	uint8_t	idxCur;
	static QueueHandle_t  evtQue;
	void setValues(void* param, void (*pcfunc)(GPIODATA*), gpio_num_t, gpio_int_type_t, gpio_int_type_t, uint8_t);
	GPIODATA() { pcallback = nullptr; }
	GPIODATA(void* param, void (*pcfunc)(GPIODATA*), gpio_num_t, gpio_int_type_t it1 = GPIO_INTR_NEGEDGE, gpio_int_type_t it2 = GPIO_INTR_NEGEDGE, uint8_t idx = 0);
} GPIODATA;

enum ERECT {
	ER_LEFT = 1,
	ER_RIGHT = 2,
	ER_BOTTOM = 4,
	ER_TOP = 8
};

//------------------------------------------------------
// 	JoyStickのアナログ操作から、デジタルスイッチとしての
//  動作を値化するためのクラス
//------------------------------------------------------
class JoystickLogic {
public:	
	JoystickLogic() : flag(0), state(0) {}
	void SetRange(int left, int bottom, int right, int top );
	void AddAction(int h, int v);
	uint8_t  GetAction();
	uint8_t  GetActionC();
	void Clear() {  flag = 0;  state = 0;  }
protected:
	uint8_t flag, state;
	int topl,
		bottoml,
		rightl,
		leftl;	
};

#ifdef __cplusplus
}

inline void dummyfunc(const char*fmt, ...) {}
#if DEBUG_MAIN
#define DEBUG_PRINT(fmt, ...) printf(fmt,  ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) dummyfunc(fmt,  ##__VA_ARGS__)
#endif

#endif
#endif