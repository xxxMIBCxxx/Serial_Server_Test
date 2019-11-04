//*****************************************************************************
// SerialClientクラス
//*****************************************************************************
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <memory.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "CSerialClient.h"


#define _CSERIAL_CLIENT_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント
#define EPOLL_TIMEOUT_TIME							( 500 )						// サーバー再接続時間（epollタイムアウト時間(ms)）


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CSerialClient::CSerialClient()
{
	CEvent::RESULT_ENUM  eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_pcClinentResponseThread = NULL;

	// サーバー切断イベントの初期化
	eEventRet = m_cServerDisconnectEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CSerialClient::~CSerialClient()
{
	// SerialClientスレッド停止漏れを考慮
	this->Stop();
}


//-----------------------------------------------------------------------------
// SerialClientスレッド開始
//-----------------------------------------------------------------------------
CSerialClient::RESULT_ENUM CSerialClient::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
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

	// SerialClientスレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CSerialClient::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// SerialClientスレッド停止
//-----------------------------------------------------------------------------
CSerialClient::RESULT_ENUM CSerialClient::Stop()
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

	// クライアント応答スレッド停止・解放
	DeleteClientResponseThread();

	// SerialClientスレッド停止
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// SerialClientスレッド
//-----------------------------------------------------------------------------
void CSerialClient::ThreadProc()
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
#ifdef _CSERIAL_CLIENT_DEBUG_
		perror("CSerialClient::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_
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
#ifdef _CSERIAL_CLIENT_DEBUG_
		perror("CSerialClient::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_
		return;
	}

	// サーバー切断イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_cServerDisconnectEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_cServerDisconnectEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERIAL_CLIENT_DEBUG_
		perror("CSerialClient::ThreadProc - epoll_ctl[ServerDisconnectEvent]");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_
		return;
	}

	// クライアント応答スレッド生成・開始
	CreateClientResponseThread();

	// スレッド開始イベントを送信
	this->m_cThreadStartEvent.SetEvent();


	// ▼--------------------------------------------------------------------------▼
	// スレッド終了要求が来るまでループ
	// ※勝手にループを抜けるとスレッド終了時にタイムアウトで終了となるため、スレッド終了要求以外は勝手にループを抜けないでください
	while (bLoop) {
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT_TIME);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
#ifdef _CSERIAL_CLIENT_DEBUG_
			perror("CLandryClient::ThreadProc - epoll_wait");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_
			continue;
		}
		// タイムアウト
		else if (nfds == 0)
		{
			// サーバーと接続していない場合、サーバー接続を試みる
			if (m_pcClinentResponseThread == NULL)
			{
				// クライアント応答スレッド生成・開始
				CreateClientResponseThread();
			}
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
			// サーバー切断イベント
			else if (tEvents[i].data.fd = this->m_cServerDisconnectEvent.GetEventFd())
			{
				m_cServerDisconnectEvent.ClearEvent();
				
				// クライアント応答スレッド停止・解放
				DeleteClientResponseThread();
			}
		}
	}
	// ▲--------------------------------------------------------------------------▲

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// SerialClientスレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CSerialClient::ThreadProcCleanup(void* pArg)
{
	CSerialClient* pcSerialClient = (CSerialClient*)pArg;


	// epollファイルディスクリプタ解放
	if (pcSerialClient->m_epfd != -1)
	{
		close(pcSerialClient->m_epfd);
		pcSerialClient->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// クライアント応答スレッド生成・開始
//-----------------------------------------------------------------------------
CSerialClient::RESULT_ENUM CSerialClient::CreateClientResponseThread()
{
	// 既に生成している場合
	if (m_pcClinentResponseThread != NULL)
	{
		return RESULT_SUCCESS;
	}

	// クライアント応答スレッドクラスの生成
	m_pcClinentResponseThread = (CClientResponseThread*)new CClientResponseThread(&m_cServerDisconnectEvent);
	if (m_pcClinentResponseThread == NULL)
	{
#ifdef _CSERIAL_CLIENT_DEBUG_
		printf("CSerialClient::CreateClientResponseThread - Create CClientResponseThread Error.\n");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}

	// クライアント応答スレッド開始
	CClientResponseThread::RESULT_ENUM	eRet = m_pcClinentResponseThread->Start();
	if (eRet != CClientResponseThread::RESULT_SUCCESS)
	{
#ifdef _CSERIAL_CLIENT_DEBUG_
		//		printf("CLandryClient::CreateClientResponseThread - Start CClientResponseThread Error.\n");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_

		// クライアント応答スレッド停止・解放
		DeleteClientResponseThread();

		return (CSerialClient::RESULT_ENUM)eRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// クライアント応答スレッド停止・解放
//-----------------------------------------------------------------------------
void CSerialClient::DeleteClientResponseThread()
{
	// クライアント応答スレッドクラスの破棄
	if (m_pcClinentResponseThread != NULL)
	{
		delete m_pcClinentResponseThread;
		m_pcClinentResponseThread = NULL;
	}
}

