#pragma once
//*****************************************************************************
// CSerialThread�N���X
//*****************************************************************************
#include "CThread.h"
#include "CLog.h"
#include "CConfFile.h"
#include "CEvent.h"
#include "CEventEx.h"
#include "CMutex.h"
#include "list"
#include "CSerialRecvThread.h"
#include "ResultHeader.h"


#define	SERIAL_DEVICE_NAME						( 64 )


class CSerialThread : public CThread
{
public:
	// �V���A���X���b�h�N���X�̃p�����[�^
	typedef struct
	{
		CLog*									pcLog;							// CLog�N���X
		CConfFile*								pcConfFile;						// ConfFile�N���X

	} CLASS_PARAM_TABLE;


	// �V���A���ʐM�f�[�^�\����
	typedef struct
	{
		void*									pClas;							// ���M�v�����N���X�̃A�h���X
		unsigned int							Size;							// �f�[�^�T�C�Y
		char*									pData;							// �f�[�^�imalloc�ŗ̈���m�ۂ��Ă���A�f�[�^���Z�b�g���Ă��������j
																				// �����CSerialThread���ŉ��(free)���܂�
	} SERIAL_DATA_TABLE;


	// �{�[���[�g�ϊ��\����
	typedef struct
	{
		int				Baudrate;
		int				BaureateDefine;
		const char*		pszBaudrate;
	} BAUDRATE_CONV_TABLE;

	// �f�[�^���ϊ��\����
	typedef struct
	{
		int				DataSize;
		int				DataSizeDefine;
		const char*		pszDataSize;
	} DATA_SIZE_CONV_TABLE;

	// �X�g�b�v�r�b�g�ϊ��\����
	typedef struct
	{
		int				StopBit;
		int				StopBitDefine;
		const char*		pszStopBit;
	} STOP_BIT_CONV_TABLE;


	// �p���e�B�ϊ��\����
	typedef struct
	{
		int				Parity;
		int				ParityDefine;
		const char*		pszParity;
	} PARITY_CONV_TABLE;


	// �V���A���ʐM�ݒ�\����
	typedef struct
	{
		char									szDevName[SERIAL_DEVICE_NAME + 1];		// �f�o�C�X��(ex:/dev/tty***)
		BAUDRATE_CONV_TABLE						tBaudRate;								// �{�[���[�g
		DATA_SIZE_CONV_TABLE					tDataSize;								// �f�[�^��
		STOP_BIT_CONV_TABLE						tStopBit;								// �X�g�b�v�r�b�g
		PARITY_CONV_TABLE						tParity;								// �p���e�B�r�b�g

	} SERIAL_CONF_TABLE;


private:
	bool										m_bInitFlag;					// �����������t���O
	int											m_ErrorNo;						// �G���[�ԍ�
	int											m_epfd;							// epoll�t�@�C���f�B�X�N���v�^
	CLASS_PARAM_TABLE							m_tClassParam;					// �V���A���X���b�h�N���X�̃p�����[�^

	// < IRQ�V���A���ʐM�f�[�^���X�g�֘A >
	CMutex										m_cIrqSerialDataListMutex;		// IRQ�V���A���ʐM�f�[�^���X�g�p�~���[�e�b�N�X
	std::list<SERIAL_DATA_TABLE>				m_IrqSerialDataList;			// IRQ�V���A���ʐM�f�[�^���X�g


	// < �ʏ�V���A���ʐM�f�[�^���X�g�֘A >
	CMutex										m_cSerialDataListMutex;			// �ʏ�V���A���ʐM�f�[�^���X�g�p�~���[�e�b�N�X
	std::list<SERIAL_DATA_TABLE>				m_SerialDataList;				// �ʏ�V���A���ʐM�f�[�^���X�g


	SERIAL_CONF_TABLE							m_tSerialConfInfo;				// �V���A���ʐM�ݒ���
	int											m_SerialFd;						// �V���A���ʐM�p�t�@�C���f�B�X�N���v�^

	CEvent										m_cSerialDataEvent;				// �V���A���ʐM�v���C�x���g�iIRQ�E�ʏ틤�ʁj
	CEvent										m_cSerialRecvEndEvent;			// �V���A����M�����C�x���g

	CSerialRecvThread*							m_pcSerialRecvThread;			// �V���A���ʐM��M�X���b�h


public:
	CSerialThread(CLASS_PARAM_TABLE &tClassParam);
	virtual ~CSerialThread();

	RESULT_HEADER_ENUM Start();
	RESULT_HEADER_ENUM Stop();
	RESULT_HEADER_ENUM SetIrqSerialDataList(SERIAL_DATA_TABLE& tSerialData);
	RESULT_HEADER_ENUM SetSerialDataList(SERIAL_DATA_TABLE& tSerialData);


private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void GetSerialConfInfo(void);
	RESULT_HEADER_ENUM Open(SERIAL_CONF_TABLE& tSerialConfInfo);
	RESULT_HEADER_ENUM Close(void);
	bool SerialSendProc(int* pEpfd, struct epoll_event* ptEvents, int MaxEvents, int Timeout);
	void ClearIrqSerialDataList(void);
	void ClearSerialDataList(void);


};