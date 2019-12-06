#pragma once
//*****************************************************************************
// LOG�N���X
//*****************************************************************************
#include "CMutex.h"
#include "CEvent.h"
#include "CThread.h"
#include "list"


#define	CLOG_MAX_SIZE					( 1024 )				// ���O�t�@�C���P�s�̍ő啶����
#define CLOG_FILE_NAME_SIZE				( 256 )					// ���O�t�@�C�����̃T�C�Y

class CLog : public CThread
{
public:

	// ���O���(bit)
	typedef enum
	{
		LOG_OUTPUT_ERROR		= 0x80000000,				// �G���[���O
		LOG_OUTPUT_WARNING		= 0x40000000,				// �x�����O
		LOG_OUTPUT_TRACE		= 0x20000000,				// �g���[�X���O
		LOG_OUTPUT_DEBUG		= 0x10000000,				// �f�o�b�O���O
		LOG_OUTPUT_RESERVED_01	= 0x08000000,				// �\��01
		LOG_OUTPUT_RESERVED_02	= 0x04000000,				// �\��02
		LOG_OUTPUT_RESERVED_03	= 0x02000000,				// �\��03
		LOG_OUTPUT_RESERVED_04	= 0x01000000,				// �\��04
		LOG_OUTPUT_RESERVED_05	= 0x00800000,				// �\��05
		LOG_OUTPUT_RESERVED_06	= 0x00400000,				// �\��06
		LOG_OUTPUT_RESERVED_07	= 0x00200000,				// �\��07
		LOG_OUTPUT_RESERVED_08	= 0x00100000,				// �\��08
		LOG_OUTPUT_RESERVED_09	= 0x00080000,				// �\��09
		LOG_OUTPUT_RESERVED_10	= 0x00040000,				// �\��10
		LOG_OUTPUT_RESERVED_11	= 0x00020000,				// �\��11
		LOG_OUTPUT_RESERVED_12	= 0x00010000,				// �\��12
		LOG_OUTPUT_RESERVED_13	= 0x00008000,				// �\��13
		LOG_OUTPUT_RESERVED_14	= 0x00004000,				// �\��14
		LOG_OUTPUT_RESERVED_15	= 0x00002000,				// �\��15
		LOG_OUTPUT_RESERVED_16	= 0x00001000,				// �\��16
		LOG_OUTPUT_RESERVED_17	= 0x00000800,				// �\��17
		LOG_OUTPUT_RESERVED_18	= 0x00000400,				// �\��18
		LOG_OUTPUT_RESERVED_19	= 0x00000200,				// �\��19
		LOG_OUTPUT_RESERVED_20	= 0x00000100,				// �\��20
		LOG_OUTPUT_RESERVED_21	= 0x00000080,				// �\��21
		LOG_OUTPUT_RESERVED_22	= 0x00000040,				// �\��22
		LOG_OUTPUT_RESERVED_23	= 0x00000020,				// �\��23
		LOG_OUTPUT_RESERVED_24	= 0x00000010,				// �\��24
		LOG_OUTPUT_RESERVED_25	= 0x00000008,				// �\��25
		LOG_OUTPUT_RESERVED_26	= 0x00000004,				// �\��26
		LOG_OUTPUT_RESERVED_27	= 0x00000002,				// �\��27
		LOG_OUTPUT_RESERVED_28	= 0x00000001,				// �\��28

	} LOG_OUTPUT_ENUM;


	// ���O�ݒ���\����
	typedef struct
	{
		unsigned int					LogFileSize;								// ���O�t�@�C���̃T�C�Y
		unsigned int					LogOutputBit;								// ���O�o�͔���p�r�b�g
		char							szFileName[CLOG_FILE_NAME_SIZE + 1];		// ���O�t�@�C����
	} LOG_SETTING_INFO_TABLE;


	// ���O�o�͏��e�[�u��
	typedef struct
	{
		unsigned int					Length;										// ���O����
		char*							pszLog;										// ���O
	} LOG_OUTPUT_INFO_TABLE;

	

	CMutex								m_cLogOutputListMutex;						// ���O�o�̓��X�g�p�~���[�e�b�N�X
	bool								m_bStartFlag;								// ���O�J�n�t���O
	LOG_SETTING_INFO_TABLE				m_tLogSettingInfo;							// ���O�ݒ���
	std::list<LOG_OUTPUT_INFO_TABLE>	m_LogOutputList;							// ���O�o�̓��X�g
	char								m_szLogBuff[CLOG_MAX_SIZE + 1];				// ���O�o�͍�Ɨp�o�b�t�@
	char								m_szSettingFile[CLOG_FILE_NAME_SIZE + 1];	// �ݒ���t�@�C��
	
	int									m_ErrorNo;									// �G���[�ԍ�
	int									m_epfd;										// epoll�t�@�C���f�B�X�N���v�^




	CLog();
	~CLog();


	bool Start();
	void Output(LOG_OUTPUT_ENUM eLog, const char* format, ...);

	static char* GetProcessName(char* pszProcessName, unsigned int BuffSize);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void LogWrite(bool bEnd);
};



