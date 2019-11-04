//*****************************************************************************
// �ڑ��Ď��X���b�h�i�T�[�o�[�Łj�N���X
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "CServerConnectMonitoringThread.h"


#define _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
#define CLIENT_CONNECT_NUM							( 5 )						// �N���C�A���g�ڑ��\��
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CServerConnectMonitoringThread::CServerConnectMonitoringThread()
{
	CEvent::RESULT_ENUM				eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	memset(&m_tServerInfo, 0x00, sizeof(m_tServerInfo));
	m_tServerInfo.Socket = -1;
	m_ServerResponseThreadList.clear();

	// �T�[�o�[�����X���b�h�I���C�x���g������
	eEventRet = m_cServerResponseThreadEndEvent.Init();
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
CServerConnectMonitoringThread::~CServerConnectMonitoringThread()
{
	// �ڑ��Ď��X���b�h��~�R����l��
	this->Stop();

	// �ēx�A���X�g�ɓo�^����Ă���A�T�[�o�[�����X���b�h��S�ĉ������
	ServerResponseThreadList_Clear();
}


//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h�J�n
//-----------------------------------------------------------------------------
CServerConnectMonitoringThread::RESULT_ENUM CServerConnectMonitoringThread::Start()
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

	// �T�[�o�[�ڑ�����������
	eRet = ServerConnectInit(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		return eRet;
	}

	// �N���C�A���g�ڑ��Ď��X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CServerConnectMonitoringThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h��~
//-----------------------------------------------------------------------------
CServerConnectMonitoringThread::RESULT_ENUM CServerConnectMonitoringThread::Stop()
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

	// �N���C�A���g�ڑ��Ď��X���b�h��~
	CThread::Stop();

	// �T�[�o�[���̃\�P�b�g�����
	if (m_tServerInfo.Socket != -1)
	{
		close(m_tServerInfo.Socket);
		memset(&m_tServerInfo, 0x00, sizeof(m_tServerInfo));
		m_tServerInfo.Socket = -1;
	}

	// ���X�g�ɓo�^����Ă���A�T�[�o�[�����X���b�h��S�ĉ������
	ServerResponseThreadList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �T�[�o�[�ڑ�������
//-----------------------------------------------------------------------------
CServerConnectMonitoringThread::RESULT_ENUM CServerConnectMonitoringThread::ServerConnectInit(SERVER_INFO_TABLE& tServerInfo)
{
	int					iRet = 0;


	// �T�[�o�[���̃\�P�b�g�𐶐�
	tServerInfo.Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tServerInfo.Socket == -1)
	{
		m_ErrorNo = errno;
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CServerConnectMonitoringThread::ServerConnectInit - socket");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		return RESULT_ERROR_CREATE_SOCKET;
	}

	// close�����璼���Ƀ\�P�b�g���������悤�ɂ���ibind�ŁuAddress already in use�v�ƂȂ�̂��������j
	const int one = 1;
	setsockopt(tServerInfo.Socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));


	// �T�[�o�[����IP�A�h���X�E�|�[�g��ݒ�
	tServerInfo.tAddr.sin_family = AF_INET;
	tServerInfo.tAddr.sin_port = htons(12345);
	tServerInfo.tAddr.sin_addr.s_addr = INADDR_ANY;
	iRet = bind(tServerInfo.Socket, (struct sockaddr*) & tServerInfo.tAddr, sizeof(tServerInfo.tAddr));
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CServerConnectMonitoringThread::ServerConnectInit - bind");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		close(tServerInfo.Socket);
		tServerInfo.Socket = -1;
		return RESULT_ERROR_BIND;
	}

	// �N���C�A���g������̐ڑ���҂�
	iRet = listen(tServerInfo.Socket, CLIENT_CONNECT_NUM);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CServerConnectMonitoringThread::ServerConnectInit - listen");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		close(tServerInfo.Socket);
		tServerInfo.Socket = -1;
		return RESULT_ERROR_LISTEN;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h
//-----------------------------------------------------------------------------
void CServerConnectMonitoringThread::ThreadProc()
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
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CServerConnectMonitoringThread::ThreadProc - epoll_create");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
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
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CServerConnectMonitoringThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		return;
	}

	// �T�[�o�[�����X���b�h�I���C�x���g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_cServerResponseThreadEndEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_cServerResponseThreadEndEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CServerConnectMonitoringThread::ThreadProc - epoll_ctl[ServerResponseThreadEndEvent]");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		return;
	}

	// �ڑ��v��
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_tServerInfo.Socket;
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_tServerInfo.Socket, &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CServerConnectMonitoringThread::ThreadProc - epoll_ctl[Server Socket]");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
		return;
	}

	// �X���b�h�J�n�C�x���g�𑗐M
	this->m_cThreadStartEvent.SetEvent();

	// �X���b�h�I���v��������܂Ń��[�v
	while (bLoop)
	{
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, -1);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
			perror("CServerConnectMonitoringThread::ThreadProc - epoll_wait");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
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
			// �T�[�o�[�����X���b�h�I���C�x���g
			else if (tEvents[i].data.fd == this->m_cServerResponseThreadEndEvent.GetEventFd())
			{
				this->m_cServerResponseThreadEndEvent.ClearEvent();
				// ���X�g�ɓo�^����Ă���A�N���C�A���g�����X���b�h����X���b�h�I���t���O�������Ă���X���b�h��S�ďI��������
				ServerResponseThreadList_CheckEndThread();
			}
			// �ڑ��v��
			else if (tEvents[i].data.fd == this->m_tServerInfo.Socket)
			{
				// �T�[�o�[�����X���b�h�N���X����
				CServerResponseThread::CLIENT_INFO_TABLE		tClientInfo;
				socklen_t len = sizeof(tClientInfo.tAddr);
				tClientInfo.Socket = accept(this->m_tServerInfo.Socket, (struct sockaddr*) & tClientInfo.tAddr, &len);
				printf("accepted connection from %s, port=%d\n", inet_ntoa(tClientInfo.tAddr.sin_addr), ntohs(tClientInfo.tAddr.sin_port));

				CServerResponseThread* pcServerResponseThread = (CServerResponseThread*)new CServerResponseThread(tClientInfo, &m_cServerResponseThreadEndEvent);
				if (pcServerResponseThread == NULL)
				{
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
					printf("CConnectMonitoringThread::ThreadProc - Create CServerResponseThread error.\n");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
					continue;
				}

				// �T�[�o�[�����X���b�h�J�n
				CServerResponseThread::RESULT_ENUM eRet = pcServerResponseThread->Start();
				if (eRet != CServerResponseThread::RESULT_SUCCESS)
				{
#ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
					printf("CConnectMonitoringThread::ThreadProc - Start CServerResponseThread error.\n");
#endif	// #ifdef _SERVER_CONNECT_MONITORING_THREAD_DEBUG_
					continue;
				}

				// ����������������������������������������������������������
				m_cServerResponceThreadListMutex.Lock();

				// ���X�g�ɓo�^
				m_ServerResponseThreadList.push_back(pcServerResponseThread);

				m_cServerResponceThreadListMutex.Unlock();
				// ����������������������������������������������������������

				continue;
			}
		}
	}

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// �N���C�A���g�ڑ��Ď��X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CServerConnectMonitoringThread::ThreadProcCleanup(void* pArg)
{
	CServerConnectMonitoringThread* pcServerConnectMonitoringThread = (CServerConnectMonitoringThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcServerConnectMonitoringThread->m_epfd != -1)
	{
		close(pcServerConnectMonitoringThread->m_epfd);
		pcServerConnectMonitoringThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// ���X�g�ɓo�^����Ă���A�T�[�o�[�����X���b�h��S�ĉ������
//-----------------------------------------------------------------------------
void CServerConnectMonitoringThread::ServerResponseThreadList_Clear()
{
	// ����������������������������������������������������������
	m_cServerResponceThreadListMutex.Lock();

	std::list<CServerResponseThread*>::iterator it = m_ServerResponseThreadList.begin();
	while (it != m_ServerResponseThreadList.end())
	{
		CServerResponseThread* p = *it;
		if (p != NULL)
		{
			delete p;
		}
		it++;
	}
	m_ServerResponseThreadList.clear();

	m_cServerResponceThreadListMutex.Unlock();
	// ����������������������������������������������������������
}


//-----------------------------------------------------------------------------
// ���X�g�ɓo�^����Ă���A�T�[�o�[�����X���b�h����X���b�h�I���t���O����
// ���Ă���X���b�h��S�ďI��������
//-----------------------------------------------------------------------------
void CServerConnectMonitoringThread::ServerResponseThreadList_CheckEndThread()
{
	// ����������������������������������������������������������
	m_cServerResponceThreadListMutex.Lock();

	std::list<CServerResponseThread*>::iterator it = m_ServerResponseThreadList.begin();
	while (it != m_ServerResponseThreadList.end())
	{
		CServerResponseThread* p = *it;

		// �X���b�h�I���v���t���O�������Ă���H
		if (p->IsServerResponseThreadEnd() == true)
		{
			delete p;
			it = m_ServerResponseThreadList.erase(it);
			continue;
		}

		it++;
	}

	m_cServerResponceThreadListMutex.Unlock();
	// ����������������������������������������������������������
}