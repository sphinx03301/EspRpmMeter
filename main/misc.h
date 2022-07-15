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
	CMD_LED_PON = 1,
	CMD_LED_WIPER = 2,
	CMD_LED_4TH = 3,
	CMD_DISP_JAPANESE = 6,
	CMD_DISP_VALUE = 7,
	CMD_SLED1,
	CMD_SLED2 = 9,
	CMD_DISP_RPM,
};

class DISPCMD{
public:	
	int		cmd;    // 1 ~ 5 LED, 6 Disp Japanese, 7 Disp value
	int		state;  // LED ON or Not
	int   	subcmd;
	char	strings[28];   // String value to display when cmd 6 or 7;
	void set(int c, int s, int sub) { cmd = c;  state = s;  subcmd = sub;   }
	DISPCMD() { strings[0] = 0; }
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
	uint8_t pwmlevel;	// PWM出力のDUTY。8bit
	uint8_t centerlo;
	uint8_t dummy[2];
	uint8_t client_mac[6];	// 通信相手のMACアドレス
	uint8_t dummy2[2];
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

class GPIODATA {
public:
	void (*pcallback)(GPIODATA*) ;
	gpio_num_t	gpionum;
	gpio_int_type_t	inttypes[2];
	uint8_t	idxCur;
	static QueueHandle_t  evtQue;
	void setValues(gpio_num_t, gpio_int_type_t, gpio_int_type_t, uint8_t);
	GPIODATA() { pcallback = nullptr; }
};

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
#endif
#endif