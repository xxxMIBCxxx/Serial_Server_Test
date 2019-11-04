//*****************************************************************************
// TCP�ʐM��M�X���b�h�N���X
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "CServerTcpRecvThread.h"


#define _CSERVER_TCP_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g
#define STX											( 0x02 )					// STX
#define ETX											( 0x03 )					// ETX

//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CServerTcpRecvThread::CServerTcpRecvThread(CLIENT_INFO_TABLE& tClientInfo)
{
	bool						bRet = false;
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;
	CEventEx::RESULT_ENUM		eEventExRet = CEventEx::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_tClientInfo = tClientInfo;
	memset(m_szIpAddr, 0x00, sizeof(m_szIpAddr));
	m_Port = 0;
	m_eAnalyzeKind = ANALYZE_KIND_STX;
	m_CommandPos = 0;
	memset(m_szRecvBuff, 0x00, sizeof(m_szRecvBuff));
	memset(m_szCommandBuff, 0x00, sizeof(m_szCommandBuff));


	// �ڑ������擾
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tClientInfo.tAddr.sin_addr));			// IP�A�h���X�擾
	m_Port = ntohs(m_tClientInfo.tAddr.sin_port);								// �|�[�g�ԍ��擾

	// �N���C�A���g�ؒf�C�x���g�̏�����
	eEventRet = m_cClientDisconnectEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

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
CServerTcpRecvThread::~CServerTcpRecvThread()
{
	// TCP�ʐM��M�X���b�h��~���Y��l��
	this->Stop();

	// �ēx�ATCP��M��񃊃X�g���N���A����
	RecvInfoList_Clear();
}


//-----------------------------------------------------------------------------
// �G���[�ԍ����擾
//-----------------------------------------------------------------------------
int CServerTcpRecvThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h�J�n
//-----------------------------------------------------------------------------
CServerTcpRecvThread::RESULT_ENUM CServerTcpRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		printf("CServerTcpRecvThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		printf("CServerTcpRecvThread::Start - Thread Active.\n");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// TCP�ʐM��M�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CServerTcpRecvThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h��~
//-----------------------------------------------------------------------------
CServerTcpRecvThread::RESULT_ENUM CServerTcpRecvThread::Stop()
{
	bool						bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		printf("CServerTcpRecvThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
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
void CServerTcpRecvThread::ThreadProc()
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
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		perror("CServerTcpRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
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
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		perror("CServerTcpRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		return;
	}

	// TCP��M�p�̃\�P�b�g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_tClientInfo.Socket;
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_tClientInfo.Socket, &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		perror("CServerTcpRecvThread::ThreadProc - epoll_ctl[ClientInfo.Socket]");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
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
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
			perror("CServerTcpRecvThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
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
			else if (this->m_tClientInfo.Socket)
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
void CServerTcpRecvThread::ThreadProcCleanup(void* pArg)
{
	// �p�����[�^�`�F�b�N
	if (pArg == NULL)
	{
		return;
	}
	CServerTcpRecvThread* pcServerTcpRecvThread = (CServerTcpRecvThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcServerTcpRecvThread->m_epfd != -1)
	{
		close(pcServerTcpRecvThread->m_epfd);
		pcServerTcpRecvThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// TCP��M��񃊃X�g���N���A����
//-----------------------------------------------------------------------------
void CServerTcpRecvThread::RecvInfoList_Clear()
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
// ��TCP��M�f�[�^�ݒ�ƕʃX���b�h�Ŏ擾����ꍇ�A�擾���钼�O�ŃX���b�h��~����
//   �Ɩ߂�l���u���ɃX���b�h�����삵�Ă��Ȃ��FRESULT_ERROR_NOT_ACTIVE�v�ƂȂ�A
//   TCP��M�f�[�^���擾���Ă��Ȃ��ɂ��ւ�炸�f�[�^�̈�����(free�j���Ă��܂�
//	 �Q�d����G���[�i�̈���Ƃ��Ă��Ȃ��ꏊ������j�ƂȂ�s�������
//   ��L���A�{�֐��̖߂�l���uRESULT_SUCCESS�v�̎������擾������M�f�[�^��
//   �Q�Ƃ���悤�ɂ��Ă�������
//-----------------------------------------------------------------------------
CServerTcpRecvThread::RESULT_ENUM CServerTcpRecvThread::GetRecvData(RECV_INFO_TABLE& tRecvInfo)
{
	bool					bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		printf("CServerTcpRecvThread::GetRecvData - Not Init Proc.\n");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
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
	printf("[%s (%d)] - =======================================================\n", m_szIpAddr, m_Port);

	// TCP��M��񃊃X�g�ɓo�^�f�[�^����ꍇ
	if (m_RecvInfoList.empty() != true)
	{
		// TCP��M��񃊃X�g�̐擪�f�[�^�����o���i�����X�g�̐擪�f�[�^�͍폜�j
		std::list<RECV_INFO_TABLE>::iterator		it = m_RecvInfoList.begin();
		tRecvInfo = *it;
		m_RecvInfoList.pop_front();
	}

	printf("[%s (%d)] - pData : %p\n", m_szIpAddr, m_Port, tRecvInfo.pData);
	printf("[%s (%d)] - =======================================================\n", m_szIpAddr, m_Port);
	m_cRecvInfoListMutex.Unlock();
	// ����������������������������������������������������������

	// TCP��M���f�[�^���擾�����̂ŁATCP��M���C�x���g���N���A����
	m_cRecvInfoEvent.ClearEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M�f�[�^�ݒ�
//-----------------------------------------------------------------------------
CServerTcpRecvThread::RESULT_ENUM CServerTcpRecvThread::SetRecvData(RECV_INFO_TABLE& tRecvInfo)
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
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		printf("CServerTcpRecvThread::SetRecvData - Not Init Proc.\n");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
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
	printf("[%s (%d)] - +++++++++++++++++++++++++++++++++++++++++++++++++++++++\n", m_szIpAddr, m_Port);

	// TCP��M��񃊃X�g�ɓo�^
	m_RecvInfoList.push_back(tRecvInfo);

	printf("[%s (%d)] - pData : %p\n", m_szIpAddr, m_Port, tRecvInfo.pData);
	printf("[%s (%d)] - +++++++++++++++++++++++++++++++++++++++++++++++++++++++\n", m_szIpAddr, m_Port);
	m_cRecvInfoListMutex.Unlock();
	// ����������������������������������������������������������

	// TCP��M���C�x���g�𑗐M����
	m_cRecvInfoEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M����
//-----------------------------------------------------------------------------
CServerTcpRecvThread::RESULT_ENUM CServerTcpRecvThread::TcpRecvProc()
{
	ssize_t					read_count = 0;
	// TCP��M����
	memset(m_szRecvBuff, 0x00, sizeof(m_szRecvBuff));
	read_count = read(m_tClientInfo.Socket, m_szRecvBuff, CSERVER_TCP_RECV_THREAD_RECV_BUFF_SIZE);
	if (read_count < 0)
	{
		m_ErrorNo = errno;
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		perror("CServerTcpRecvThread::TcpRecvProc - read");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_RECV;
	}
	else if (read_count == 0)
	{
		// �N���C�A���g�����ؒf�����TCP��M�\�P�b�g�̒ʒm�����x������̂ŁATCP��M�\�P�b�g�C�x���g�o�^����������
		epoll_ctl(m_epfd, EPOLL_CTL_DEL, this->m_tClientInfo.Socket, NULL);

		// �N���C�A���g�����ؒf���ꂽ�̂ŁA�N���C�A���g�ؒf�C�x���g�𑗐M����
		printf("[%s (%d)] - Client Disconnect!\n", m_szIpAddr, m_Port);
		m_cClientDisconnectEvent.SetEvent();
	}
	else
	{
#if 0
		// TCP��M�f�[�^���
		TcpRecvDataAnalyze(m_szRecvBuff, read_count);
#else
		// TCP��M����
		RECV_INFO_TABLE		tRecvInfo;
		memset(&tRecvInfo, 0x00, sizeof(tRecvInfo));
		tRecvInfo.pReceverClass = this;
		tRecvInfo.DataSize = strlen(m_szRecvBuff) + 1;			// Debug�p
		tRecvInfo.pData = (char*)malloc(tRecvInfo.DataSize);
		if (tRecvInfo.pData != NULL)
		{
			memcpy(tRecvInfo.pData, m_szRecvBuff, tRecvInfo.DataSize);
			this->SetRecvData(tRecvInfo);
		}
#endif
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M�f�[�^���
//-----------------------------------------------------------------------------
CServerTcpRecvThread::RESULT_ENUM CServerTcpRecvThread::TcpRecvDataAnalyze(char* pRecvData, ssize_t RecvDataNum)
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
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
					printf("CServerTcpRecvThread::TcpRecvDataAnalyze - malloc error.\n");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
					return RESULT_ERROR_SYSTEM;
				}
				memcpy(tRecvInfo.pData, m_szCommandBuff, tRecvInfo.DataSize);
				eRet = this->SetRecvData(tRecvInfo);
				if (eRet != RESULT_SUCCESS)
				{
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
					printf("CServerTcpRecvThread::TcpRecvDataAnalyze - SetRecvData error.\n");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
					return eRet;
				}
			}
			else
			{
				// ��M�f�[�^���i�[
				m_szCommandBuff[m_CommandPos++] = ch;
				if (m_CommandPos >= CSERVER_TCP_RECV_THREAD_COMMAND_BUFF_SIZE)
				{
#ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
					printf("CServerTcpRecvThread::TcpRecvDataAnalyze - Command Buffer Over.\n");
#endif	// #ifdef _CSERVER_TCP_RECV_THREAD_DEBUG_
					return RESULT_ERROR_COMMAND_BUFF_OVER;
				}
			}
		}
	}

	return RESULT_SUCCESS;
}

