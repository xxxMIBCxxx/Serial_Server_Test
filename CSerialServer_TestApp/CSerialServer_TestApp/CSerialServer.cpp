//*****************************************************************************
// SerialServer�N���X
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
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CSerialServer::CSerialServer()
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	
	// �ڑ��Ď��X���b�h�N���X����
	m_pcServerConnectMonitoringThread = (CServerConnectMonitoringThread*)new CServerConnectMonitoringThread();
	if (m_pcServerConnectMonitoringThread == NULL)
	{
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CSerialServer::~CSerialServer()
{
	// SerialServer�X���b�h��~�R����l��
	this->Stop();

	// �ڑ��Ď��X���b�h�N���X���
	if (m_pcServerConnectMonitoringThread != NULL)
	{
		delete m_pcServerConnectMonitoringThread;
		m_pcServerConnectMonitoringThread = NULL;
	}
}


//-----------------------------------------------------------------------------
// �G���[�ԍ��擾
//-----------------------------------------------------------------------------
int CSerialServer::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// SerialServer�X���b�h�J�n
//-----------------------------------------------------------------------------
CSerialServer::RESULT_ENUM CSerialServer::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// SerialServer�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CSerialServer::RESULT_ENUM)eThreadRet;
	}

	// �Ď��X���b�h�J�n
	CServerConnectMonitoringThread::RESULT_ENUM eServerConnetMonitoringThreadRet = m_pcServerConnectMonitoringThread->Start();
	if (eServerConnetMonitoringThreadRet != CServerConnectMonitoringThread::RESULT_SUCCESS)
	{
		// SerialServer�X���b�h��~
		CThread::Stop();

		return (CSerialServer::RESULT_ENUM)eServerConnetMonitoringThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// SerialServer�X���b�h��~
//-----------------------------------------------------------------------------
CSerialServer::RESULT_ENUM CSerialServer::Stop()
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

	// SerialServer�X���b�h��~
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// SerialServer�X���b�h
//-----------------------------------------------------------------------------
void CSerialServer::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;
	ssize_t						ReadNum = 0;


	// �X���b�h���I������ۂɌĂ΂��֐���o�^
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epoll�t�@�C���f�B�X�N���v�^����
	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
	if (m_epfd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERIAL_SERVER_DEBUG_
		perror("CSerialServer::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_SERVER_DEBUG_
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
#ifdef _CSERIAL_SERVER_DEBUG_
		perror("CSerialServer::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_SERVER_DEBUG_
		return;
	}

	// �X���b�h�J�n�C�x���g�𑗐M
	this->m_cThreadStartEvent.SetEvent();


	// ��--------------------------------------------------------------------------��
	// �X���b�h�I���v��������܂Ń��[�v
	// ������Ƀ��[�v�𔲂���ƃX���b�h�I�����Ƀ^�C���A�E�g�ŏI���ƂȂ邽�߁A�X���b�h�I���v���ȊO�͏���Ƀ��[�v�𔲂��Ȃ��ł�������
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
		// �^�C���A�E�g
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
		}
	}
	// ��--------------------------------------------------------------------------��

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// SerialServer�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CSerialServer::ThreadProcCleanup(void* pArg)
{
	CSerialServer* pcSerialServer = (CSerialServer*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcSerialServer->m_epfd != -1)
	{
		close(pcSerialServer->m_epfd);
		pcSerialServer->m_epfd = -1;
	}
}
