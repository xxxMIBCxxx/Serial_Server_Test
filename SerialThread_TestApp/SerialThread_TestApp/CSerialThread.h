#pragma once
//*****************************************************************************
// CSerialThreadクラス
//*****************************************************************************
#include "CThread.h"
#include "CLog.h"
#include "CConfFile.h"
#include "CEvent.h"
#include "CEventEx.h"
#include "CMutex.h"
#include "list"
#include "CSerialRecvThread.h"


#define	SERIAL_DEVICE_NAME						( 64 )


class CSerialThread : public CThread
{
public:
	
	// シリアルスレッドクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,										// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,										// スレッド開始に失敗しました

		RESULT_ERROR_ALREADY_SERIAL_OPEN = 0xE0000004,							// 既にシリアルオープンしている
		RESULT_ERROR_SERIAL_OPEN = 0xE0000005,									// シリアルオープンに失敗しました
		RESULT_ERROR_SERIAL_SET_ATTRIBUTE = 0xE0000006,							// シリアルの属性設定に失敗しました
		RESULT_ERROR_SERIAL_SET_BAUDRATE = 0xE0000007,							// シリアルのボーレート設定に失敗しました
		RESULT_ERROR_THREAD_NOT_ACTIVE = 0xE0000008,							// スレッドが動作していない

		RESULT_ERROR_SYSTEM = 0xE9999999,										// システムエラー
	} RESULT_ENUM;


	// シリアルスレッドクラスのパラメータ
	typedef struct
	{
		CLog*									pcLog;							// CLogクラス
		CConfFile*								pcConfFile;						// ConfFileクラス

	} CLASS_PARAM_TABLE;


	// シリアル通信データ構造体
	typedef struct
	{
		void*									pClas;							// 送信要求側クラスのアドレス
		unsigned int							Size;							// データサイズ
		char*									pData;							// データ（mallocで領域を確保してから、データをセットしてください）
																				// 解放はCSerialThread側で解放(free)します
	} SERIAL_DATA_TABLE;


	// ボーレート変換構造体
	typedef struct
	{
		int				Baudrate;
		int				BaureateDefine;
		const char*		pszBaudrate;
	} BAUDRATE_CONV_TABLE;

	// データ長変換構造体
	typedef struct
	{
		int				DataSize;
		int				DataSizeDefine;
		const char*		pszDataSize;
	} DATA_SIZE_CONV_TABLE;

	// ストップビット変換構造体
	typedef struct
	{
		int				StopBit;
		int				StopBitDefine;
		const char*		pszStopBit;
	} STOP_BIT_CONV_TABLE;


	// パリティ変換構造体
	typedef struct
	{
		int				Parity;
		int				ParityDefine;
		const char*		pszParity;
	} PARITY_CONV_TABLE;


	// シリアル通信設定構造体
	typedef struct
	{
		char									szDevName[SERIAL_DEVICE_NAME + 1];		// デバイス名(ex:/dev/tty***)
		BAUDRATE_CONV_TABLE						tBaudRate;								// ボーレート
		DATA_SIZE_CONV_TABLE					tDataSize;								// データ長
		STOP_BIT_CONV_TABLE						tStopBit;								// ストップビット
		PARITY_CONV_TABLE						tParity;								// パリティビット

	} SERIAL_CONF_TABLE;


private:
	bool										m_bInitFlag;					// 初期化完了フラグ
	int											m_ErrorNo;						// エラー番号
	int											m_epfd;							// epollファイルディスクリプタ
	CLASS_PARAM_TABLE							m_tClassParam;					// シリアルスレッドクラスのパラメータ

	// < IRQシリアル通信データリスト関連 >
	CMutex										m_cIrqSerialDataListMutex;		// IRQシリアル通信データリスト用ミューテックス
	std::list<SERIAL_DATA_TABLE>				m_IrqSerialDataList;			// IRQシリアル通信データリスト


	// < 通常シリアル通信データリスト関連 >
	CMutex										m_cSerialDataListMutex;			// 通常シリアル通信データリスト用ミューテックス
	std::list<SERIAL_DATA_TABLE>				m_SerialDataList;				// 通常シリアル通信データリスト


	SERIAL_CONF_TABLE							m_tSerialConfInfo;				// シリアル通信設定情報
	int											m_SerialFd;						// シリアル通信用ファイルディスクリプタ

	CEvent										m_cSerialDataEvent;				// シリアル通信要求イベント（IRQ・通常共通）
	CEvent										m_cSerialRecvEndEvent;			// シリアル受信完了イベント

	CSerialRecvThread*							m_pcSerialRecvThread;			// シリアル通信受信スレッド


public:
	CSerialThread(CLASS_PARAM_TABLE &tClassParam);
	~CSerialThread();

	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM SetIrqSerialDataList(SERIAL_DATA_TABLE& tSerialData);
	RESULT_ENUM SetSerialDataList(SERIAL_DATA_TABLE& tSerialData);


private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void GetSerialConfInfo(void);
	RESULT_ENUM Open(SERIAL_CONF_TABLE& tSerialConfInfo);
	RESULT_ENUM Close(void);
	bool SerialSendProc(int* pEpfd, struct epoll_event* ptEvents, int MaxEvents, int Timeout);
	void ClearIrqSerialDataList(void);
	void ClearSerialDataList(void);


};