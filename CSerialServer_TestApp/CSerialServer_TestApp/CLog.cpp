//*****************************************************************************
// LOG�N���X
//*****************************************************************************
#include "CLog.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>


#define _CLOG_DEBUG_

#define DEFAULT_LOG_FILE_SIZE			( 1024 * 1024 * 4 )							// �f�t�H���g���O�t�@�C���T�C�Y(4MB)
#define MIN_LOG_FILE_SIZE				( 1024 * 1 )								// �ŏ����O�t�@�C���T�C�Y(1MB)
#define MAX_LOG_FILE_SIZE				( 1024 * 1024 * 10 )						// �ő働�O�t�@�C���T�C�Y(10MB)
#define	DEFAULT_LOG_OUTPUT				( CLog::LOG_OUTPUT_ERROR )					// �f�t�H���g���O�o�̓r�b�g
#define LOG_OUTPUT_COUNT				( 25 )										// �P��̏����ɏo�͂ł��郍�O��

#define EPOLL_MAX_EVENTS				( 10 )										// epoll�ő�C�x���g
#define EPOLL_TIMEOUT_TIME				( 1000 )									// epoll�^�C���A�E�g(ms) ��1�b���Ƀ��O�����݂��s���܂���


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CLog::CLog()
{
	char				szProcessName[256 + 1];


	m_bStartFlag = false;
	m_epfd = -1;
	m_ErrorNo = 0;

	// ���O�ݒ�����f�t�H���g�l�ŏ�����
	memset(&m_tLogSettingInfo, 0x00, sizeof(m_tLogSettingInfo));
	m_tLogSettingInfo.LogOutputBit = DEFAULT_LOG_OUTPUT;
	m_tLogSettingInfo.LogFileSize = DEFAULT_LOG_FILE_SIZE;
	memset(szProcessName, 0x00, sizeof(szProcessName));
	sprintf(m_tLogSettingInfo.szFileName, "/var/tmp/%s.log", GetProcessName(szProcessName, 256));			// ���O�o�͐�

	// ���O�o�̓��X�g���N���A
	m_LogOutputList.clear();

	memset(m_szLogBuff, 0x00, sizeof(m_szLogBuff));
	memset(m_szSettingFile, 0x00, sizeof(m_szSettingFile));

	// ���O�o�̓X���b�h�J�n
	Start();
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CLog::~CLog()
{
	// ���O�o�̓X���b�h���~������
	CThread::Stop();
}


//-----------------------------------------------------------------------------
// ���O�J�n
//-----------------------------------------------------------------------------
bool CLog::Start()
{
	bool					bRet = false;
	FILE*					pFile = NULL;
	CThread::RESULT_ENUM	eThreadRet = CThread::RESULT_SUCCESS;


	// ���Ƀ��O�J�n���Ă���ꍇ
	if (m_bStartFlag == true)
	{
		return true;
	}

	// �t�@�C���̑��ݗL�����m�F���邽�߁A�ꎞ�I�ɃI�[�v������
	pFile = fopen(m_tLogSettingInfo.szFileName, "r+");
	if (errno == ENOENT)
	{
		pFile = fopen(m_tLogSettingInfo.szFileName, "w+");
	}

	if (pFile == NULL)
	{
#ifdef _CLOG_DEBUG_
		perror("CLog::Start - fopen");
#endif	// #ifdef _CLOG_DEBUG_
		return false;
	}
	fclose(pFile);

	// ���O�������ݗp�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		return false;
	}

	m_bStartFlag = true;

	return true;
}



//-----------------------------------------------------------------------------
// ���O�o��
//-----------------------------------------------------------------------------
void CLog::Output(LOG_OUTPUT_ENUM eLog, const char* format, ...)
{
	struct timespec		tTimeSpec;
	struct tm			tTm;
	char				szTime[64 + 1];
	unsigned int		Length;


	// ���O�J�n���Ă��Ȃ��ꍇ
	if (m_bStartFlag == false)
	{
		return;
	}

	// ���O�o�͔���
	if (!(m_tLogSettingInfo.LogOutputBit & (unsigned int)eLog))
	{
		return;
	}

	// ���ݎ������擾
	memset(szTime, 0x00, sizeof(szTime));
	clock_gettime(CLOCK_REALTIME, &tTimeSpec);
	localtime_r(&tTimeSpec.tv_sec, &tTm);
	sprintf(szTime, "[%04d/%02d/%02d %02d:%02d:%02d.%03ld] ",	
		tTm.tm_year + 1900,
		tTm.tm_mon + 1,
		tTm.tm_mday,
		tTm.tm_hour,
		tTm.tm_min,
		tTm.tm_sec,
		tTimeSpec.tv_nsec / 1000000);

	// �ψ�����W�J
	memset(m_szLogBuff, 0x00, sizeof(m_szLogBuff));
	va_list			ap;
	va_start(ap, format);
	vsprintf(m_szLogBuff, format, ap);
	va_end(ap);

	// ���O�o�̓��X�g�ɓo�^
	LOG_OUTPUT_INFO_TABLE		tLogOutput;
	tLogOutput.Length = strlen(szTime) + strlen(m_szLogBuff) + 1 + 1;			// CR + NULL
	tLogOutput.pszLog = (char*)malloc(tLogOutput.Length);
	if (tLogOutput.pszLog != NULL)
	{
		memset(tLogOutput.pszLog, 0x00, tLogOutput.Length);
		Length = strlen(szTime);
		strncpy(tLogOutput.pszLog, szTime, Length);
		strcat(&tLogOutput.pszLog[Length], m_szLogBuff);
		Length += strlen(m_szLogBuff);
		strcat(&tLogOutput.pszLog[Length], "\n");

		// ������������������������������������������������������������
		m_cLogOutputListMutex.Lock();

		m_LogOutputList.push_back(tLogOutput);

		m_cLogOutputListMutex.Unlock();
		// ������������������������������������������������������������
	}
}


//-----------------------------------------------------------------------------
// ���O�o�̓X���b�h
//-----------------------------------------------------------------------------
void CLog::ThreadProc()
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
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT_TIME);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
			continue;
		}
		// �^�C���A�E�g�i���O�������݁j
		else if (nfds == 0)
		{

			// ������������������������������������������������������������
			m_cLogOutputListMutex.Lock();

			// ���O�o��
			LogWrite(false);

			m_cLogOutputListMutex.Unlock();
			// ������������������������������������������������������������

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

	pthread_cleanup_pop(1);

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();
}



//-----------------------------------------------------------------------------
// �N���C�A���g�����X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CLog::ThreadProcCleanup(void* pArg)
{
	CLog* pcLog = (CLog*)pArg;

	// ������������������������������������������������������������
	pcLog->m_cLogOutputListMutex.Lock();

	// ���O�o��
	pcLog->LogWrite(true);

	pcLog->m_cLogOutputListMutex.Unlock();
	// ������������������������������������������������������������

	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcLog->m_epfd != -1)
	{
		close(pcLog->m_epfd);
		pcLog->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// ���O�o�͏���
//-----------------------------------------------------------------------------
void CLog::LogWrite(bool bEnd)
{
	char					szBuff[16];
	unsigned int			Pos = 0;
	unsigned int			Count = 0;
	FILE*					pFile = NULL;


	// ���O�o�̓f�[�^���Ȃ���Ώ������s��Ȃ�
	if (m_LogOutputList.size() == 0)
	{
		return;
	}

	// �t�@�C�����I�[�v������
	pFile = fopen(m_tLogSettingInfo.szFileName, "r+");
	if (errno == ENOENT)
	{
		pFile = fopen(m_tLogSettingInfo.szFileName, "w+");
	}
	if (pFile == NULL)
	{
#ifdef _CLOG_DEBUG_
		perror("CLog::LogWrite - fopen");
#endif	// #ifdef _CLOG_DEBUG_
		return;
	}

	// ���O�o�̓f�[�^���Ȃ��Ȃ�܂Ń��[�v
	while (m_LogOutputList.size() != 0)
	{
		// ���O�o�͂̐擪�f�[�^���擾
		std::list<LOG_OUTPUT_INFO_TABLE>::iterator			it = m_LogOutputList.begin();

		// �t�@�C�������݈ʒu���擾
		memset(szBuff, 0x00, sizeof(szBuff));
		fseek(pFile, 0, SEEK_SET);
		fread(szBuff, 8, 1, pFile);
		Pos = atol(szBuff);
		if (Pos == 0)
		{
			sprintf(szBuff, "%08d\n", Pos);
			fseek(pFile, 0, SEEK_SET);
			fwrite(szBuff, 9, 1, pFile);
			Pos = 9;
		}


		// ���O�������ލہA�t�@�C���ő�T�C�Y�𒴂��Ă��邩���ׂ�
		if ((Pos + it->Length) > m_tLogSettingInfo.LogFileSize)
		{
			Pos = 9;
		}

		// ���O������
		fseek(pFile, Pos, SEEK_SET);
		fwrite(it->pszLog, (it->Length - 1), 1, pFile);
		Pos += (it->Length - 1);

		// �t�@�C���ʒu������
		memset(szBuff, 0x00, sizeof(szBuff));
		sprintf(szBuff, "%08d\n", Pos);
		fseek(pFile, 0, SEEK_SET);
		fwrite(szBuff, 9, 1, pFile);

		// ���O�o�̓��X�g�̐擪���O�����폜
		if (it->pszLog != NULL)
		{
			free(it->pszLog);
		}
		m_LogOutputList.pop_front();

		// �I����������Ă΂�Ă��Ȃ��ꍇ
		if (bEnd == false)
		{
			Count++;

			if (Count >= LOG_OUTPUT_COUNT)
			{
				break;
			}
		}
	}

	// �t�@�C�������
	fclose(pFile);
}


//-----------------------------------------------------------------------------
// �v���Z�X���̂��擾
//-----------------------------------------------------------------------------
char* CLog::GetProcessName(char* pszProcessName, unsigned int BuffSize)
{
	char*			pPos = NULL;


	// �����`�F�b�N
	if ((pszProcessName == NULL) || (BuffSize == 0))
	{
		return NULL;
	}

	// �v���Z�X���̂��R�s�[
	strncpy(pszProcessName, program_invocation_short_name, BuffSize);

	// �g���q������ꍇ�́A�g���q���폜
	pPos = strchr(pszProcessName, '.');
	if (pPos != NULL)
	{
		*pPos = '\0';
	}

	return pszProcessName;
}

	