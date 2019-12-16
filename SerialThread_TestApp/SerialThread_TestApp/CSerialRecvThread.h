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
#include "ResultHeader.h"


class CSerialRecvThread : public CThread
{
public:
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
	virtual ~CSerialRecvThread();


	RESULT_HEADER_ENUM Start();
	RESULT_HEADER_ENUM Stop();
	
private:	
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);

};







