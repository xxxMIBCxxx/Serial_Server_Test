//*****************************************************************************
// TCP通信受信スレッドクラス
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "CClientTcpRecvThread.h"


#define _CCLIENT_TCP_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント
#define STX											( 0x02 )					// STX
#define ETX											( 0x03 )					// ETX

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CClientTcpRecvThread::CClientTcpRecvThread(SERVER_INFO_TABLE& tServerInfo, CEvent* pcServerDisconnectEvent)
{
	bool						bRet = false;
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;
	CEventEx::RESULT_ENUM		eEventExRet = CEventEx::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_tServerInfo = tServerInfo;
	memset(m_szIpAddr, 0x00, sizeof(m_szIpAddr));
	m_Port = 0;
	m_eAnalyzeKind = ANALYZE_KIND_STX;
	m_CommandPos = 0;
	memset(m_szRecvBuff, 0x00, sizeof(m_szRecvBuff));
	memset(m_szCommandBuff, 0x00, sizeof(m_szCommandBuff));
	m_pcServerDisconnectEvent = NULL;

	if (pcServerDisconnectEvent == NULL)
	{
		return;
	}
	m_pcServerDisconnectEvent = pcServerDisconnectEvent;


	// 接続情報を取得
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tServerInfo.tAddr.sin_addr));			// IPアドレス取得
	m_Port = ntohs(m_tServerInfo.tAddr.sin_port);								// ポート番号取得

	// TCP受信情報イベントの初期化
	eEventExRet = m_cRecvInfoEvent.Init();
	if (eEventExRet != CEventEx::RESULT_SUCCESS)
	{
		return;
	}

	// TCP受信情報リストのクリア
	m_RecvInfoList.clear();


	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CClientTcpRecvThread::~CClientTcpRecvThread()
{
	// TCP通信受信スレッド停止し忘れ考慮
	this->Stop();

	// 再度、TCP受信情報リストをクリアする
	RecvInfoList_Clear();
}


//-----------------------------------------------------------------------------
// エラー番号を取得
//-----------------------------------------------------------------------------
int CClientTcpRecvThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// TCP通信受信スレッド開始
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが動作している場合
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::Start - Thread Active.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// TCP通信受信スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CClientTcpRecvThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP通信受信スレッド停止
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::Stop()
{
	bool						bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// TCP通信受信スレッド停止
	CThread::Stop();

	// TCP受信情報リストをクリアする
	RecvInfoList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP通信受信スレッド
//-----------------------------------------------------------------------------
void CClientTcpRecvThread::ThreadProc()
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
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		perror("CClientTcpRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
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
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		perror("CClientTcpRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return;
	}

	// TCP受信用のソケットを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_tServerInfo.Socket;
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_tServerInfo.Socket, &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		perror("CClientTcpRecvThread::ThreadProc - epoll_ctl[RecvResponseEvent]");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
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
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
			perror("CClientTcpRecvThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
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
			// TCP受信用のソケット
			else if (this->m_tServerInfo.Socket)
			{
				// TCP受信処理
				TcpRecvProc();
			}
		}
	}

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// TCP通信受信スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CClientTcpRecvThread::ThreadProcCleanup(void* pArg)
{
	// パラメータチェック
	if (pArg == NULL)
	{
		return;
	}
	CClientTcpRecvThread* pcClientTcpRecvThread = (CClientTcpRecvThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcClientTcpRecvThread->m_epfd != -1)
	{
		close(pcClientTcpRecvThread->m_epfd);
		pcClientTcpRecvThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// TCP受信情報リストをクリアする
//-----------------------------------------------------------------------------
void CClientTcpRecvThread::RecvInfoList_Clear()
{
	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cRecvInfoListMutex.Lock();

	std::list<RECV_INFO_TABLE>::iterator		it = m_RecvInfoList.begin();
	while (it != m_RecvInfoList.end())
	{
		RECV_INFO_TABLE			tRecvInfo = *it;

		// バッファが確保されている場合
		if (tRecvInfo.pData != NULL)
		{
			// バッファ領域を解放
			free(tRecvInfo.pData);
		}
		it++;
	}

	// TCP受信応答リストをクリア
	m_RecvInfoList.clear();

	m_cRecvInfoListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲
}


//-----------------------------------------------------------------------------
// TCP受信データ取得
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::GetRecvData(RECV_INFO_TABLE& tRecvInfo)
{
	bool					bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::GetRecvData - Not Init Proc.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cRecvInfoListMutex.Lock();

	// TCP受信情報リストに登録データある場合
	if (m_RecvInfoList.empty() != true)
	{
		// TCP受信情報リストの先頭データを取り出す（※リストの先頭データは削除）
		std::list<RECV_INFO_TABLE>::iterator		it = m_RecvInfoList.begin();
		tRecvInfo = *it;
		m_RecvInfoList.pop_front();
	}

	m_cRecvInfoListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	// TCP受信情報データを取得したので、TCP受信情報イベントをクリアする
	m_cRecvInfoEvent.ClearEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP受信データ設定
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::SetRecvData(RECV_INFO_TABLE& tRecvInfo)
{
	bool					bRet = false;


	// 引数チェック
	if (tRecvInfo.pData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::SetRecvData - Not Init Proc.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cRecvInfoListMutex.Lock();

	// TCP受信情報リストに登録
	m_RecvInfoList.push_back(tRecvInfo);

	m_cRecvInfoListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	// TCP受信情報イベントを送信する
	m_cRecvInfoEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP受信処理
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::TcpRecvProc()
{
	ssize_t					read_count = 0;
	// TCP受信処理
	memset(m_szRecvBuff, 0x00, sizeof(m_szRecvBuff));
	read_count = read(m_tServerInfo.Socket, m_szRecvBuff, CCLIENT_TCP_RECV_THREAD_RECV_BUFF_SIZE);
	if (read_count < 0)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		perror("CClientTcpRecvThread::TcpRecvProc - read");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_RECV;
	}
	else if (read_count == 0)
	{
		// 切断するとTCP受信ソケットの通知が何度も来るので、TCP受信ソケットイベント登録を解除する
		epoll_ctl(m_epfd, EPOLL_CTL_DEL, this->m_tServerInfo.Socket, NULL);

		// 切断されたので、切断イベントを送信する
		printf("[%s (%d)] - Client Disconnect!\n", m_szIpAddr, m_Port);
		m_pcServerDisconnectEvent->SetEvent();
	}
	else
	{
		// TCP受信データ解析
		TcpRecvDataAnalyze(m_szRecvBuff, read_count);
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP受信データ解析
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::TcpRecvDataAnalyze(char* pRecvData, ssize_t RecvDataNum)
{
	RESULT_ENUM					eRet = RESULT_SUCCESS;

	// パラメータチェック
	if (pRecvData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// 受信したデータを1Byteずつ調べる
	for (ssize_t i = 0; i < RecvDataNum; i++)
	{
		char		ch = pRecvData[i];


		// 解析種別によって処理を変更
		if (m_eAnalyzeKind == ANALYZE_KIND_STX)
		{
			// STX
			if (ch == STX)
			{
				m_CommandPos = 0;
				memset(m_szCommandBuff, 0x00, sizeof(m_szCommandBuff));
				m_szCommandBuff[m_CommandPos++] = ch;
				m_eAnalyzeKind = ANALYZE_KIND_ETX;
			}
			else
			{
				// 受信データを破棄
			}
		}
		else
		{
			// STX
			if (ch == STX)
			{
				m_CommandPos = 0;
				memset(m_szCommandBuff, 0x00, sizeof(m_szCommandBuff));
				m_szCommandBuff[m_CommandPos++] = ch;
				m_eAnalyzeKind = ANALYZE_KIND_ETX;
			}
			else if (ch == ETX)
			{
				// 解析完了
				m_szCommandBuff[m_CommandPos++] = ch;
				m_eAnalyzeKind = ANALYZE_KIND_STX;

				// TCP受信応答
				RECV_INFO_TABLE		tRecvInfo;
				memset(&tRecvInfo, 0x00, sizeof(tRecvInfo));
				tRecvInfo.pReceverClass = this;
				tRecvInfo.DataSize = strlen(m_szCommandBuff) + 1;			// Debug用
//				tRecvInfo.DataSize = strlen(m_szCommandBuff);
				tRecvInfo.pData = (char*)malloc(tRecvInfo.DataSize);
				if (tRecvInfo.pData == NULL)
				{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
					printf("CClientTcpRecvThread::TcpRecvDataAnalyze - malloc error.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
					return RESULT_ERROR_SYSTEM;
				}
				memcpy(tRecvInfo.pData, m_szCommandBuff, tRecvInfo.DataSize);
				eRet = this->SetRecvData(tRecvInfo);
				if (eRet != RESULT_SUCCESS)
				{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
					printf("CClientTcpRecvThread::TcpRecvDataAnalyze - SetRecvData error.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
					return eRet;
				}
			}
			else
			{
				// 受信データを格納
				m_szCommandBuff[m_CommandPos++] = ch;
				if (m_CommandPos >= CCLIENT_TCP_RECV_THREAD_COMMAND_BUFF_SIZE)
				{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
					printf("CClientTcpRecvThread::TcpRecvDataAnalyze - Command Buffer Over.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
					return RESULT_ERROR_COMMAND_BUFF_OVER;
				}
			}
		}
	}

	return RESULT_SUCCESS;
}

