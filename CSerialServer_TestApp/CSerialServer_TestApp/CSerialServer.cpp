//*****************************************************************************
// SerialServerクラス
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
#include "CSerialServer.h"



#define _CSERIAL_SERVER_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CSerialServer::CSerialServer()
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	
	// 接続監視スレッドクラス生成
	m_pcServerConnectMonitoringThread = (CServerConnectMonitoringThread*)new CServerConnectMonitoringThread();
	if (m_pcServerConnectMonitoringThread == NULL)
	{
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CSerialServer::~CSerialServer()
{
	// SerialServerスレッド停止漏れを考慮
	this->Stop();

	// 接続監視スレッドクラス解放
	if (m_pcServerConnectMonitoringThread != NULL)
	{
		delete m_pcServerConnectMonitoringThread;
		m_pcServerConnectMonitoringThread = NULL;
	}
}


//-----------------------------------------------------------------------------
// エラー番号取得
//-----------------------------------------------------------------------------
int CSerialServer::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// SerialServerスレッド開始
//-----------------------------------------------------------------------------
CSerialServer::RESULT_ENUM CSerialServer::Start()
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

	// SerialServerスレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CSerialServer::RESULT_ENUM)eThreadRet;
	}

	// 監視スレッド開始
	CServerConnectMonitoringThread::RESULT_ENUM eServerConnetMonitoringThreadRet = m_pcServerConnectMonitoringThread->Start();
	if (eServerConnetMonitoringThreadRet != CServerConnectMonitoringThread::RESULT_SUCCESS)
	{
		// SerialServerスレッド停止
		CThread::Stop();

		return (CSerialServer::RESULT_ENUM)eServerConnetMonitoringThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// SerialServerスレッド停止
//-----------------------------------------------------------------------------
CSerialServer::RESULT_ENUM CSerialServer::Stop()
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

	// SerialServerスレッド停止
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// SerialServerスレッド
//-----------------------------------------------------------------------------
void CSerialServer::ThreadProc()
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
#ifdef _CSERIAL_SERVER_DEBUG_
		perror("CSerialServer::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_SERVER_DEBUG_
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
#ifdef _CSERIAL_SERVER_DEBUG_
		perror("CSerialServer::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_SERVER_DEBUG_
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
#ifdef _CSERIAL_SERVER_DEBUG_
			perror("CLandryClient::ThreadProc - epoll_wait");
#endif	// #ifdef _CSERIAL_SERVER_DEBUG_
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
		}
	}
	// ▲--------------------------------------------------------------------------▲

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// SerialServerスレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CSerialServer::ThreadProcCleanup(void* pArg)
{
	CSerialServer* pcSerialServer = (CSerialServer*)pArg;


	// epollファイルディスクリプタ解放
	if (pcSerialServer->m_epfd != -1)
	{
		close(pcSerialServer->m_epfd);
		pcSerialServer->m_epfd = -1;
	}
}
