//*****************************************************************************
// Threadクラス
// ※リンカオプションに「-pthread」を追加すること
//*****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "CThread.h"


#define _CTHREAD_DEBUG_
#define	CTHREAD_START_TIMEOUT				( 3 * 1000 )			// スレッド開始待ちタイムアウト(ms)
#define	CTHREAD_END_TIMEOUT					( 3 * 1000 )			// スレッド終了待ちタイムアウト(ms)


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CThread::CThread(const char* pszId)
{
	int							iRet = 0;
	CEvent::RESULT_ENUM			eRet = CEvent::RESULT_SUCCESS;


	m_strId = "";
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_hThread = 0;
	
	// クラス名を保持
	if (pszId != NULL)
	{
		m_strId = pszId;
	}

	// スレッド開始イベント初期化
	eRet = m_cThreadStartEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// スレッド終了要求イベント
	eRet = m_cThreadEndReqEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// スレッド終了用イベント初期化
	eRet = m_cThreadEndEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CThread::~CThread()
{
	// スレッドが停止していないことを考慮
	this->Stop();
}


//-----------------------------------------------------------------------------
// スレッド開始
//-----------------------------------------------------------------------------
CThread::RESULT_ENUM CThread::Start()
{
	int						iRet = 0;
	CEvent::RESULT_ENUM		eEventRet = CEvent::RESULT_SUCCESS;


	// 初期化処理で失敗している場合
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// 既に動作している場合
	if (m_hThread != 0)
	{
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// スレッド開始
	this->m_cThreadStartEvent.ClearEvent();
	this->m_cThreadEndReqEvent.ClearEvent();
	iRet = pthread_create(&m_hThread, NULL, ThreadLauncher, this);
	if (iRet != 0)
	{
		m_ErrorNo = errno;
#ifdef _CTHREAD_DEBUG_
		perror("CThread::Start - pthread_create");
#endif	// #ifdef _CTHREAD_DEBUG_
		return RESULT_ERROR_START;
	}

	// スレッド開始イベント待ち
	eEventRet = this->m_cThreadStartEvent.Wait(CTHREAD_START_TIMEOUT);
	switch (eEventRet) {
	case CEvent::RESULT_RECIVE_EVENT:			// スレッド開始イベントを受信
		this->m_cThreadStartEvent.ClearEvent();
		break;

	case CEvent::RESULT_WAIT_TIMEOUT:			// タイムアウト
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Start - WaitTimeout\n");
#endif	// #ifdef _CTHREAD_DEBUG_
		pthread_cancel(m_hThread);
		pthread_join(m_hThread, NULL);
		m_hThread = 0;
		return RESULT_ERROR_START_TIMEOUT;

	default:
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Start - Wait Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
		pthread_cancel(m_hThread);
		pthread_join(m_hThread, NULL);
		m_hThread = 0;
		return RESULT_ERROR_SYSTEM;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// スレッド停止
//-----------------------------------------------------------------------------
CThread::RESULT_ENUM CThread::Stop()
{
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;


	// 初期化処理で失敗している場合
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// 既に停止している場合
	if (m_hThread == 0)
	{
		return RESULT_SUCCESS;
	}

	// スレッドを停止させる（スレッド終了要求イベントを送信）
	this->m_cThreadEndEvent.ClearEvent();
	eEventRet = this->m_cThreadEndReqEvent.SetEvent();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Stop - SetEvent Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
		
		// スレッド停止に失敗した場合は、強制的に終了させる
		pthread_cancel(m_hThread);
	}
	else
	{
		// スレッド終了イベント待ち
		eEventRet = this->m_cThreadEndEvent.Wait(CTHREAD_END_TIMEOUT);
		switch (eEventRet) {
		case CEvent::RESULT_RECIVE_EVENT:			// スレッド終了イベントを受信
			this->m_cThreadEndEvent.ClearEvent();
			break;

		case CEvent::RESULT_WAIT_TIMEOUT:			// タイムアウト
#ifdef _CTHREAD_DEBUG_
			printf("CThread::Stop - Timeout\n");
#endif	// #ifdef _CTHREAD_DEBUG_
			pthread_cancel(m_hThread);
			break;

		default:
#ifdef _CTHREAD_DEBUG_
			printf("CThread::Stop - Wait Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
			pthread_cancel(m_hThread);
			break;
		}
	}
	pthread_join(m_hThread, NULL);
	m_hThread = 0;

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// エラー番号を取得
//-----------------------------------------------------------------------------
int CThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// スレッドが動作しているかの確認
//-----------------------------------------------------------------------------
bool CThread::IsActive()
{
	return ((m_hThread != 0) ? true : false);
}


//-----------------------------------------------------------------------------
// スレッド開始イベントファイルディスクリプタを取得
//-----------------------------------------------------------------------------
int CThread::GetThreadStartEventFd()
{
	return m_cThreadStartEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// スレッド終了要求イベントファイルディスクリプタを取得
//-----------------------------------------------------------------------------
int CThread::GetThreadEndReqEventFd()
{
	return m_cThreadEndReqEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// スレッド終了イベントファイルディスクリプタを取得
//-----------------------------------------------------------------------------
int CThread::GetThreadEndEventFd()
{
	return m_cThreadEndEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// スレッド呼び出し
//-----------------------------------------------------------------------------
void* CThread::ThreadLauncher(void* pUserData)
{
	// スレッド処理呼び出し
	reinterpret_cast<CThread*>(pUserData)->ThreadProc();

	return (void *)NULL;
}


//-----------------------------------------------------------------------------
// スレッド処理（※サンプル※）
//-----------------------------------------------------------------------------
void CThread::ThreadProc()
{
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;
	unsigned int				Count = 1;
	unsigned int				Timeout = 0;
	bool						bLoop = true;


	// スレッド開始イベントを送信
	m_cThreadStartEvent.SetEvent();
	printf("-- Thread %s Start --\n", m_strId.c_str());

	// pthread_testcancelが呼ばれるまで処理を続ける
	while (bLoop)
	{
		// Sleep時間を生成
		Timeout = ((rand() % 30) + 1) * 100;

		// スレッド終了要求イベントが通知されるまで待つ
		eEventRet = m_cThreadEndReqEvent.Wait(Timeout);
		switch (eEventRet) {
		case CEvent::RESULT_WAIT_TIMEOUT:
			printf("[%s] Count : %lu \n", this->m_strId.c_str(), Count++);
			break;

		case CEvent::RESULT_RECIVE_EVENT:
			m_cThreadEndReqEvent.ClearEvent();
			bLoop = false;
			break;

		default:
			printf("CEvent::Wait error. \n");
			bLoop = false;
			break;
		}
	}

	// スレッド終了イベントを送信
	m_cThreadEndEvent.SetEvent();
	printf("-- Thread %s End --\n", m_strId.c_str());
}
