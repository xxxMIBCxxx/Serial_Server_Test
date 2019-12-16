//*****************************************************************************
// Threadクラス
// ※リンカオプションに「-pthread」を追加すること
//*****************************************************************************
#include "CThread.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>


#define _CTHREAD_DEBUG_
#define	CTHREAD_START_TIMEOUT				( 3 * 1000 )			// スレッド開始待ちタイムアウト(ms)
#define	CTHREAD_END_TIMEOUT					( 3 * 1000 )			// スレッド終了待ちタイムアウト(ms)
#define EPOLL_MAX_EVENTS					( 10 )					// epoll最大イベント


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CThread::CThread(const char* pszId)
{
	CEvent::RESULT_ENUM			eRet = CEvent::RESULT_SUCCESS;


	m_strId = "";
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_hThread = 0;
//	m_epfd = -1;
	
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
//	int							iRet = 0;
//	struct epoll_event			tEvent;
//	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
//	bool						bLoop = true;
//	struct tm					tTm;
//	char						szTime[64 + 1];
//	struct timespec				tTimeSpec;
//
//
//	// スレッドが終了する際に呼ばれる関数を登録
//	pthread_cleanup_push(ThreadProcCleanup, this);
//
//	// epollファイルディスクリプタ生成
//	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
//	if (m_epfd == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CTHREAD_DEBUG_
//		perror("CThread::ThreadProc - epoll_create");
//#endif	// #ifdef _CTHREAD_DEBUG_
//		return;
//	}
//
//	// スレッド終了要求イベントを登録
//	memset(&tEvent, 0x00, sizeof(tEvent));
//	tEvent.events = EPOLLIN;
//	tEvent.data.fd = this->GetThreadEndReqEventFd();
//	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->GetThreadEndReqEventFd(), &tEvent);
//	if (iRet == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CTHREAD_DEBUG_
//		perror("CThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
//#endif	// #ifdef _CTHREAD_DEBUG_
//		return;
//	}
//
//#ifdef _CTHREAD_DEBUG_
//	printf("-- Thread %s Start --\n", m_strId.c_str());
//#endif	// #ifdef _CTHREAD_DEBUG_
//	// スレッド開始イベントを送信
//	m_cThreadStartEvent.SetEvent();
//
//	// ▼--------------------------------------------------------------------------▼
//	// スレッド終了要求が来るまでループ
//	// ※勝手にループを抜けるとスレッド終了時にタイムアウトで終了となるため、スレッド終了要求以外は勝手にループを抜けないでください
//	while (bLoop)
//	{
//		memset(tEvents, 0x00, sizeof(tEvents));
//		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, 1000);
//		if (nfds == -1)
//		{
//			m_ErrorNo = errno;
//#ifdef _CTHREAD_DEBUG_
//			perror("CThread::ThreadProc - epoll_wait");
//#endif	// #ifdef _CTHREAD_DEBUG_
//			bLoop = false;
//			continue;
//		}
//		else if (nfds == 0)
//		{
//			// 現在時刻を取得
//			memset(szTime, 0x00, sizeof(szTime));
//			clock_gettime(CLOCK_REALTIME, &tTimeSpec);
//			localtime_r(&tTimeSpec.tv_sec, &tTm);
//			sprintf(szTime, "[%04d/%02d/%02d %02d:%02d:%02d.%03ld]",
//				tTm.tm_year + 1900,
//				tTm.tm_mon + 1,
//				tTm.tm_mday,
//				tTm.tm_hour,
//				tTm.tm_min,
//				tTm.tm_sec,
//				tTimeSpec.tv_nsec / 1000000);
//
//#ifdef _CTHREAD_DEBUG_
//			printf("%s : CThread::ThreadProc.\n", szTime);
//#endif	// #ifdef _CTHREAD_DEBUG_
//			continue;
//		}
//
//		for (int i = 0; i < nfds; i++)
//		{
//			// スレッド終了要求イベント受信
//			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
//			{
//				bLoop = false;
//				break;
//			}
//		}
//	}
//	// ▲--------------------------------------------------------------------------▲
//
//#ifdef _CTHREAD_DEBUG_
//	printf("-- Thread %s End --\n", m_strId.c_str());
//#endif	// #ifdef _CTHREAD_DEBUG_
//	// スレッド終了イベントを送信
//	m_cThreadEndEvent.SetEvent();
//
//	pthread_cleanup_pop(1);
}


////-----------------------------------------------------------------------------
//// スレッド終了時に呼ばれる処理
////-----------------------------------------------------------------------------
//void CThread::ThreadProcCleanup(void* pArg)
//{
//	CThread* pcThread = (CThread*)pArg;
//
//
//	// epollファイルディスクリプタ解放
//	if (pcThread->m_epfd != -1)
//	{
//		close(pcThread->m_epfd);
//		pcThread->m_epfd = -1;
//	}
//}