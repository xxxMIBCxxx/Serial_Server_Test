//*****************************************************************************
// Thread�N���X
// �������J�I�v�V�����Ɂu-pthread�v��ǉ����邱��
//*****************************************************************************
#include "CThread.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>


#define _CTHREAD_DEBUG_
#define	CTHREAD_START_TIMEOUT				( 3 * 1000 )			// �X���b�h�J�n�҂��^�C���A�E�g(ms)
#define	CTHREAD_END_TIMEOUT					( 3 * 1000 )			// �X���b�h�I���҂��^�C���A�E�g(ms)
#define EPOLL_MAX_EVENTS					( 10 )					// epoll�ő�C�x���g


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CThread::CThread(const char* pszId)
{
	CEvent::RESULT_ENUM			eRet = CEvent::RESULT_SUCCESS;


	m_strId = "";
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_hThread = 0;
//	m_epfd = -1;
	
	// �N���X����ێ�
	if (pszId != NULL)
	{
		m_strId = pszId;
	}

	// �X���b�h�J�n�C�x���g������
	eRet = m_cThreadStartEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// �X���b�h�I���v���C�x���g
	eRet = m_cThreadEndReqEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// �X���b�h�I���p�C�x���g������
	eRet = m_cThreadEndEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CThread::~CThread()
{
	// �X���b�h����~���Ă��Ȃ����Ƃ��l��
	this->Stop();
}


//-----------------------------------------------------------------------------
// �X���b�h�J�n
//-----------------------------------------------------------------------------
CThread::RESULT_ENUM CThread::Start()
{
	int						iRet = 0;
	CEvent::RESULT_ENUM		eEventRet = CEvent::RESULT_SUCCESS;


	// �����������Ŏ��s���Ă���ꍇ
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// ���ɓ��삵�Ă���ꍇ
	if (m_hThread != 0)
	{
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// �X���b�h�J�n
	this->m_cThreadStartEvent.ClearEvent();
	this->m_cThreadEndReqEvent.ClearEvent();
	iRet = pthread_create(&m_hThread, NULL, ThreadLauncher, this);
	if (iRet != 0)
	{
		m_ErrorNo = errno;
#ifdef _CTHREAD_DEBUG_
		perror("CThread::Start - pthread_create");
#endif	// #ifdef _CTHREAD_DEBUG_
		return RESULT_ERROR_START;
	}

	// �X���b�h�J�n�C�x���g�҂�
	eEventRet = this->m_cThreadStartEvent.Wait(CTHREAD_START_TIMEOUT);
	switch (eEventRet) {
	case CEvent::RESULT_RECIVE_EVENT:			// �X���b�h�J�n�C�x���g����M
		this->m_cThreadStartEvent.ClearEvent();
		break;

	case CEvent::RESULT_WAIT_TIMEOUT:			// �^�C���A�E�g
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Start - WaitTimeout\n");
#endif	// #ifdef _CTHREAD_DEBUG_
		pthread_cancel(m_hThread);
		pthread_join(m_hThread, NULL);
		m_hThread = 0;
		return RESULT_ERROR_START_TIMEOUT;

	default:
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Start - Wait Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
		pthread_cancel(m_hThread);
		pthread_join(m_hThread, NULL);
		m_hThread = 0;
		return RESULT_ERROR_SYSTEM;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �X���b�h��~
//-----------------------------------------------------------------------------
CThread::RESULT_ENUM CThread::Stop()
{
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;


	// �����������Ŏ��s���Ă���ꍇ
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// ���ɒ�~���Ă���ꍇ
	if (m_hThread == 0)
	{
		return RESULT_SUCCESS;
	}

	// �X���b�h���~������i�X���b�h�I���v���C�x���g�𑗐M�j
	this->m_cThreadEndEvent.ClearEvent();
	eEventRet = this->m_cThreadEndReqEvent.SetEvent();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Stop - SetEvent Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
		
		// �X���b�h��~�Ɏ��s�����ꍇ�́A�����I�ɏI��������
		pthread_cancel(m_hThread);
	}
	else
	{
		// �X���b�h�I���C�x���g�҂�
		eEventRet = this->m_cThreadEndEvent.Wait(CTHREAD_END_TIMEOUT);
		switch (eEventRet) {
		case CEvent::RESULT_RECIVE_EVENT:			// �X���b�h�I���C�x���g����M
			this->m_cThreadEndEvent.ClearEvent();
			break;

		case CEvent::RESULT_WAIT_TIMEOUT:			// �^�C���A�E�g
#ifdef _CTHREAD_DEBUG_
			printf("CThread::Stop - Timeout\n");
#endif	// #ifdef _CTHREAD_DEBUG_
			pthread_cancel(m_hThread);
			break;

		default:
#ifdef _CTHREAD_DEBUG_
			printf("CThread::Stop - Wait Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
			pthread_cancel(m_hThread);
			break;
		}
	}
	pthread_join(m_hThread, NULL);
	m_hThread = 0;

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �G���[�ԍ����擾
//-----------------------------------------------------------------------------
int CThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// �X���b�h�����삵�Ă��邩�̊m�F
//-----------------------------------------------------------------------------
bool CThread::IsActive()
{
	return ((m_hThread != 0) ? true : false);
}


//-----------------------------------------------------------------------------
// �X���b�h�J�n�C�x���g�t�@�C���f�B�X�N���v�^���擾
//-----------------------------------------------------------------------------
int CThread::GetThreadStartEventFd()
{
	return m_cThreadStartEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// �X���b�h�I���v���C�x���g�t�@�C���f�B�X�N���v�^���擾
//-----------------------------------------------------------------------------
int CThread::GetThreadEndReqEventFd()
{
	return m_cThreadEndReqEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// �X���b�h�I���C�x���g�t�@�C���f�B�X�N���v�^���擾
//-----------------------------------------------------------------------------
int CThread::GetThreadEndEventFd()
{
	return m_cThreadEndEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// �X���b�h�Ăяo��
//-----------------------------------------------------------------------------
void* CThread::ThreadLauncher(void* pUserData)
{
	// �X���b�h�����Ăяo��
	reinterpret_cast<CThread*>(pUserData)->ThreadProc();

	return (void *)NULL;
}


//-----------------------------------------------------------------------------
// �X���b�h�����i���T���v�����j
//-----------------------------------------------------------------------------
void CThread::ThreadProc()
{
//	int							iRet = 0;
//	struct epoll_event			tEvent;
//	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
//	bool						bLoop = true;
//	struct tm					tTm;
//	char						szTime[64 + 1];
//	struct timespec				tTimeSpec;
//
//
//	// �X���b�h���I������ۂɌĂ΂��֐���o�^
//	pthread_cleanup_push(ThreadProcCleanup, this);
//
//	// epoll�t�@�C���f�B�X�N���v�^����
//	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
//	if (m_epfd == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CTHREAD_DEBUG_
//		perror("CThread::ThreadProc - epoll_create");
//#endif	// #ifdef _CTHREAD_DEBUG_
//		return;
//	}
//
//	// �X���b�h�I���v���C�x���g��o�^
//	memset(&tEvent, 0x00, sizeof(tEvent));
//	tEvent.events = EPOLLIN;
//	tEvent.data.fd = this->GetThreadEndReqEventFd();
//	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->GetThreadEndReqEventFd(), &tEvent);
//	if (iRet == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CTHREAD_DEBUG_
//		perror("CThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
//#endif	// #ifdef _CTHREAD_DEBUG_
//		return;
//	}
//
//#ifdef _CTHREAD_DEBUG_
//	printf("-- Thread %s Start --\n", m_strId.c_str());
//#endif	// #ifdef _CTHREAD_DEBUG_
//	// �X���b�h�J�n�C�x���g�𑗐M
//	m_cThreadStartEvent.SetEvent();
//
//	// ��--------------------------------------------------------------------------��
//	// �X���b�h�I���v��������܂Ń��[�v
//	// ������Ƀ��[�v�𔲂���ƃX���b�h�I�����Ƀ^�C���A�E�g�ŏI���ƂȂ邽�߁A�X���b�h�I���v���ȊO�͏���Ƀ��[�v�𔲂��Ȃ��ł�������
//	while (bLoop)
//	{
//		memset(tEvents, 0x00, sizeof(tEvents));
//		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, 1000);
//		if (nfds == -1)
//		{
//			m_ErrorNo = errno;
//#ifdef _CTHREAD_DEBUG_
//			perror("CThread::ThreadProc - epoll_wait");
//#endif	// #ifdef _CTHREAD_DEBUG_
//			bLoop = false;
//			continue;
//		}
//		else if (nfds == 0)
//		{
//			// ���ݎ������擾
//			memset(szTime, 0x00, sizeof(szTime));
//			clock_gettime(CLOCK_REALTIME, &tTimeSpec);
//			localtime_r(&tTimeSpec.tv_sec, &tTm);
//			sprintf(szTime, "[%04d/%02d/%02d %02d:%02d:%02d.%03ld]",
//				tTm.tm_year + 1900,
//				tTm.tm_mon + 1,
//				tTm.tm_mday,
//				tTm.tm_hour,
//				tTm.tm_min,
//				tTm.tm_sec,
//				tTimeSpec.tv_nsec / 1000000);
//
//#ifdef _CTHREAD_DEBUG_
//			printf("%s : CThread::ThreadProc.\n", szTime);
//#endif	// #ifdef _CTHREAD_DEBUG_
//			continue;
//		}
//
//		for (int i = 0; i < nfds; i++)
//		{
//			// �X���b�h�I���v���C�x���g��M
//			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
//			{
//				bLoop = false;
//				break;
//			}
//		}
//	}
//	// ��--------------------------------------------------------------------------��
//
//#ifdef _CTHREAD_DEBUG_
//	printf("-- Thread %s End --\n", m_strId.c_str());
//#endif	// #ifdef _CTHREAD_DEBUG_
//	// �X���b�h�I���C�x���g�𑗐M
//	m_cThreadEndEvent.SetEvent();
//
//	pthread_cleanup_pop(1);
}


////-----------------------------------------------------------------------------
//// �X���b�h�I�����ɌĂ΂�鏈��
////-----------------------------------------------------------------------------
//void CThread::ThreadProcCleanup(void* pArg)
//{
//	CThread* pcThread = (CThread*)pArg;
//
//
//	// epoll�t�@�C���f�B�X�N���v�^���
//	if (pcThread->m_epfd != -1)
//	{
//		close(pcThread->m_epfd);
//		pcThread->m_epfd = -1;
//	}
//}