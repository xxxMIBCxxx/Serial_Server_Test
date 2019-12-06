#pragma once
//*****************************************************************************
// CSerialRecvThreadクラス
//*****************************************************************************
#include "CThread.h"
#include "CEvent.h"
#include "CLog.h"
#include "CConfFile.h"
#include "CMutex.h"
#include "list"


class CSerialRecvThread : public CThread
{
public:
	// シリアル受信スレッドクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,										// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,										// スレッド開始に失敗しました

		RESULT_ERROR_SYSTEM = 0xE9999999,										// システムエラー
	} RESULT_ENUM;


	// シリアル受信スレッドクラスのパラメータ
	typedef struct
	{
		CLog*									pcLog;							// CLogクラス
		int										SerialFd;						// シリアル通信用ファイルディスクリプタ

	} CLASS_PARAM_TABLE;


private:
	bool										m_bInitFlag;					// 初期化完了フラグ
	int											m_ErrorNo;						// エラー番号
	int											m_epfd;							// epollファイルディスクリプタ

	CLASS_PARAM_TABLE							m_tClassParam;					// シリアル受信スレッドクラスのパラメータ
	std::list<int>								m_SerialRecvDataList;			// シリアル通信受信データリスト
	CMutex										m_cSerialRecvDataListMutex;		// シリアル通信受信データリスト用ミューテックス
	CEvent										m_cSerialRecvDataListEvent;		// シリアル通信受信データリスト用イベント



public:
	CSerialRecvThread(CLASS_PARAM_TABLE& tClassParam);
	~CSerialRecvThread();


	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	
private:	
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);

};







