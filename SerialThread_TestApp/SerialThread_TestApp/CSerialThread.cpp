//*****************************************************************************
// CSerialThreadクラス
//*****************************************************************************
#include "CSerialThread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <termios.h>
#include <fcntl.h>


#define _CSERIAL_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント


// ボーレート変換テーブル
const CSerialThread::BAUDRATE_CONV_TABLE g_tBaudrateConv[] =
{
	{	 4800,		B4800,		"4800" },
	{	 9600,		B9600,		"9600" },
	{	19200,	   B19200,	   "19200" },
	{   38400,	   B38400,	   "38400" },
	{   57600,	   B57600,     "57600" },
	{  115200,    B115200,    "115200" },
	{  230400,	  B230400,    "230400" },
	{  460800,    B460800,    "460800" },
	{  500000,    B500000,    "500000" },
	{  921600,    B921600,    "921600" },
	{ 1000000,   B1000000,   "1000000" },
	{ 2000000,   B2000000,   "2000000" },
	{ 2500000,   B2500000,   "2500000" },
	{ 3000000,   B3000000,   "3000000" },
	{ 3500000,   B3500000,   "3500000" },
	{ 4000000,   B4000000,   "4000000" },
	{      -1,		B9600,      "9600" },	// < default >
};


// データ長変換テーブル
const CSerialThread::DATA_SIZE_CONV_TABLE g_tDataSizeConv[] =
{
	{	5,	CS5, "5Bit" },		// 5Bit
	{	6,	CS6, "6Bit" },		// 6Bit
	{	7,	CS7, "7Bit" },		// 7Bit
	{	8,	CS8, "8Bit" },		// 8Bit
	{  -1,  CS8, "8Bit" },		// < default >
};


// ストップビット変換テーブル
CSerialThread::STOP_BIT_CONV_TABLE g_tStopBitConv[] =
{
	{	1,		0, "1" },	// StopBit 1
	{	2, CSTOPB, "2" },	// StopBit 2
	{  -1,      0, "1" },	// < default >
};


// パリティ変換テーブル
CSerialThread::PARITY_CONV_TABLE g_ParityConv[] =
{
	{	0,		0, "EVEN" },	// 偶数パリティ
	{	1, PARODD, "ODD"  },	// 奇数パリティ
	{  -1,		0, "EVEN" },	// < default >
};


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CSerialThread::CSerialThread(CSerialThread::CLASS_PARAM_TABLE& tClassParam)
{
	CEvent::RESULT_ENUM				eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_pcSerialRecvThread = NULL;
	m_SerialFd = -1;
	memset(&m_tSerialConfInfo, 0x00, sizeof(m_tSerialConfInfo));


	// クラスパラメータを保持
	m_tClassParam = tClassParam;
	if (m_tClassParam.pcLog == NULL)
	{
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::CSerialThread - m_tClassParam.pcLog NULL.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return;
	}

	if (m_tClassParam.pcConfFile == NULL)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::CSerialThread - m_tClassParam.pcConfFile NULL.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::CSerialThread - m_tClassParam.pcConfFile NULL.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return;
	}

	// シリアル通信設定情報取得
	GetSerialConfInfo();

	// IRQシリアル通信データ要求リストクリア
	ClearIrqSerialDataList();

	// シリアル通信データ要求リストクリア
	ClearSerialDataList();

	// シリアル通知要求イベント（IRQ・通常共通）の初期化
	eEventRet = m_cSerialDataEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::CSerialThread - m_cSerialDataEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::CSerialThread - m_cSerialDataEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return;
	}

	// シリアル受信完了イベントの初期化
	eEventRet = m_cSerialRecvEndEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::CSerialThread - m_cSerialRecvEndEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::CSerialThread - m_cSerialRecvEndEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CSerialThread::~CSerialThread()
{
	// シリアル送信スレッドが停止していないことを考慮
	this->Stop();
}


//-----------------------------------------------------------------------------
// シリアル通信スレッド開始
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Not InitProc.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Not InitProc.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが動作している場合
	bRet = this->IsActive();
	if (bRet == true)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Thread is Active.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Thread is Active.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// シリアルをオープン
	eRet = Open(m_tSerialConfInfo);
	if (eRet != RESULT_SUCCESS)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Open Error. [eRet:0x%08X]", eRet);
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Open Error. [eRet:0x%08X]", eRet);
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return eRet;
	}

	// シリアル通信受信スレッドクラスの生成
	CSerialRecvThread::CLASS_PARAM_TABLE		tSerialRecvClassParam;
	tSerialRecvClassParam.pcLog = m_tClassParam.pcLog;
	tSerialRecvClassParam.SerialFd = this->m_SerialFd;
	m_pcSerialRecvThread = (CSerialRecvThread*)new CSerialRecvThread(tSerialRecvClassParam);
	if (m_pcSerialRecvThread == NULL)
	{
		Close();
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Create CSerialRecvThread Error.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Create CSerialRecvThread Error.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}

	// シリアル通信受信スレッド開始
	CSerialRecvThread::RESULT_ENUM eSerialTecvThread = m_pcSerialRecvThread->Start();
	if (eSerialTecvThread != CSerialRecvThread::RESULT_SUCCESS)
	{
		if (m_pcSerialRecvThread != NULL)
		{
			delete m_pcSerialRecvThread;
			m_pcSerialRecvThread = NULL;
		}
		Close();
	}

	// クライアント接続監視スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		if (m_pcSerialRecvThread != NULL)
		{
			m_pcSerialRecvThread->Stop();
			delete m_pcSerialRecvThread;
			m_pcSerialRecvThread = NULL;
		}
		Close();
		m_ErrorNo = CThread::GetErrorNo();
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Start Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::Start - Start Error.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return (CSerialThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル通信スレッド停止
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::Stop()
{
	bool						bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Stop - Not InitProc.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Not InitProc.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Thread is Active.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Thread is Active.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_SUCCESS;
	}

	// シリアル通信スレッド停止
	CThread::Stop();

	// シリアル受信通信スレッド停止
	if (m_pcSerialRecvThread != NULL)
	{
		m_pcSerialRecvThread->Stop();
		delete m_pcSerialRecvThread;
		m_pcSerialRecvThread = NULL;
	}

	// シリアルをクローズ
	Close();

	// IRQシリアル通信データ要求リストクリア
	ClearIrqSerialDataList();

	// シリアル通信データ要求リストクリア
	ClearSerialDataList();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル通信設定情報取得
//-----------------------------------------------------------------------------
void CSerialThread::GetSerialConfInfo(void)
{
	std::string							strValue;
	int									i = 0;
	int									TableMaxNum = 0;


	// デバイス情報取得
	strValue = "";
	m_tClassParam.pcConfFile->GetValue("Serial", "DeviceName", strValue);
	if (strValue.length() != 0)
	{
		strncpy(m_tSerialConfInfo.szDevName, strValue.c_str(), SERIAL_DEVICE_NAME);
	}
	else
	{
		strncpy(m_tSerialConfInfo.szDevName, "/dev/tty???", SERIAL_DEVICE_NAME);
	}

	// ボーレート
	strValue = "";

	m_tClassParam.pcConfFile->GetValue("Serial", "Baudrate", strValue);
	if (strValue.length() != 0)
	{
		m_tSerialConfInfo.tBaudRate.Baudrate = atol(strValue.c_str());
	}
	else
	{
		m_tSerialConfInfo.tBaudRate.Baudrate = -1;
	}
	i = 0;
	TableMaxNum = sizeof(g_tBaudrateConv) / sizeof(CSerialThread::BAUDRATE_CONV_TABLE);
	while (TableMaxNum > i)
	{
		if ((g_tBaudrateConv[i].Baudrate == -1) || (g_tBaudrateConv[i].Baudrate == m_tSerialConfInfo.tBaudRate.Baudrate))
		{
			m_tSerialConfInfo.tBaudRate = g_tBaudrateConv[i];
			break;
		}
		i++;
	}

	// データ長
	strValue = "";
	m_tClassParam.pcConfFile->GetValue("Serial", "DataSize", strValue);
	if (strValue.length() != 0)
	{
		m_tSerialConfInfo.tDataSize.DataSize = atol(strValue.c_str());
	}
	else
	{
		m_tSerialConfInfo.tDataSize.DataSize = -1;
	}
	i = 0;
	TableMaxNum = sizeof(g_tDataSizeConv) / sizeof(CSerialThread::DATA_SIZE_CONV_TABLE);
	while (TableMaxNum > i)
	{
		if ((g_tDataSizeConv[i].DataSize == -1) || (g_tDataSizeConv[i].DataSize == m_tSerialConfInfo.tDataSize.DataSize))
		{
			m_tSerialConfInfo.tDataSize = g_tDataSizeConv[i];
			break;
		}
		i++;
	}

	// ストップビット
	strValue = "";
	m_tClassParam.pcConfFile->GetValue("Serial", "StopBit", strValue);
	if (strValue.length() != 0)
	{
		m_tSerialConfInfo.tStopBit.StopBit = atol(strValue.c_str());
	}
	else
	{
		m_tSerialConfInfo.tStopBit.StopBit = -1;
	}
	i = 0;
	TableMaxNum = sizeof(g_tStopBitConv) / sizeof(CSerialThread::STOP_BIT_CONV_TABLE);
	while (TableMaxNum > i)
	{
		if ((g_tStopBitConv[i].StopBit == -1) || (g_tStopBitConv[i].StopBit == m_tSerialConfInfo.tStopBit.StopBit))
		{
			m_tSerialConfInfo.tStopBit = g_tStopBitConv[i];
			break;
		}
		i++;
	}

	// パリティビット
	strValue = "";
	m_tClassParam.pcConfFile->GetValue("Serial", "Parity", strValue);
	if (strValue.length() != 0)
	{
		m_tSerialConfInfo.tParity.Parity = atol(strValue.c_str());
	}
	else
	{
		m_tSerialConfInfo.tParity.Parity = -1;
	}
	i = 0;
	TableMaxNum = sizeof(g_ParityConv) / sizeof(CSerialThread::PARITY_CONV_TABLE);
	while (TableMaxNum > i)
	{
		if ((g_ParityConv[i].Parity == -1) || (g_ParityConv[i].Parity == m_tSerialConfInfo.tParity.Parity))
		{
			m_tSerialConfInfo.tParity = g_ParityConv[i];
			break;
		}
		i++;
	}

	m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "-----[ SerialConfInfo ]-----");
	m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "DeviceName : %s", m_tSerialConfInfo.szDevName);
	m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "Baudrate   : %s", m_tSerialConfInfo.tBaudRate.pszBaudrate);
	m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "DataSize   : %s", m_tSerialConfInfo.tDataSize.pszDataSize);
	m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "StopBit    : %s", m_tSerialConfInfo.tStopBit.pszStopBit);
	m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "Parity     : %s", m_tSerialConfInfo.tParity.pszParity);
}


//-----------------------------------------------------------------------------
// シリアルオープン
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::Open(SERIAL_CONF_TABLE& tSerialConfInfo)
{
	int						iRet = 0;
	struct termios			tNewtio;



	// 既にオープンしている場合
	if (m_SerialFd != -1)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Open - Already Serial Open.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Open - Already Serial Open.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_SERIAL_OPEN;
	}

	// シリアルオープン処理
	m_SerialFd = open(tSerialConfInfo.szDevName, (O_RDWR | O_NOCTTY));
	if (m_SerialFd < 0)
	{
		m_ErrorNo = errno;
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Open - open Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::Open - open");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_START;
	}

	// シリアルボーレート設定
	bzero(&tNewtio, sizeof(tNewtio));
	iRet = cfsetispeed(&tNewtio, m_tSerialConfInfo.tBaudRate.BaureateDefine);
	if (iRet < 0)
	{
		m_ErrorNo = errno;
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Open - cfsetispeed Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::Open - cfsetispeed");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		Close();
		return RESULT_ERROR_SERIAL_SET_BAUDRATE;
	}

	iRet = cfsetospeed(&tNewtio, m_tSerialConfInfo.tBaudRate.BaureateDefine);
	if (iRet < 0)
	{
		m_ErrorNo = errno;
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Open - cfsetospeed Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::Open - cfsetospeed");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		Close();
		return RESULT_ERROR_SERIAL_SET_BAUDRATE;
	}

	// シリアル属性設定
	// ---< 入力モード >---
	tNewtio.c_iflag = IGNPAR;											// IGNPAR：Parity Error文字を無視
	tNewtio.c_iflag |= ICRNL;											// ICRNL：受信したcarriage returnを改行に変換
	
	// ---< 出力モード >---
//	tNewtio.c_oflag = OCRNL;											// OCRNL：出力されるcarrige returnを改行に変換
	tNewtio.c_oflag = 0;

	// ---< 制御モード >---
	tNewtio.c_cflag = CREAD | CLOCAL;									// CREAD：文字の受信を可能にする
	tNewtio.c_cflag |= m_tSerialConfInfo.tBaudRate.BaureateDefine;		//      ：ボーレート
	tNewtio.c_cflag |= m_tSerialConfInfo.tDataSize.DataSizeDefine;		//      ：送受信文字サイズ
	tNewtio.c_cflag |= m_tSerialConfInfo.tStopBit.StopBitDefine;		//      ：ストップビット
	tNewtio.c_cflag |= m_tSerialConfInfo.tParity.ParityDefine;			//      ：パリティ
//	tNewtio.c_lflag |= ICANON;

	iRet = tcsetattr(m_SerialFd, TCSANOW, &tNewtio);
	if (iRet < 0)
	{
		m_ErrorNo = errno;
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Open - tcsetattr Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::Open - tcsetattr");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		Close();
		return RESULT_ERROR_START;
	}

	return RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
// シリアルクローズ
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::Close(void)
{
	// 既にクローズしている場合
	if (m_SerialFd == -1)
	{
		return RESULT_SUCCESS;
	}

	close(m_SerialFd);
	m_SerialFd = -1;

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル通信スレッド
//-----------------------------------------------------------------------------
void CSerialThread::ThreadProc()
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
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::ThreadProc - epoll_create Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
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
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::ThreadProc - epoll_ctl[ThreadEndReqEvent] Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return;
	}

	// シリアル通知要求イベント（IRQ・通常共通）を登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_cSerialDataEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_cSerialDataEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::ThreadProc - epoll_ctl[SerialDataEvent] Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::ThreadProc - epoll_ctl[SerialDataEvent]");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return;
	}

	// シリアル受信完了イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_cSerialRecvEndEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_cSerialRecvEndEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::ThreadProc - epoll_ctl[SerialRecvEndEvent] Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
		perror("CSerialThread::ThreadProc - epoll_ctl[SerialRecvEndEvent]");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
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
			m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::ThreadProc - epoll_wait Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
			perror("CSerialThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
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
			// シリアル通知要求イベント（IRQ・通常共通）
			else if (tEvents[i].data.fd == this->m_cSerialDataEvent.GetEventFd())
			{
				this->m_cSerialDataEvent.ClearEvent();

				// シリアル送信処理
				bLoop = SerialSendProc(&this->m_epfd, tEvents, EPOLL_MAX_EVENTS, 500);
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
// シリアル通信スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CSerialThread::ThreadProcCleanup(void* pArg)
{
	CSerialThread* pcSerialThread = (CSerialThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcSerialThread->m_epfd != -1)
	{
		close(pcSerialThread->m_epfd);
		pcSerialThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// シリアル送信処理
//-----------------------------------------------------------------------------
bool CSerialThread::SerialSendProc(int* pEpfd, struct epoll_event* ptEvents, int MaxEvents, int Timeout)
{
	bool					bLoop = true;
	size_t					SendDataNum = 0;
	bool					bSendData = false;
	std::list<SERIAL_DATA_TABLE>::iterator		it;
	SERIAL_DATA_TABLE		tTempSeriarData;
	ssize_t					WriteNum = 0;

	// ▼--------------------------------------------------------------------------▼
	// 送信データが全て無くなる or 送信後に終了要求が通知されるまでループ
	while (bLoop)
	{
		bSendData = false;
		memset(&tTempSeriarData, 0x00, sizeof(tTempSeriarData));

		// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
		m_cIrqSerialDataListMutex.Lock();

		// IRQシリアル送信データリストに送信データがあるか調べる
		SendDataNum = m_IrqSerialDataList.size();
		if (SendDataNum != 0)
		{
			// リストの先頭データを取り出す
			it = m_IrqSerialDataList.begin();
tTempSeriarData = *it;

// リストの先頭を削除
m_IrqSerialDataList.pop_front();

bSendData = true;
		}

		m_cIrqSerialDataListMutex.Unlock();
		// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

		// シリアル送信データがまだ見つかっていない場合
		if (bSendData == false)
		{
			// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
			m_cSerialDataListMutex.Lock();

			// シリアル送信データリストに送信データがあるか調べる
			SendDataNum = m_SerialDataList.size();
			if (SendDataNum != 0)
			{
				// リストの先頭データを取り出す
				it = m_SerialDataList.begin();
				tTempSeriarData = *it;

				// リストの先頭を削除
				m_SerialDataList.pop_front();

				bSendData = true;
			}

			m_cSerialDataListMutex.Unlock();
			// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲
		}

		// シリアル送信データが見つからなかった場合
		if (bSendData == false)
		{
			// メインスレッド側へ戻る
			return true;
		}

		// シリアルデータ送信
		if ((tTempSeriarData.Size > 0) && (tTempSeriarData.pData != NULL))
		{
			WriteNum = write(this->m_SerialFd, tTempSeriarData.pData, tTempSeriarData.Size);
			if (WriteNum != tTempSeriarData.Size)
			{
				m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SerialSendProc - write Error. [WriteNum:%ld, tTempSeriarData.Size:%ld]", WriteNum, tTempSeriarData.Size);
#ifdef _CSERIAL_THREAD_DEBUG_
				printf("CSerialThread::SerialSendProc - write Error. [WriteNum:%ld, tTempSeriarData.Size:%ld]", WriteNum, tTempSeriarData.Size);
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
			}

			// ※送信要求側にてmallocで取得した領域を解放※
			free(tTempSeriarData.pData);
		}

		// 
		memset(ptEvents, 0x00, sizeof(struct epoll_event) * MaxEvents);
		int nfds = epoll_wait(*pEpfd, ptEvents, MaxEvents, Timeout);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
			m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SerialSendProc - epoll_wait Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_THREAD_DEBUG_
			perror("CSerialThread::SerialSendProc - epoll_wait");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
			continue;
		}
		else if (nfds == 0)
		{
			continue;
		}

		for (int i = 0; i < nfds; i++)
		{
			// スレッド終了要求イベント受信
			if (ptEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// シリアル受信完了イベントを受信
			else if (ptEvents[i].data.fd == this->m_cSerialRecvEndEvent.GetEventFd())
			{
				this->m_cSerialRecvEndEvent.ClearEvent();
				continue;
			}
		}
	}
	// ▲--------------------------------------------------------------------------▲

	return bLoop;
}


//-----------------------------------------------------------------------------
// IRQシリアル通信データ登録
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::SetIrqSerialDataList(SERIAL_DATA_TABLE& tSerialData)
{
	bool				bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SetIrqSerialDataList - Not InitProc.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::SetIrqSerialDataList - Not InitProc.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SetIrqSerialDataList - Thread is Not Active.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::SetIrqSerialDataList - Thread is Not Active.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_THREAD_NOT_ACTIVE;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cIrqSerialDataListMutex.Lock();

	// IRQシリアル通信データリストに登録
	m_IrqSerialDataList.push_back(tSerialData);

	m_cIrqSerialDataListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	m_cSerialDataEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// IRQシリアル通信データ要求リストクリア
//-----------------------------------------------------------------------------
void CSerialThread::ClearIrqSerialDataList(void)
{
	size_t					DataNum = 0;


	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cIrqSerialDataListMutex.Lock();

	// IRQシリアル送信データリストに送信データがあるか調べる
	DataNum = m_IrqSerialDataList.size();
	if (DataNum != 0)
	{
		// データが全て無くなるまでループ
		std::list<SERIAL_DATA_TABLE>::iterator		it = m_IrqSerialDataList.begin();
		while (it != m_IrqSerialDataList.end())
		{
			// ※送信要求側にてmallocで取得した領域を解放※
			if (it->pData != NULL)
			{
				free(it->pData);
			}

			it++;
		}
	}
	m_IrqSerialDataList.clear();

	m_cIrqSerialDataListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	return;
}


//-----------------------------------------------------------------------------
// シリアル通信データ登録
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::SetSerialDataList(SERIAL_DATA_TABLE& tSerialData)
{
	bool				bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SetSerialDataList - Not InitProc.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::SetSerialDataList - Not InitProc.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SetSerialDataList - Thread is Not Active.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::SetSerialDataList - Thread is Not Active.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_THREAD_NOT_ACTIVE;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cSerialDataListMutex.Lock();

	// シリアル通信データリストに登録
	m_SerialDataList.push_back(tSerialData);

	m_cSerialDataListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	m_cSerialDataEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル通信データ要求リストクリア
//-----------------------------------------------------------------------------
void CSerialThread::ClearSerialDataList(void)
{
	size_t					DataNum = 0;


	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cSerialDataListMutex.Lock();

	// シリアル送信データリストに送信データがあるか調べる
	DataNum = m_SerialDataList.size();
	if (DataNum != 0)
	{
		// データが全て無くなるまでループ
		std::list<SERIAL_DATA_TABLE>::iterator		it = m_SerialDataList.begin();
		while (it != m_SerialDataList.end())
		{
			// ※送信要求側にてmallocで取得した領域を解放※
			if (it->pData != NULL)
			{
				free(it->pData);
			}

			it++;
		}
	}
	m_SerialDataList.clear();

	m_cSerialDataListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	return;
}