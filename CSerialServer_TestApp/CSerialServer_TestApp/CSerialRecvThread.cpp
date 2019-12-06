//*****************************************************************************
// CSerialRecvThreadクラス
//*****************************************************************************
#include "CSerialRecvThread.h"
#include <unistd.h>
#include <sys/epoll.h>

#define _CSERIAL_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CSerialRecvThread::CSerialRecvThread(CSerialRecvThread::CLASS_PARAM_TABLE& tClassParam)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;

	// クラスパラメータを保持
	m_tClassParam = tClassParam;
	if (m_tClassParam.pcLog == NULL)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::CSerialRecvThread - m_tClassParam.pcLog NULL.");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	if (m_tClassParam.SerialFd < 0)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::CSerialRecvThread - m_tClassParam.SerialFd < 0.");
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::CSerialRecvThread - m_tClassParam.SerialFd < 0.");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// シリアル通信受信データリスト用イベントの初期化
	CEvent::RESULT_ENUM eEventRet = m_cSerialRecvDataListEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::CSerialRecvThread - m_cSerialRecvDataListEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::CSerialRecvThread - m_cSerialRecvDataListEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CSerialRecvThread::~CSerialRecvThread()
{

}


//-----------------------------------------------------------------------------
// シリアル通信受信スレッド開始
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::Start - Not InitProc.");
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Start - Not InitProc.");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが動作している場合
	bRet = this->IsActive();
	if (bRet == true)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::Start - Thread is Active.");
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Start - Thread is Active.");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// シリアル通信受信スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::Start - Start Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::Start - Start Error.");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return (CSerialRecvThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル通信受信スレッド停止
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::Stop()
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

	// シリアル通信受信スレッド停止
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル通信受信スレッド
//-----------------------------------------------------------------------------
void CSerialRecvThread::ThreadProc()
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
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::ThreadProc - epoll_create Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
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
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent] Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// シリアル通信受信用ファイルディスクリプタ登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_tClassParam.SerialFd;
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_tClassParam.SerialFd, &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::ThreadProc - epoll_ctl[SerialFd] Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_ctl[SerialFd]");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
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
			m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::ThreadProc - epoll_wait Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
			perror("CSerialRecvThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
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
			// シリアル通信受信用ファイルディスクリプタ
			else if (tEvents[i].data.fd == this->m_tClassParam.SerialFd)
			{
				// シリアル受信
				char				szBuff[500 + 1];
				ssize_t				ReadNum = 0;
				memset(szBuff, 0x00, sizeof(szBuff));
				ReadNum = read(this->m_tClassParam.SerialFd, szBuff, 500);
				if (ReadNum != 0)
				{
					printf("%s\n", szBuff);
				}
				continue;
			}
		}
		// ▲--------------------------------------------------------------------------▲
	}

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// シリアル通信受信スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CSerialRecvThread::ThreadProcCleanup(void* pArg)
{
	CSerialRecvThread* pcSerialRecvThread = (CSerialRecvThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcSerialRecvThread->m_epfd != -1)
	{
		close(pcSerialRecvThread->m_epfd);
		pcSerialRecvThread->m_epfd = -1;
	}
}