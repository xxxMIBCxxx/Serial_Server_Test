//*****************************************************************************
// TCP�ʐM��M�X���b�h�N���X
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "CClientTcpRecvThread.h"


#define _CCLIENT_TCP_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g
#define STX											( 0x02 )					// STX
#define ETX											( 0x03 )					// ETX

//-----------------------------------------------------------------------------
// �R���X�g���N�^
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


	// �ڑ������擾
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tServerInfo.tAddr.sin_addr));			// IP�A�h���X�擾
	m_Port = ntohs(m_tServerInfo.tAddr.sin_port);								// �|�[�g�ԍ��擾

	// TCP��M���C�x���g�̏�����
	eEventExRet = m_cRecvInfoEvent.Init();
	if (eEventExRet != CEventEx::RESULT_SUCCESS)
	{
		return;
	}

	// TCP��M��񃊃X�g�̃N���A
	m_RecvInfoList.clear();


	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CClientTcpRecvThread::~CClientTcpRecvThread()
{
	// TCP�ʐM��M�X���b�h��~���Y��l��
	this->Stop();

	// �ēx�ATCP��M��񃊃X�g���N���A����
	RecvInfoList_Clear();
}


//-----------------------------------------------------------------------------
// �G���[�ԍ����擾
//-----------------------------------------------------------------------------
int CClientTcpRecvThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h�J�n
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::Start - Thread Active.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// TCP�ʐM��M�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CClientTcpRecvThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h��~
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::Stop()
{
	bool						bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// TCP�ʐM��M�X���b�h��~
	CThread::Stop();

	// TCP��M��񃊃X�g���N���A����
	RecvInfoList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h
//-----------------------------------------------------------------------------
void CClientTcpRecvThread::ThreadProc()
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
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		perror("CClientTcpRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
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
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		perror("CClientTcpRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return;
	}

	// TCP��M�p�̃\�P�b�g��o�^
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
			// �X���b�h�I���v���C�x���g��M
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// TCP��M�p�̃\�P�b�g
			else if (this->m_tServerInfo.Socket)
			{
				// TCP��M����
				TcpRecvProc();
			}
		}
	}

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CClientTcpRecvThread::ThreadProcCleanup(void* pArg)
{
	// �p�����[�^�`�F�b�N
	if (pArg == NULL)
	{
		return;
	}
	CClientTcpRecvThread* pcClientTcpRecvThread = (CClientTcpRecvThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcClientTcpRecvThread->m_epfd != -1)
	{
		close(pcClientTcpRecvThread->m_epfd);
		pcClientTcpRecvThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// TCP��M��񃊃X�g���N���A����
//-----------------------------------------------------------------------------
void CClientTcpRecvThread::RecvInfoList_Clear()
{
	// ����������������������������������������������������������
	m_cRecvInfoListMutex.Lock();

	std::list<RECV_INFO_TABLE>::iterator		it = m_RecvInfoList.begin();
	while (it != m_RecvInfoList.end())
	{
		RECV_INFO_TABLE			tRecvInfo = *it;

		// �o�b�t�@���m�ۂ���Ă���ꍇ
		if (tRecvInfo.pData != NULL)
		{
			// �o�b�t�@�̈�����
			free(tRecvInfo.pData);
		}
		it++;
	}

	// TCP��M�������X�g���N���A
	m_RecvInfoList.clear();

	m_cRecvInfoListMutex.Unlock();
	// ����������������������������������������������������������
}


//-----------------------------------------------------------------------------
// TCP��M�f�[�^�擾
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::GetRecvData(RECV_INFO_TABLE& tRecvInfo)
{
	bool					bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::GetRecvData - Not Init Proc.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ����������������������������������������������������������
	m_cRecvInfoListMutex.Lock();

	// TCP��M��񃊃X�g�ɓo�^�f�[�^����ꍇ
	if (m_RecvInfoList.empty() != true)
	{
		// TCP��M��񃊃X�g�̐擪�f�[�^�����o���i�����X�g�̐擪�f�[�^�͍폜�j
		std::list<RECV_INFO_TABLE>::iterator		it = m_RecvInfoList.begin();
		tRecvInfo = *it;
		m_RecvInfoList.pop_front();
	}

	m_cRecvInfoListMutex.Unlock();
	// ����������������������������������������������������������

	// TCP��M���f�[�^���擾�����̂ŁATCP��M���C�x���g���N���A����
	m_cRecvInfoEvent.ClearEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M�f�[�^�ݒ�
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::SetRecvData(RECV_INFO_TABLE& tRecvInfo)
{
	bool					bRet = false;


	// �����`�F�b�N
	if (tRecvInfo.pData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		printf("CClientTcpRecvThread::SetRecvData - Not Init Proc.\n");
#endif	// #ifdef _CCLIENT_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ����������������������������������������������������������
	m_cRecvInfoListMutex.Lock();

	// TCP��M��񃊃X�g�ɓo�^
	m_RecvInfoList.push_back(tRecvInfo);

	m_cRecvInfoListMutex.Unlock();
	// ����������������������������������������������������������

	// TCP��M���C�x���g�𑗐M����
	m_cRecvInfoEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M����
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::TcpRecvProc()
{
	ssize_t					read_count = 0;
	// TCP��M����
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
		// �ؒf�����TCP��M�\�P�b�g�̒ʒm�����x������̂ŁATCP��M�\�P�b�g�C�x���g�o�^����������
		epoll_ctl(m_epfd, EPOLL_CTL_DEL, this->m_tServerInfo.Socket, NULL);

		// �ؒf���ꂽ�̂ŁA�ؒf�C�x���g�𑗐M����
		printf("[%s (%d)] - Client Disconnect!\n", m_szIpAddr, m_Port);
		m_pcServerDisconnectEvent->SetEvent();
	}
	else
	{
		// TCP��M�f�[�^���
		TcpRecvDataAnalyze(m_szRecvBuff, read_count);
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M�f�[�^���
//-----------------------------------------------------------------------------
CClientTcpRecvThread::RESULT_ENUM CClientTcpRecvThread::TcpRecvDataAnalyze(char* pRecvData, ssize_t RecvDataNum)
{
	RESULT_ENUM					eRet = RESULT_SUCCESS;

	// �p�����[�^�`�F�b�N
	if (pRecvData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// ��M�����f�[�^��1Byte�����ׂ�
	for (ssize_t i = 0; i < RecvDataNum; i++)
	{
		char		ch = pRecvData[i];


		// ��͎�ʂɂ���ď�����ύX
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
				// ��M�f�[�^��j��
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
				// ��͊���
				m_szCommandBuff[m_CommandPos++] = ch;
				m_eAnalyzeKind = ANALYZE_KIND_STX;

				// TCP��M����
				RECV_INFO_TABLE		tRecvInfo;
				memset(&tRecvInfo, 0x00, sizeof(tRecvInfo));
				tRecvInfo.pReceverClass = this;
				tRecvInfo.DataSize = strlen(m_szCommandBuff) + 1;			// Debug�p
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
				// ��M�f�[�^���i�[
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

