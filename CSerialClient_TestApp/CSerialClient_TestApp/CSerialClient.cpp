//*****************************************************************************
// SerialClient�N���X
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
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g
#define EPOLL_TIMEOUT_TIME							( 500 )						// �T�[�o�[�Đڑ����ԁiepoll�^�C���A�E�g����(ms)�j


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CSerialClient::CSerialClient()
{
	CEvent::RESULT_ENUM  eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_pcClinentResponseThread = NULL;

	// �T�[�o�[�ؒf�C�x���g�̏�����
	eEventRet = m_cServerDisconnectEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CSerialClient::~CSerialClient()
{
	// SerialClient�X���b�h��~�R����l��
	this->Stop();
}


//-----------------------------------------------------------------------------
// SerialClient�X���b�h�J�n
//-----------------------------------------------------------------------------
CSerialClient::RESULT_ENUM CSerialClient::Start()
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

	// SerialClient�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CSerialClient::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// SerialClient�X���b�h��~
//-----------------------------------------------------------------------------
CSerialClient::RESULT_ENUM CSerialClient::Stop()
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

	// �N���C�A���g�����X���b�h��~�E���
	DeleteClientResponseThread();

	// SerialClient�X���b�h��~
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// SerialClient�X���b�h
//-----------------------------------------------------------------------------
void CSerialClient::ThreadProc()
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
#ifdef _CSERIAL_CLIENT_DEBUG_
		perror("CSerialClient::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_
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
#ifdef _CSERIAL_CLIENT_DEBUG_
		perror("CSerialClient::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_
		return;
	}

	// �T�[�o�[�ؒf�C�x���g��o�^
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

	// �N���C�A���g�����X���b�h�����E�J�n
	CreateClientResponseThread();

	// �X���b�h�J�n�C�x���g�𑗐M
	this->m_cThreadStartEvent.SetEvent();


	// ��--------------------------------------------------------------------------��
	// �X���b�h�I���v��������܂Ń��[�v
	// ������Ƀ��[�v�𔲂���ƃX���b�h�I�����Ƀ^�C���A�E�g�ŏI���ƂȂ邽�߁A�X���b�h�I���v���ȊO�͏���Ƀ��[�v�𔲂��Ȃ��ł�������
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
		// �^�C���A�E�g
		else if (nfds == 0)
		{
			// �T�[�o�[�Ɛڑ����Ă��Ȃ��ꍇ�A�T�[�o�[�ڑ������݂�
			if (m_pcClinentResponseThread == NULL)
			{
				// �N���C�A���g�����X���b�h�����E�J�n
				CreateClientResponseThread();
			}
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
			// �T�[�o�[�ؒf�C�x���g
			else if (tEvents[i].data.fd = this->m_cServerDisconnectEvent.GetEventFd())
			{
				m_cServerDisconnectEvent.ClearEvent();
				
				// �N���C�A���g�����X���b�h��~�E���
				DeleteClientResponseThread();
			}
		}
	}
	// ��--------------------------------------------------------------------------��

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// SerialClient�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CSerialClient::ThreadProcCleanup(void* pArg)
{
	CSerialClient* pcSerialClient = (CSerialClient*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcSerialClient->m_epfd != -1)
	{
		close(pcSerialClient->m_epfd);
		pcSerialClient->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// �N���C�A���g�����X���b�h�����E�J�n
//-----------------------------------------------------------------------------
CSerialClient::RESULT_ENUM CSerialClient::CreateClientResponseThread()
{
	// ���ɐ������Ă���ꍇ
	if (m_pcClinentResponseThread != NULL)
	{
		return RESULT_SUCCESS;
	}

	// �N���C�A���g�����X���b�h�N���X�̐���
	m_pcClinentResponseThread = (CClientResponseThread*)new CClientResponseThread(&m_cServerDisconnectEvent);
	if (m_pcClinentResponseThread == NULL)
	{
#ifdef _CSERIAL_CLIENT_DEBUG_
		printf("CSerialClient::CreateClientResponseThread - Create CClientResponseThread Error.\n");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}

	// �N���C�A���g�����X���b�h�J�n
	CClientResponseThread::RESULT_ENUM	eRet = m_pcClinentResponseThread->Start();
	if (eRet != CClientResponseThread::RESULT_SUCCESS)
	{
#ifdef _CSERIAL_CLIENT_DEBUG_
		//		printf("CLandryClient::CreateClientResponseThread - Start CClientResponseThread Error.\n");
#endif	// #ifdef _CSERIAL_CLIENT_DEBUG_

		// �N���C�A���g�����X���b�h��~�E���
		DeleteClientResponseThread();

		return (CSerialClient::RESULT_ENUM)eRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �N���C�A���g�����X���b�h��~�E���
//-----------------------------------------------------------------------------
void CSerialClient::DeleteClientResponseThread()
{
	// �N���C�A���g�����X���b�h�N���X�̔j��
	if (m_pcClinentResponseThread != NULL)
	{
		delete m_pcClinentResponseThread;
		m_pcClinentResponseThread = NULL;
	}
}

