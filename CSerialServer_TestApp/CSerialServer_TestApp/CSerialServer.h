#pragma once
//*****************************************************************************
// SerialServerクラス
//*****************************************************************************
#include "CThread.h"
#include "CEvent.h"
#include "CServerConnectMonitoringThread.h"

class CSerialServer : public CThread
{
public:
	// SerialServerクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,										// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,										// スレッド開始に失敗しました

		RESULT_ERROR_PARAM = 0xE1000001,										// パラメータエラー
		RESULT_ERROR_CREATE_SOCKET = 0xE1000002,								// ソケット生成に失敗
		RESULT_ERROR_CONNECT = 0xE1000003,										// 接続に失敗
		RESULT_ERROR_LISTEN = 0xE1000004,										// 接続待ちに失敗

		RESULT_ERROR_SYSTEM = 0xE9999999,										// システムエラー
	} RESULT_ENUM;




private:
	bool									m_bInitFlag;						// 初期化完了フラグ
	int										m_ErrorNo;							// エラー番号
	int										m_epfd;								// epollファイルディスクリプタ

	CServerConnectMonitoringThread*			m_pcServerConnectMonitoringThread;	// 接続監視スレッド


public:
	CSerialServer();
	~CSerialServer();
	int GetErrorNo();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
};




