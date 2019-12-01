//*****************************************************************************
// �N���C�A���g�����X���b�h
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
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g
#define EPOLL_TIMEOUT_TIME							( 200 )						// epoll�^�C���A�E�g(ms)


//-----------------------------------------------------------------------------
// �R���X�g���N�^
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

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CClientResponseThread::~CClientResponseThread()
{
	// �N���C�A���g�����X���b�h��~�R��l��
	this->Stop();
}


//-----------------------------------------------------------------------------
// �N���C�A���g�����X���b�h�J�n
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::Start()
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

	// �T�[�o�[�ڑ�����
	eRet = ServerConnect(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		return eRet;
	}

	// �N���C�A���g�����X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();

		// �T�[�o�[�ؒf����
		ServerDisconnect(m_tServerInfo);

		return (CClientResponseThread::RESULT_ENUM)eThreadRet;
	}

	// TCP��M�E���M�X���b�h���� & �J�n
	eRet = CreateTcpThread(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		// �ڑ��Ď��X���b�h��~
		CThread::Stop();

		// �T�[�o�[�ؒf����
		ServerDisconnect(m_tServerInfo);

		return eRet;
	}


	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �N���C�A���g�����X���b�h��~
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::Stop()
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

	// TCP��M�E���M�X���b�h��� & ��~
	DeleteTcpThread();

	// �N���C�A���g�����X���b�h��~
	CThread::Stop();

	// �T�[�o�[�ؒf����
	ServerDisconnect(m_tServerInfo);

	return RESULT_SUCCESS;
}



//-----------------------------------------------------------------------------
// �N���C�A���g�����X���b�h
//-----------------------------------------------------------------------------
void CClientResponseThread::ThreadProc()
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
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread::ThreadProc - epoll_create");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
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
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		return;
	}

	// �T�[�o�[�����擾
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tServerInfo.tAddr.sin_addr));			// IP�A�h���X�擾
	m_Port = ntohs(m_tServerInfo.tAddr.sin_port);								// �|�[�g�ԍ��擾

	printf("[%s (%d)] - Server Connect!\n", m_szIpAddr, m_Port);

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
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
			perror("CClientResponseThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
			continue;
		}
		// �^�C���A�E�g
		else if (nfds == 0)
		{
			write(m_tServerInfo.Socket, "Hello!", 7);
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
// �N���C�A���g�����X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CClientResponseThread::ThreadProcCleanup(void* pArg)
{
	CClientResponseThread* pcClientResponseThread = (CClientResponseThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcClientResponseThread->m_epfd != -1)
	{
		close(pcClientResponseThread->m_epfd);
		pcClientResponseThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// �T�[�o�[�ڑ�����
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::ServerConnect(SERVER_INFO_TABLE& tServerInfo)
{
	int					iRet = 0;


	// �\�P�b�g�𐶐�
	tServerInfo.Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tServerInfo.Socket == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread::ServerConnect - socket");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		return RESULT_ERROR_CREATE_SOCKET;
	}

	// �T�[�o�[�ڑ����ݒ�
	tServerInfo.tAddr.sin_family = AF_INET;
	tServerInfo.tAddr.sin_port = htons(12345);							// �� �|�[�g�ԍ�
	tServerInfo.tAddr.sin_addr.s_addr = inet_addr("192.168.10.10");		// �� IP�A�h���X
	if (tServerInfo.tAddr.sin_addr.s_addr == 0xFFFFFFFF)
	{
		// �z�X�g������IP�A�h���X���擾
		struct hostent* host;
		host = gethostbyname("localhost");
		tServerInfo.tAddr.sin_addr.s_addr = *(unsigned int*)host->h_addr_list[0];
	}

	// �T�[�o�[�ɐڑ�����
	iRet = connect(tServerInfo.Socket, (struct sockaddr*) & tServerInfo.tAddr, sizeof(tServerInfo.tAddr));
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread::ServerConnect - connect");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_

		// �T�[�o�[�ؒf����
		ServerDisconnect(tServerInfo);

		return RESULT_ERROR_CONNECT;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �T�[�o�[�ؒf����
//-----------------------------------------------------------------------------
void CClientResponseThread::ServerDisconnect(SERVER_INFO_TABLE& tServerInfo)
{
	// �\�P�b�g����������Ă���ꍇ
	if (tServerInfo.Socket != -1)
	{
		close(tServerInfo.Socket);
	}

	// �T�[�o�[��񏉊���
	memset(&tServerInfo, 0x00, sizeof(tServerInfo));
	tServerInfo.Socket = -1;
}


//-----------------------------------------------------------------------------
// TCP��M�X���b�h���� & �J�n
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::CreateTcpThread(SERVER_INFO_TABLE& tServerInfo)
{
	// TCP��M�X���b�h����
	CClientTcpRecvThread::SERVER_INFO_TABLE		tServer;
	tServer.Socket = m_tServerInfo.Socket;
	memcpy(&tServer.tAddr, &m_tServerInfo.tAddr, sizeof(tServer.tAddr));
	m_pcClientTcpRecvThread = (CClientTcpRecvThread*)new CClientTcpRecvThread(tServer,m_pcServerDisconnectEvent);
	if (m_pcClientTcpRecvThread == NULL)
	{
		return RESULT_ERROR_SYSTEM;
	}

	// TCP��M�X���b�h�J�n
	CClientTcpRecvThread::RESULT_ENUM eRet = m_pcClientTcpRecvThread->Start();
	if (eRet != CClientTcpRecvThread::RESULT_SUCCESS)
	{
		// TCP��M�E���M�X���b�h��� & ��~
		DeleteTcpThread();

		return (CClientResponseThread::RESULT_ENUM)eRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M�X���b�h��� & ��~
//-----------------------------------------------------------------------------
void CClientResponseThread::DeleteTcpThread()
{
	// TCP��M�X���b�h���
	if (m_pcClientTcpRecvThread != NULL)
	{
		delete m_pcClientTcpRecvThread;
		m_pcClientTcpRecvThread = NULL;
	}
}
