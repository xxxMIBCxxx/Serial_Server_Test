//*****************************************************************************
// �T�[�o�[�����X���b�h�N���X
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "CServerResponseThread.h"


#define _CSERVER_RESPONSE_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CServerResponseThread::CServerResponseThread(CLIENT_INFO_TABLE& tClientInfo, CEvent* pcServerResponceThreadEndEvent)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	memset(&m_tClientInfo, 0x00, sizeof(m_tClientInfo));
	memset(m_szIpAddr, 0x00, sizeof(m_szIpAddr));
	m_Port = 0;
	m_pcServerTcpRecvThread = NULL;
	m_pcServerResponceThreadEndEvent = NULL;
	m_bServerResponceThreadEnd = false;

	// �T�[�o�[�����X���b�h�I���C�x���g�̃`�F�b�N
	if (pcServerResponceThreadEndEvent == NULL)
	{
		return;
	}
	m_pcServerResponceThreadEndEvent = pcServerResponceThreadEndEvent;

	// �N���C�A���g�ؒf�C�x���g�̏�����
	CEvent::RESULT_ENUM eEventRet = m_cClientDisconnectEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// �N���C�A���g�����擾
	memcpy(&m_tClientInfo, &tClientInfo, sizeof(CLIENT_INFO_TABLE));
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tClientInfo.tAddr.sin_addr));			// IP�A�h���X�擾
	m_Port = ntohs(m_tClientInfo.tAddr.sin_port);								// �|�[�g�ԍ��擾

	// TCP��M�X���b�h�N���X�𐶐�
	CServerTcpRecvThread::CLIENT_INFO_TABLE				tServerTcpRecvThread_ClientInfo;
	tServerTcpRecvThread_ClientInfo.Socket = tClientInfo.Socket;
	tServerTcpRecvThread_ClientInfo.tAddr = tClientInfo.tAddr;
	m_pcServerTcpRecvThread = (CServerTcpRecvThread*)new CServerTcpRecvThread(tServerTcpRecvThread_ClientInfo);
	if (m_pcServerTcpRecvThread == NULL)
	{
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CServerResponseThread::~CServerResponseThread()
{
	// �T�[�o�[�����X���b�h��~�R����l��
	this->Stop();

	// TCP��M�X���b�h�N���X��j��
	if (m_pcServerTcpRecvThread != NULL)
	{
		delete m_pcServerTcpRecvThread;
		m_pcServerTcpRecvThread = NULL;
	}

	// �N���C�A���g���̃\�P�b�g�����
	if (m_tClientInfo.Socket != -1)
	{
		close(m_tClientInfo.Socket);
		m_tClientInfo.Socket = -1;
	}
}


//-----------------------------------------------------------------------------
// �T�[�o�[�����X���b�h�J�n
//-----------------------------------------------------------------------------
CServerResponseThread::RESULT_ENUM CServerResponseThread::Start()
{
	bool						bRet = false;
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

	// TCP��M�X���b�h�J�n
	CServerTcpRecvThread::RESULT_ENUM eServerTcpRecvThreadResult = m_pcServerTcpRecvThread->Start();
	if (eServerTcpRecvThreadResult != CServerTcpRecvThread::RESULT_SUCCESS)
	{
		m_ErrorNo = m_pcServerTcpRecvThread->GetErrorNo();
		return (CServerResponseThread::RESULT_ENUM)eServerTcpRecvThreadResult;
	}

	// �T�[�o�[�����X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CServerResponseThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �T�[�o�[�����X���b�h��~
//-----------------------------------------------------------------------------
CServerResponseThread::RESULT_ENUM CServerResponseThread::Stop()
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

	// TCP��M�X���b�h��~
	m_pcServerTcpRecvThread->Stop();

	// �T�[�o�[�����X���b�h��~
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �T�[�o�[�����X���b�h
//-----------------------------------------------------------------------------
void CServerResponseThread::ThreadProc()
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
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		perror("CServerResponseThread - epoll_create");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
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
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		perror("CServerResponseThread - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		return;
	}

	// �N���C�A���g�ؒf�C�x���g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_pcServerTcpRecvThread->m_cClientDisconnectEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_pcServerTcpRecvThread->m_cClientDisconnectEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		perror("CServerResponseThread - epoll_ctl[ClientDisconnectEvent]");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		return;
	}

	// TCP��M���C�x���g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_pcServerTcpRecvThread->m_cRecvInfoEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_pcServerTcpRecvThread->m_cRecvInfoEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
		perror("CServerResponseThread - epoll_ctl[TcpRecvResponseEvent]");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
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
#ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
			perror("CServerResponseThread - epoll_wait");
#endif	// #ifdef _CSERVER_RESPONSE_THREAD_DEBUG_
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
			// �N���C�A���g�ؒf�C�x���g
			else if (tEvents[i].data.fd == this->m_pcServerTcpRecvThread->m_cClientDisconnectEvent.GetEventFd())
			{
				this->m_pcServerTcpRecvThread->m_cClientDisconnectEvent.ClearEvent();

				// �T�[�o�[�����X���b�h�I���t���O�𗧂ĂāA�T�[�o�[�����X���b�h�I���C�x���g�𑗐M����
				m_bServerResponceThreadEnd = true;
				m_pcServerResponceThreadEndEvent->SetEvent();
				break;
			}
			// TCP��M���C�x���g
			else if (tEvents[i].data.fd == this->m_pcServerTcpRecvThread->m_cRecvInfoEvent.GetEventFd())
			{
				CServerTcpRecvThread::RECV_INFO_TABLE		tRecvInfo;
				CServerTcpRecvThread::RESULT_ENUM eServerTcpRecvThreadRet = this->m_pcServerTcpRecvThread->GetRecvData(tRecvInfo);
				if (eServerTcpRecvThreadRet == CServerTcpRecvThread::RESULT_SUCCESS)
				{
					printf("[%s (%d)] - %s\n", this->m_szIpAddr, this->m_Port, tRecvInfo.pData);
					if (tRecvInfo.pData != NULL)
					{
						free(tRecvInfo.pData);
					}
				}
			}
		}
	}
	// ��--------------------------------------------------------------------------��

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// �T�[�o�[�����X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CServerResponseThread::ThreadProcCleanup(void* pArg)
{
	CServerResponseThread* pcServerResponseThread = (CServerResponseThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcServerResponseThread->m_epfd != -1)
	{
		close(pcServerResponseThread->m_epfd);
		pcServerResponseThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// �T�[�o�[�����X���b�h�I�����������̂����ׂ�
//-----------------------------------------------------------------------------
bool CServerResponseThread::IsServerResponseThreadEnd()
{
	return m_bServerResponceThreadEnd;
}
