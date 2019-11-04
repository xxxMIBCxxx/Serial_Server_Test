//*****************************************************************************
// サーバー応答スレッドクラス
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "CServerResponseThread.h"


#define _CSERVER_RESPONSE_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CServerResponseThread::CServerResponseThread(CLIENT_INFO_TABLE& tClientInfo, CEvent* pcServerResponceThreadEndEvent)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	memset(&m_tClientInfo, 0x00, sizeof(m_tClientInfo));
	memset(m_szIpAddr, 0x00, sizeof(m_szIpAddr));
	m_Port = 0;
	//m_pcTcpSendThread = NULL;
	m_pcTcpRecvThread = NULL;
	m_pcServerResponceThreadEndEvent = NULL;
	m_bServerResponceThreadEnd = false;

	// サーバー応答スレッド終了イベントのチェック
	if (pcServerResponceThreadEndEvent == NULL)
	{
		return;
	}
	m_pcServerResponceThreadEndEvent = pcServerResponceThreadEndEvent;

	// クライアント切断イベントの初期化
	CEvent::RESULT_ENUM eEventRet = m_cClientDisconnectEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// クライアント情報を取得
	memcpy(&m_tClientInfo, &tClientInfo, sizeof(CLIENT_INFO_TABLE));
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tClientInfo.tAddr.sin_addr));			// IPアドレス取得
	m_Port = ntohs(m_tClientInfo.tAddr.sin_port);								// ポート番号取得

	// TCP受信スレッドクラスを生成
	CTcpRecvThread::CLIENT_INFO_TABLE		tTcpRecvClientInfo;
	tTcpRecvClientInfo.Socket = tClientInfo.Socket;
	tTcpRecvClientInfo.tAddr = tClientInfo.tAddr;
	m_pcTcpRecvThread = (CTcpRecvThread*)new CTcpRecvThread(tTcpRecvClientInfo);
	if (m_pcTcpRecvThread == NULL)
	{
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CServerResponseThread::~CServerResponseThread()
{
	// サーバー応答スレッド停止漏れを考慮
	this->Stop();

	// TCP受信スレッドクラスを破棄
	if (m_pcTcpRecvThread != NULL)
	{
		delete m_pcTcpRecvThread;
		m_pcTcpRecvThread = NULL;
	}

	// クライアント側のソケットを解放
	if (m_tClientInfo.Socket != -1)
	{
		close(m_tClientInfo.Socket);
		m_tClientInfo.Socket = -1;
	}
}


//-----------------------------------------------------------------------------
// サーバー応答スレッド開始
//-----------------------------------------------------------------------------
CServerResponseThread::RESULT_ENUM CServerResponseThread::Start()
{
	bool						bRet = false;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが動作している場合
	bRet = this->IsActive();
	if (bRet == true)
	{
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// TCP受信スレッド開始
	CTcpRecvThread::RESULT_ENUM eTcpRecvThreadResult = m_pcTcpRecvThread->Start();
	if (eTcpRecvThreadResult != CTcpRecvThread::RESULT_SUCCESS)
	{
		m_ErrorNo = m_pcTcpRecvThread->GetErrorNo();
		return (CServerResponseThread::RESULT_ENUM)eTcpRecvThreadResult;
	}

	// サーバー応答スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CServerResponseThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// サーバー応答スレッド停止
//-----------------------------------------------------------------------------
CServerResponseThread::RESULT_ENUM CServerResponseThread::Stop()
{
	bool						bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// TCP受信スレッド停止
	m_pcTcpRecvThread->Stop();

	// サーバー応答スレッド停止
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// サーバー応答スレッド
//-----------------------------------------------------------------------------
void CServerResponseThread::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;
	ssize_t						ReadNum = 0;


	// スレッドが終了する際に呼ばれる関数を登録
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epollファイルディスクリプタ生成
	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
	if (m_epfd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		perror("CServerResponseThread - epoll_create");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
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
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		perror("CServerResponseThread - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		return;
	}

	// クライアント切断イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_pcTcpRecvThread->m_cDisconnectEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_pcTcpRecvThread->m_cDisconnectEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		perror("CServerResponseThread - epoll_ctl[ClientDisconnectEvent]");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		return;
	}

	// TCP受信情報イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_pcTcpRecvThread->m_cRecvInfoEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_pcTcpRecvThread->m_cRecvInfoEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		perror("CServerResponseThread - epoll_ctl[TcpRecvResponseEvent]");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		return;
	}

	// スレッド開始イベントを送信
	this->m_cThreadStartEvent.SetEvent();


	// ▼--------------------------------------------------------------------------▼
	// スレッド終了要求が来るまでループ
	// ※勝手にループを抜けるとスレッド終了時にタイムアウトで終了となるため、スレッド終了要求以外は勝手にループを抜けないでください
	while (bLoop) {
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, -1);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
			perror("CServerResponseThread - epoll_wait");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
			continue;
		}
		// タイムアウト
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
			// クライアント切断イベント
			else if (tEvents[i].data.fd == this->m_pcTcpRecvThread->m_cDisconnectEvent.GetEventFd())
			{
				this->m_pcTcpRecvThread->m_cDisconnectEvent.ClearEvent();

				// サーバー応答スレッド終了フラグを立てて、サーバー応答スレッド終了イベントを送信する
				m_bServerResponceThreadEnd = true;
				m_pcServerResponceThreadEndEvent->SetEvent();
				break;
			}
			// TCP受信情報イベント
			else if (tEvents[i].data.fd == this->m_pcTcpRecvThread->m_cRecvInfoEvent.GetEventFd())
			{
				CTcpRecvThread::RECV_INFO_TABLE		tRecvInfo;
				this->m_pcTcpRecvThread->GetRecvData(tRecvInfo);
				printf("[%s (%d)] - %s\n", this->m_szIpAddr, this->m_Port, tRecvInfo.pData);
				free(tRecvInfo.pData);
			}
		}
	}
	// ▲--------------------------------------------------------------------------▲

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// サーバー応答スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CServerResponseThread::ThreadProcCleanup(void* pArg)
{
	CServerResponseThread* pcServerResponseThread = (CServerResponseThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcServerResponseThread->m_epfd != -1)
	{
		close(pcServerResponseThread->m_epfd);
		pcServerResponseThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// サーバー応答スレッド終了させたいのか調べる
//-----------------------------------------------------------------------------
bool CServerResponseThread::IsServerResponseThreadEnd()
{
	return m_bServerResponceThreadEnd;
}
