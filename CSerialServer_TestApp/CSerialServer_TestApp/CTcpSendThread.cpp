//*****************************************************************************
// TCP通信送信スレッドクラス
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "CTcpSendThread.h"


#define _CTCP_SEND_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CTcpSendThread::CTcpSendThread(CLIENT_INFO_TABLE& tClientInfo)
{
	bool						bRet = false;
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_tClientInfo = tClientInfo;

	// TCP送信要求イベントの初期化
	eEventRet = m_cSendRequestEvent.Init(EFD_SEMAPHORE);
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// TCP送信要求リストのクリア
	m_SendRequestList.clear();


	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CTcpSendThread::~CTcpSendThread()
{
	// TCP通信送信スレッド停止し忘れ考慮
	this->Stop();

	// 再度、TCP送信要求リストをクリアする
	SendRequestList_Clear();
}


//-----------------------------------------------------------------------------
// エラー番号を取得
//-----------------------------------------------------------------------------
int CTcpSendThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// TCP通信送信スレッド開始
//-----------------------------------------------------------------------------
CTcpSendThread::RESULT_ENUM CTcpSendThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが動作している場合
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::Start - Thread Active.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// TCP通信送信スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CTcpSendThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP通信送信スレッド停止
//-----------------------------------------------------------------------------
CTcpSendThread::RESULT_ENUM CTcpSendThread::Stop()
{
	bool						bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// TCP通信送信スレッド停止
	CThread::Stop();

	// TCP送信要求リストをクリアする
	SendRequestList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP通信送信スレッド
//-----------------------------------------------------------------------------
void CTcpSendThread::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;


	// スレッドが終了する際に呼ばれる関数を登録
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epollファイルディスクリプタ生成
	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
	if (m_epfd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CTCP_SEND_THREAD_DEBUG_
		perror("CTcpSendThread::ThreadProc - epoll_create");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return;
	}

	// スレッド終了要求イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->GetThreadEndReqEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->GetThreadEndReqEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CTCP_SEND_THREAD_DEBUG_
		perror("CTcpSendThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return;
	}

	// TCP送信要求イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_cSendRequestEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_cSendRequestEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CTCP_SEND_THREAD_DEBUG_
		perror("CTcpSendThread::ThreadProc - epoll_ctl[SendRequestEvent]");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return;
	}

	// スレッド開始イベントを送信
	this->m_cThreadStartEvent.SetEvent();

	// ▼--------------------------------------------------------------------------▼
	// スレッド終了要求が来るまでループ
	// ※勝手にループを抜けるとスレッド終了時にタイムアウトで終了となるため、スレッド終了要求以外は勝手にループを抜けないでください
	while (bLoop)
	{
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, -1);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
#ifdef _CTCP_SEND_THREAD_DEBUG_
			perror("CTcpSendThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
			continue;
		}
		else if (nfds == 0)
		{
			continue;
		}

		for (int i = 0; i < nfds; i++)
		{
			// スレッド終了要求イベント受信
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// TCP送信要求イベント受信
			else if (tEvents[i].data.fd == this->m_cSendRequestEvent.GetEventFd())
			{
				// TCP送信処理
			}
		}
	}

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// TCP通信送信スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CTcpSendThread::ThreadProcCleanup(void* pArg)
{
	// パラメータチェック
	if (pArg == NULL)
	{
		return;
	}
	CTcpSendThread* pcTcpSendThread = (CTcpSendThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcTcpSendThread->m_epfd != -1)
	{
		close(pcTcpSendThread->m_epfd);
		pcTcpSendThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// TCP送信要求リストをクリアする
//-----------------------------------------------------------------------------
void CTcpSendThread::SendRequestList_Clear()
{
	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cSendRequestListMutex.Lock();

	std::list<SEND_REQUEST_TABLE>::iterator		it = m_SendRequestList.begin();
	while (it != m_SendRequestList.end())
	{
		SEND_REQUEST_TABLE			tSendRequest = *it;

		// バッファが確保されている場合
		if (tSendRequest.pSendData != NULL)
		{
			// バッファ領域を解放
			free(tSendRequest.pSendData);
		}
		it++;
	}

	// TCP送信要求リストをクリア
	m_SendRequestList.clear();

	m_cSendRequestListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲
}


//-----------------------------------------------------------------------------
// TCP送信要求データ設定
//-----------------------------------------------------------------------------
CTcpSendThread::RESULT_ENUM CTcpSendThread::SetSendRequestData(SEND_REQUEST_TABLE& tSendReauest)
{
	bool					bRet = false;


	// 引数チェック
	if (tSendReauest.pSendData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::SetSendRequestData - Not Init Proc.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cSendRequestListMutex.Lock();

	// TCP送信要求リストに登録
	m_SendRequestList.push_back(tSendReauest);

	m_cSendRequestListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	// TCP通信送信スレッドにTCP送信要求イベントを送信する
	m_cSendRequestEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP送信要求データ取得
//-----------------------------------------------------------------------------
CTcpSendThread::RESULT_ENUM CTcpSendThread::GetSendRequestData(SEND_REQUEST_TABLE& tSendReauest)
{
	bool					bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::GetSendRequestData - Not Init Proc.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cSendRequestListMutex.Lock();

	// TCP送信要求リストに登録データある場合
	if (m_SendRequestList.empty() != true)
	{
		// TCP受信応答リストの先頭データを取り出す（※リストの先頭データは削除）
		std::list<SEND_REQUEST_TABLE>::iterator		it = m_SendRequestList.begin();
		tSendReauest = *it;
		m_SendRequestList.pop_front();
	}

	m_cSendRequestListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	// TCP受信応答データを取得したので、TCP送信要求イベントをクリアする
	m_cSendRequestEvent.ClearEvent();

	return RESULT_SUCCESS;
}