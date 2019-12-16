#pragma once
//*****************************************************************************
// Threadクラス
// ※リンカオプションに「-pthread」を追加すること
//*****************************************************************************
#include <cstdio>
#include <pthread.h>
#include <errno.h> 
#include <string.h>
#include "string"
#include "CEvent.h"


// スレッドの呼出し関数定義
typedef void* (*PTHREAD_LAUNCHER_FUNC) (void*);


// CThreadクラス定義
class CThread
{
public:
	// CThreadクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,						// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,					// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,			// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,					// スレッド開始に失敗しました
		RESULT_ERROR_START_TIMEOUT = 0xE0000004,			// スレッド開始タイムアウト


		RESULT_ERROR_SYSTEM = 0xE9999999					// システム異常
	} RESULT_ENUM;


private:
	std::string						m_strId;				// 識別名
	bool							m_bInitFlag;			// 初期化フラグ
	int								m_ErrorNo;				// エラー番号
//	int								m_epfd;					// epollファイルディスクリプタ

	pthread_t						m_hThread;				// スレッドハンドル

public:
	CEvent							m_cThreadStartEvent;	// スレッド開始イベント
	CEvent							m_cThreadEndReqEvent;	// スレッド終了要求イベント
	CEvent							m_cThreadEndEvent;		// スレッド終了イベント

public:
	CThread(const char* pszId = NULL);
	virtual ~CThread();
	CThread::RESULT_ENUM Start();
	CThread::RESULT_ENUM Stop();
	int GetErrorNo();
	bool IsActive();
	int GetThreadStartEventFd();
	int GetThreadEndReqEventFd();
	int GetThreadEndEventFd();
	virtual void ThreadProc();

private:
	static void* ThreadLauncher(void* pUserData);
//	static void ThreadProcCleanup(void* pArg);
};

