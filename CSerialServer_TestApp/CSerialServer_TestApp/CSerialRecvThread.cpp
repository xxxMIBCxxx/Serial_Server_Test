//*****************************************************************************
// CSerialRecvThread�N���X
//*****************************************************************************
#include "CSerialRecvThread.h"
#include <unistd.h>
#include <sys/epoll.h>

#define _CSERIAL_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CSerialRecvThread::CSerialRecvThread(CSerialRecvThread::CLASS_PARAM_TABLE& tClassParam)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;

	// �N���X�p�����[�^��ێ�
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

	// �V���A���ʐM��M�f�[�^���X�g�p�C�x���g�̏�����
	CEvent::RESULT_ENUM eEventRet = m_cSerialRecvDataListEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::CSerialRecvThread - m_cSerialRecvDataListEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::CSerialRecvThread - m_cSerialRecvDataListEvent.Init Error. [eEventRet:0x%08X]", eEventRet);
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CSerialRecvThread::~CSerialRecvThread()
{

}


//-----------------------------------------------------------------------------
// �V���A���ʐM��M�X���b�h�J�n
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::Start - Not InitProc.");
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Start - Not InitProc.");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::Start - Thread is Active.");
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Start - Thread is Active.");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// �V���A���ʐM��M�X���b�h�J�n
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
// �V���A���ʐM��M�X���b�h��~
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::Stop()
{
	bool						bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// �V���A���ʐM��M�X���b�h��~
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �V���A���ʐM��M�X���b�h
//-----------------------------------------------------------------------------
void CSerialRecvThread::ThreadProc()
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
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::ThreadProc - epoll_create Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
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
		m_tClassParam.pcLog->Output(CLog::LOG_OUTPUT_ERROR, "CSerialRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent] Error. [%d]:%s", m_ErrorNo, strerror(m_ErrorNo));
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// �V���A���ʐM��M�p�t�@�C���f�B�X�N���v�^�o�^
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
			// �X���b�h�I���v���C�x���g��M
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// �V���A���ʐM��M�p�t�@�C���f�B�X�N���v�^
			else if (tEvents[i].data.fd == this->m_tClassParam.SerialFd)
			{
				// �V���A����M
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
		// ��--------------------------------------------------------------------------��
	}

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// �V���A���ʐM��M�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CSerialRecvThread::ThreadProcCleanup(void* pArg)
{
	CSerialRecvThread* pcSerialRecvThread = (CSerialRecvThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcSerialRecvThread->m_epfd != -1)
	{
		close(pcSerialRecvThread->m_epfd);
		pcSerialRecvThread->m_epfd = -1;
	}
}