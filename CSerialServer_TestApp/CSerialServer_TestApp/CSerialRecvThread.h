#pragma once
//*****************************************************************************
// CSerialRecvThread�N���X
//*****************************************************************************
#include "CThread.h"
#include "CEvent.h"
#include "CLog.h"
#include "CConfFile.h"
#include "CMutex.h"
#include "list"


class CSerialRecvThread : public CThread
{
public:
	// �V���A����M�X���b�h�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// ����I��
		RESULT_ERROR_INIT = 0xE00000001,										// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,										// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[
	} RESULT_ENUM;


	// �V���A����M�X���b�h�N���X�̃p�����[�^
	typedef struct
	{
		CLog*									pcLog;							// CLog�N���X
		int										SerialFd;						// �V���A���ʐM�p�t�@�C���f�B�X�N���v�^

	} CLASS_PARAM_TABLE;


private:
	bool										m_bInitFlag;					// �����������t���O
	int											m_ErrorNo;						// �G���[�ԍ�
	int											m_epfd;							// epoll�t�@�C���f�B�X�N���v�^

	CLASS_PARAM_TABLE							m_tClassParam;					// �V���A����M�X���b�h�N���X�̃p�����[�^
	std::list<int>								m_SerialRecvDataList;			// �V���A���ʐM��M�f�[�^���X�g
	CMutex										m_cSerialRecvDataListMutex;		// �V���A���ʐM��M�f�[�^���X�g�p�~���[�e�b�N�X
	CEvent										m_cSerialRecvDataListEvent;		// �V���A���ʐM��M�f�[�^���X�g�p�C�x���g



public:
	CSerialRecvThread(CLASS_PARAM_TABLE& tClassParam);
	~CSerialRecvThread();


	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	
private:	
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);

};







