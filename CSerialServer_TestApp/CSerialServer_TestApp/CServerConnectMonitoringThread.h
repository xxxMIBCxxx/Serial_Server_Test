#pragma once
//*****************************************************************************
// �ڑ��Ď��X���b�h�i�T�[�o�[�Łj�N���X
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "list"
#include "CServerResponseThread.h"


class CServerConnectMonitoringThread : CThread
{
public:
	// �ڑ��Ď��X���b�h�i�T�[�o�[�Łj�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// ����I��
		RESULT_ERROR_INIT = 0xE00000001,										// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,										// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_PARAM = 0xE1000001,										// �p�����[�^�G���[
		RESULT_ERROR_CREATE_SOCKET = 0xE1000002,								// �\�P�b�g�����Ɏ��s
		RESULT_ERROR_BIND = 0xE1000003,											// �\�P�b�g�̖��O�t���Ɏ��s
		RESULT_ERROR_LISTEN = 0xE1000004,										// �ڑ��҂��Ɏ��s

		RESULT_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[
	} RESULT_ENUM;


	// �T�[�o�[���\����
	typedef struct
	{
		int									Socket;								// �\�P�b�g
		struct sockaddr_in					tAddr;								// �C���^�[�l�b�g�\�P�b�g�A�h���X�\����
	} SERVER_INFO_TABLE;

private:
	bool									m_bInitFlag;						// �����������t���O
	int										m_ErrorNo;							// �G���[�ԍ�
	int										m_epfd;								// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�ڑ��Ď��X���b�h�Ŏg�p�j

	SERVER_INFO_TABLE						m_tServerInfo;						// �T�[�o�[���

	CEvent									m_cServerResponseThreadEndEvent;	// �T�[�o�[�����X���b�h�I���C�x���g
	std::list<CServerResponseThread*>		m_ServerResponseThreadList;			// �T�[�o�[�����X���b�h�̃��X�g
	CMutex									m_cServerResponceThreadListMutex;	// �T�[�o�[�����X���b�h�̃��X�g�p�~���[�e�b�N�X

public:
	CServerConnectMonitoringThread();
	~CServerConnectMonitoringThread();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();

private:
	RESULT_ENUM ServerConnectInit(SERVER_INFO_TABLE& tServerInfo);
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void ServerResponseThreadList_Clear();
	void ServerResponseThreadList_CheckEndThread();

};