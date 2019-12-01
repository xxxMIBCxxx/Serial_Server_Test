//*****************************************************************************
// クライアント応答スレッド
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
#include "CClientResponseThread.h"


#define _CCLIENT_RESPONSE_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント
#define EPOLL_TIMEOUT_TIME							( 200 )						// epollタイムアウト(ms)


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CClientResponseThread::CClientResponseThread(CEvent* pcServerDisconnectEvent)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	memset(&m_tServerInfo, 0x00, sizeof(m_tServerInfo));
	memset(m_szIpAddr, 0x00, sizeof(m_szIpAddr));
	m_Port = 0;
	m_tServerInfo.Socket = -1;
	m_epfd = -1;
	m_pcClientTcpRecvThread = NULL;
	m_pcServerDisconnectEvent = NULL;

	if (pcServerDisconnectEvent == NULL)
	{
		return;
	}
	m_pcServerDisconnectEvent = pcServerDisconnectEvent;

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CClientResponseThread::~CClientResponseThread()
{
	// クライアント応答スレッド停止漏れ考慮
	this->Stop();
}


//-----------------------------------------------------------------------------
// クライアント応答スレッド開始
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::Start()
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

	// サーバー接続処理
	eRet = ServerConnect(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		return eRet;
	}

	// クライアント応答スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();

		// サーバー切断処理
		ServerDisconnect(m_tServerInfo);

		return (CClientResponseThread::RESULT_ENUM)eThreadRet;
	}

	// TCP受信・送信スレッド生成 & 開始
	eRet = CreateTcpThread(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		// 接続監視スレッド停止
		CThread::Stop();

		// サーバー切断処理
		ServerDisconnect(m_tServerInfo);

		return eRet;
	}


	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// クライアント応答スレッド停止
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::Stop()
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

	// TCP受信・送信スレッド解放 & 停止
	DeleteTcpThread();

	// クライアント応答スレッド停止
	CThread::Stop();

	// サーバー切断処理
	ServerDisconnect(m_tServerInfo);

	return RESULT_SUCCESS;
}



//-----------------------------------------------------------------------------
// クライアント応答スレッド
//-----------------------------------------------------------------------------
void CClientResponseThread::ThreadProc()
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
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread::ThreadProc - epoll_create");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
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
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		return;
	}

	// サーバー情報を取得
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tServerInfo.tAddr.sin_addr));			// IPアドレス取得
	m_Port = ntohs(m_tServerInfo.tAddr.sin_port);								// ポート番号取得

	printf("[%s (%d)] - Server Connect!\n", m_szIpAddr, m_Port);

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
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
			perror("CClientResponseThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
			continue;
		}
		// タイムアウト
		else if (nfds == 0)
		{
			write(m_tServerInfo.Socket, "Hello!", 7);
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
// クライアント応答スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CClientResponseThread::ThreadProcCleanup(void* pArg)
{
	CClientResponseThread* pcClientResponseThread = (CClientResponseThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcClientResponseThread->m_epfd != -1)
	{
		close(pcClientResponseThread->m_epfd);
		pcClientResponseThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// サーバー接続処理
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::ServerConnect(SERVER_INFO_TABLE& tServerInfo)
{
	int					iRet = 0;


	// ソケットを生成
	tServerInfo.Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tServerInfo.Socket == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread::ServerConnect - socket");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		return RESULT_ERROR_CREATE_SOCKET;
	}

	// サーバー接続先を設定
	tServerInfo.tAddr.sin_family = AF_INET;
	tServerInfo.tAddr.sin_port = htons(12345);							// ← ポート番号
	tServerInfo.tAddr.sin_addr.s_addr = inet_addr("192.168.10.10");		// ← IPアドレス
	if (tServerInfo.tAddr.sin_addr.s_addr == 0xFFFFFFFF)
	{
		// ホスト名からIPアドレスを取得
		struct hostent* host;
		host = gethostbyname("localhost");
		tServerInfo.tAddr.sin_addr.s_addr = *(unsigned int*)host->h_addr_list[0];
	}

	// サーバーに接続する
	iRet = connect(tServerInfo.Socket, (struct sockaddr*) & tServerInfo.tAddr, sizeof(tServerInfo.tAddr));
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread::ServerConnect - connect");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_

		// サーバー切断処理
		ServerDisconnect(tServerInfo);

		return RESULT_ERROR_CONNECT;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// サーバー切断処理
//-----------------------------------------------------------------------------
void CClientResponseThread::ServerDisconnect(SERVER_INFO_TABLE& tServerInfo)
{
	// ソケットが生成されている場合
	if (tServerInfo.Socket != -1)
	{
		close(tServerInfo.Socket);
	}

	// サーバー情報初期化
	memset(&tServerInfo, 0x00, sizeof(tServerInfo));
	tServerInfo.Socket = -1;
}


//-----------------------------------------------------------------------------
// TCP受信スレッド生成 & 開始
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::CreateTcpThread(SERVER_INFO_TABLE& tServerInfo)
{
	// TCP受信スレッド生成
	CClientTcpRecvThread::SERVER_INFO_TABLE		tServer;
	tServer.Socket = m_tServerInfo.Socket;
	memcpy(&tServer.tAddr, &m_tServerInfo.tAddr, sizeof(tServer.tAddr));
	m_pcClientTcpRecvThread = (CClientTcpRecvThread*)new CClientTcpRecvThread(tServer,m_pcServerDisconnectEvent);
	if (m_pcClientTcpRecvThread == NULL)
	{
		return RESULT_ERROR_SYSTEM;
	}

	// TCP受信スレッド開始
	CClientTcpRecvThread::RESULT_ENUM eRet = m_pcClientTcpRecvThread->Start();
	if (eRet != CClientTcpRecvThread::RESULT_SUCCESS)
	{
		// TCP受信・送信スレッド解放 & 停止
		DeleteTcpThread();

		return (CClientResponseThread::RESULT_ENUM)eRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP受信スレッド解放 & 停止
//-----------------------------------------------------------------------------
void CClientResponseThread::DeleteTcpThread()
{
	// TCP受信スレッド解放
	if (m_pcClientTcpRecvThread != NULL)
	{
		delete m_pcClientTcpRecvThread;
		m_pcClientTcpRecvThread = NULL;
	}
}
