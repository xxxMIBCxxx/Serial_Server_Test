//*****************************************************************************
// CSerialThread�N���X
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
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g


// �{�[���[�g�ϊ��e�[�u��
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


// �f�[�^���ϊ��e�[�u��
const CSerialThread::DATA_SIZE_CONV_TABLE g_tDataSizeConv[] =
{
	{	5,	CS5, "5Bit" },		// 5Bit
	{	6,	CS6, "6Bit" },		// 6Bit
	{	7,	CS7, "7Bit" },		// 7Bit
	{	8,	CS8, "8Bit" },		// 8Bit
	{  -1,  CS8, "8Bit" },		// < default >
};


// �X�g�b�v�r�b�g�ϊ��e�[�u��
CSerialThread::STOP_BIT_CONV_TABLE g_tStopBitConv[] =
{
	{	1,		0, "1" },	// StopBit 1
	{	2, CSTOPB, "2" },	// StopBit 2
	{  -1,      0, "1" },	// < default >
};


// �p���e�B�ϊ��e�[�u��
CSerialThread::PARITY_CONV_TABLE g_ParityConv[] =
{
	{	0,		0, "EVEN" },	// �����p���e�B
	{	1, PARODD, "ODD"  },	// ��p���e�B
	{  -1,		0, "EVEN" },	// < default >
};


//-----------------------------------------------------------------------------
// �R���X�g���N�^
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


	// �N���X�p�����[�^��ێ�
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

	// �V���A���ʐM�ݒ���擾
	GetSerialConfInfo();

	// IRQ�V���A���ʐM�f�[�^�v�����X�g�N���A
	ClearIrqSerialDataList();

	// �V���A���ʐM�f�[�^�v�����X�g�N���A
	ClearSerialDataList();

	// �V���A���ʒm�v���C�x���g�iIRQ�E�ʏ틤�ʁj�̏�����
	eEventRet = m_cSerialDataEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::CSerialThread - m_cSerialDataEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::CSerialThread - m_cSerialDataEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return;
	}

	// �V���A����M�����C�x���g�̏�����
	eEventRet = m_cSerialRecvEndEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::CSerialThread - m_cSerialRecvEndEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::CSerialThread - m_cSerialRecvEndEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CSerialThread::~CSerialThread()
{
	// �V���A�����M�X���b�h����~���Ă��Ȃ����Ƃ��l��
	this->Stop();
}


//-----------------------------------------------------------------------------
// �V���A���ʐM�X���b�h�J�n
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Not InitProc.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Not InitProc.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Thread is Active.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Thread is Active.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// �V���A�����I�[�v��
	eRet = Open(m_tSerialConfInfo);
	if (eRet != RESULT_SUCCESS)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Open Error. [eRet:0x%08X]", eRet);
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Open Error. [eRet:0x%08X]", eRet);
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return eRet;
	}

	// �V���A���ʐM��M�X���b�h�N���X�̐���
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

	// �V���A���ʐM��M�X���b�h�J�n
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

	// �N���C�A���g�ڑ��Ď��X���b�h�J�n
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
// �V���A���ʐM�X���b�h��~
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::Stop()
{
	bool						bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Stop - Not InitProc.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Not InitProc.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Start - Thread is Active.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Start - Thread is Active.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_SUCCESS;
	}

	// �V���A���ʐM�X���b�h��~
	CThread::Stop();

	// �V���A����M�ʐM�X���b�h��~
	if (m_pcSerialRecvThread != NULL)
	{
		m_pcSerialRecvThread->Stop();
		delete m_pcSerialRecvThread;
		m_pcSerialRecvThread = NULL;
	}

	// �V���A�����N���[�Y
	Close();

	// IRQ�V���A���ʐM�f�[�^�v�����X�g�N���A
	ClearIrqSerialDataList();

	// �V���A���ʐM�f�[�^�v�����X�g�N���A
	ClearSerialDataList();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �V���A���ʐM�ݒ���擾
//-----------------------------------------------------------------------------
void CSerialThread::GetSerialConfInfo(void)
{
	std::string							strValue;
	int									i = 0;
	int									TableMaxNum = 0;


	// �f�o�C�X���擾
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

	// �{�[���[�g
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

	// �f�[�^��
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

	// �X�g�b�v�r�b�g
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

	// �p���e�B�r�b�g
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
// �V���A���I�[�v��
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::Open(SERIAL_CONF_TABLE& tSerialConfInfo)
{
	int						iRet = 0;
	struct termios			tNewtio;



	// ���ɃI�[�v�����Ă���ꍇ
	if (m_SerialFd != -1)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::Open - Already Serial Open.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::Open - Already Serial Open.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_SERIAL_OPEN;
	}

	// �V���A���I�[�v������
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

	// �V���A���{�[���[�g�ݒ�
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

	// �V���A�������ݒ�
	// ---< ���̓��[�h >---
	tNewtio.c_iflag = IGNPAR;											// IGNPAR�FParity Error�����𖳎�
	tNewtio.c_iflag |= ICRNL;											// ICRNL�F��M����carriage return�����s�ɕϊ�
	
	// ---< �o�̓��[�h >---
//	tNewtio.c_oflag = OCRNL;											// OCRNL�F�o�͂����carrige return�����s�ɕϊ�
	tNewtio.c_oflag = 0;

	// ---< ���䃂�[�h >---
	tNewtio.c_cflag = CREAD | CLOCAL;									// CREAD�F�����̎�M���\�ɂ���
	tNewtio.c_cflag |= m_tSerialConfInfo.tBaudRate.BaureateDefine;		//      �F�{�[���[�g
	tNewtio.c_cflag |= m_tSerialConfInfo.tDataSize.DataSizeDefine;		//      �F����M�����T�C�Y
	tNewtio.c_cflag |= m_tSerialConfInfo.tStopBit.StopBitDefine;		//      �F�X�g�b�v�r�b�g
	tNewtio.c_cflag |= m_tSerialConfInfo.tParity.ParityDefine;			//      �F�p���e�B
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
// �V���A���N���[�Y
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::Close(void)
{
	// ���ɃN���[�Y���Ă���ꍇ
	if (m_SerialFd == -1)
	{
		return RESULT_SUCCESS;
	}

	close(m_SerialFd);
	m_SerialFd = -1;

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �V���A���ʐM�X���b�h
//-----------------------------------------------------------------------------
void CSerialThread::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;


	// �X���b�h���I������ۂɌĂ΂��֐���o�^
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epoll�t�@�C���f�B�X�N���v�^����
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

	// �X���b�h�I���v���C�x���g��o�^
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

	// �V���A���ʒm�v���C�x���g�iIRQ�E�ʏ틤�ʁj��o�^
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

	// �V���A����M�����C�x���g��o�^
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

	// �X���b�h�J�n�C�x���g�𑗐M
	this->m_cThreadStartEvent.SetEvent();

	// ��--------------------------------------------------------------------------��
	// �X���b�h�I���v��������܂Ń��[�v
	// ������Ƀ��[�v�𔲂���ƃX���b�h�I�����Ƀ^�C���A�E�g�ŏI���ƂȂ邽�߁A�X���b�h�I���v���ȊO�͏���Ƀ��[�v�𔲂��Ȃ��ł�������
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
			// �X���b�h�I���v���C�x���g��M
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// �V���A���ʒm�v���C�x���g�iIRQ�E�ʏ틤�ʁj
			else if (tEvents[i].data.fd == this->m_cSerialDataEvent.GetEventFd())
			{
				this->m_cSerialDataEvent.ClearEvent();

				// �V���A�����M����
				bLoop = SerialSendProc(&this->m_epfd, tEvents, EPOLL_MAX_EVENTS, 500);
				continue;
			}
		}
		// ��--------------------------------------------------------------------------��
	}

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// �V���A���ʐM�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CSerialThread::ThreadProcCleanup(void* pArg)
{
	CSerialThread* pcSerialThread = (CSerialThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcSerialThread->m_epfd != -1)
	{
		close(pcSerialThread->m_epfd);
		pcSerialThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// �V���A�����M����
//-----------------------------------------------------------------------------
bool CSerialThread::SerialSendProc(int* pEpfd, struct epoll_event* ptEvents, int MaxEvents, int Timeout)
{
	bool					bLoop = true;
	size_t					SendDataNum = 0;
	bool					bSendData = false;
	std::list<SERIAL_DATA_TABLE>::iterator		it;
	SERIAL_DATA_TABLE		tTempSeriarData;
	ssize_t					WriteNum = 0;

	// ��--------------------------------------------------------------------------��
	// ���M�f�[�^���S�Ė����Ȃ� or ���M��ɏI���v�����ʒm�����܂Ń��[�v
	while (bLoop)
	{
		bSendData = false;
		memset(&tTempSeriarData, 0x00, sizeof(tTempSeriarData));

		// ����������������������������������������������������������
		m_cIrqSerialDataListMutex.Lock();

		// IRQ�V���A�����M�f�[�^���X�g�ɑ��M�f�[�^�����邩���ׂ�
		SendDataNum = m_IrqSerialDataList.size();
		if (SendDataNum != 0)
		{
			// ���X�g�̐擪�f�[�^�����o��
			it = m_IrqSerialDataList.begin();
tTempSeriarData = *it;

// ���X�g�̐擪���폜
m_IrqSerialDataList.pop_front();

bSendData = true;
		}

		m_cIrqSerialDataListMutex.Unlock();
		// ����������������������������������������������������������

		// �V���A�����M�f�[�^���܂��������Ă��Ȃ��ꍇ
		if (bSendData == false)
		{
			// ����������������������������������������������������������
			m_cSerialDataListMutex.Lock();

			// �V���A�����M�f�[�^���X�g�ɑ��M�f�[�^�����邩���ׂ�
			SendDataNum = m_SerialDataList.size();
			if (SendDataNum != 0)
			{
				// ���X�g�̐擪�f�[�^�����o��
				it = m_SerialDataList.begin();
				tTempSeriarData = *it;

				// ���X�g�̐擪���폜
				m_SerialDataList.pop_front();

				bSendData = true;
			}

			m_cSerialDataListMutex.Unlock();
			// ����������������������������������������������������������
		}

		// �V���A�����M�f�[�^��������Ȃ������ꍇ
		if (bSendData == false)
		{
			// ���C���X���b�h���֖߂�
			return true;
		}

		// �V���A���f�[�^���M
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

			// �����M�v�����ɂ�malloc�Ŏ擾�����̈�������
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
			// �X���b�h�I���v���C�x���g��M
			if (ptEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// �V���A����M�����C�x���g����M
			else if (ptEvents[i].data.fd == this->m_cSerialRecvEndEvent.GetEventFd())
			{
				this->m_cSerialRecvEndEvent.ClearEvent();
				continue;
			}
		}
	}
	// ��--------------------------------------------------------------------------��

	return bLoop;
}


//-----------------------------------------------------------------------------
// IRQ�V���A���ʐM�f�[�^�o�^
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::SetIrqSerialDataList(SERIAL_DATA_TABLE& tSerialData)
{
	bool				bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SetIrqSerialDataList - Not InitProc.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::SetIrqSerialDataList - Not InitProc.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SetIrqSerialDataList - Thread is Not Active.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::SetIrqSerialDataList - Thread is Not Active.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_THREAD_NOT_ACTIVE;
	}

	// ����������������������������������������������������������
	m_cIrqSerialDataListMutex.Lock();

	// IRQ�V���A���ʐM�f�[�^���X�g�ɓo�^
	m_IrqSerialDataList.push_back(tSerialData);

	m_cIrqSerialDataListMutex.Unlock();
	// ����������������������������������������������������������

	m_cSerialDataEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// IRQ�V���A���ʐM�f�[�^�v�����X�g�N���A
//-----------------------------------------------------------------------------
void CSerialThread::ClearIrqSerialDataList(void)
{
	size_t					DataNum = 0;


	// ����������������������������������������������������������
	m_cIrqSerialDataListMutex.Lock();

	// IRQ�V���A�����M�f�[�^���X�g�ɑ��M�f�[�^�����邩���ׂ�
	DataNum = m_IrqSerialDataList.size();
	if (DataNum != 0)
	{
		// �f�[�^���S�Ė����Ȃ�܂Ń��[�v
		std::list<SERIAL_DATA_TABLE>::iterator		it = m_IrqSerialDataList.begin();
		while (it != m_IrqSerialDataList.end())
		{
			// �����M�v�����ɂ�malloc�Ŏ擾�����̈�������
			if (it->pData != NULL)
			{
				free(it->pData);
			}

			it++;
		}
	}
	m_IrqSerialDataList.clear();

	m_cIrqSerialDataListMutex.Unlock();
	// ����������������������������������������������������������

	return;
}


//-----------------------------------------------------------------------------
// �V���A���ʐM�f�[�^�o�^
//-----------------------------------------------------------------------------
CSerialThread::RESULT_ENUM CSerialThread::SetSerialDataList(SERIAL_DATA_TABLE& tSerialData)
{
	bool				bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SetSerialDataList - Not InitProc.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::SetSerialDataList - Not InitProc.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialThread::SetSerialDataList - Thread is Not Active.");
#ifdef _CSERIAL_THREAD_DEBUG_
		printf("CSerialThread::SetSerialDataList - Thread is Not Active.");
#endif	// #ifdef _CSERIAL_THREAD_DEBUG_
		return RESULT_ERROR_THREAD_NOT_ACTIVE;
	}

	// ����������������������������������������������������������
	m_cSerialDataListMutex.Lock();

	// �V���A���ʐM�f�[�^���X�g�ɓo�^
	m_SerialDataList.push_back(tSerialData);

	m_cSerialDataListMutex.Unlock();
	// ����������������������������������������������������������

	m_cSerialDataEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �V���A���ʐM�f�[�^�v�����X�g�N���A
//-----------------------------------------------------------------------------
void CSerialThread::ClearSerialDataList(void)
{
	size_t					DataNum = 0;


	// ����������������������������������������������������������
	m_cSerialDataListMutex.Lock();

	// �V���A�����M�f�[�^���X�g�ɑ��M�f�[�^�����邩���ׂ�
	DataNum = m_SerialDataList.size();
	if (DataNum != 0)
	{
		// �f�[�^���S�Ė����Ȃ�܂Ń��[�v
		std::list<SERIAL_DATA_TABLE>::iterator		it = m_SerialDataList.begin();
		while (it != m_SerialDataList.end())
		{
			// �����M�v�����ɂ�malloc�Ŏ擾�����̈�������
			if (it->pData != NULL)
			{
				free(it->pData);
			}

			it++;
		}
	}
	m_SerialDataList.clear();

	m_cSerialDataListMutex.Unlock();
	// ����������������������������������������������������������

	return;
}