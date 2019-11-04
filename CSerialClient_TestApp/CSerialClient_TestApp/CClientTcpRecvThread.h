#pragma once
//*****************************************************************************
// TCP�ʐM��M�X���b�h�N���X
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CEvent.h"
#include "CEventEx.h"
#include "CMutex.h"
#include "list"


#define CCLIENT_TCP_RECV_THREAD_RECV_BUFF_SIZE				( 100 )
#define CCLIENT_TCP_RECV_THREAD_COMMAND_BUFF_SIZE			( 1000 )
#define	IP_ADDR_BUFF_SIZE									( 32 )


class CClientTcpRecvThread : public CThread
{
public:
	// TCP�ʐM��M�X���b�h�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// ����I��
		RESULT_ERROR_INIT = 0xE00000001,										// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,										// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,									// �X���b�h�����삵�Ă��Ȃ��i�܂��͏I�����Ă���j
		RESULT_ERROR_PARAM = 0xE1000002,										// �p�����[�^�G���[
		RESULT_ERROR_RECV = 0xE1000003,											// TCP��M�G���[
		RESULT_ERROR_COMMAND_BUFF_OVER = 0xE1000004,							// �R�}���h�o�b�t�@�I�[�o�[
		RESULT_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[
	} RESULT_ENUM;


	// �T�[�o�[���\����
	typedef struct
	{
		int									Socket;								// �\�P�b�g
		struct sockaddr_in					tAddr;								// �C���^�[�l�b�g�\�P�b�g�A�h���X�\����

	} SERVER_INFO_TABLE;

	// TCP��M���\���� 
	typedef struct
	{
		void*								pReceverClass;						// ��M��N���X
		ssize_t								DataSize;							// ��M�f�[�^�T�C�Y
		char*								pData;								// ��M�f�[�^�i����M��ɂăf�[�^���s�v�ƂȂ�����Afree���g�p���ė̈��������Ă��������j
	} RECV_INFO_TABLE;

	// ��͎��
	typedef enum
	{
		ANALYZE_KIND_STX = 0,													// STX�҂�
		ANALYZE_KIND_ETX = 1,													// ETX�҂�
	} ANALYZE_KIND_ENUM;



	CEventEx								m_cRecvInfoEvent;					// TCP��M���C�x���g
	CMutex									m_cRecvInfoListMutex;				// TCP��M��񃊃X�g�p�~���[�e�b�N�X
	std::list<RECV_INFO_TABLE>				m_RecvInfoList;						// TCP��M��񃊃X�g


	ANALYZE_KIND_ENUM						m_eAnalyzeKind;													// ��͎��
	char									m_szRecvBuff[CCLIENT_TCP_RECV_THREAD_RECV_BUFF_SIZE + 1];		// ��M�o�b�t�@
	char									m_szCommandBuff[CCLIENT_TCP_RECV_THREAD_COMMAND_BUFF_SIZE + 1];	// ��M�R�}���h�o�b�t�@
	ssize_t									m_CommandPos;													// ��M�R�}���h�i�[�ʒu

private:
	bool									m_bInitFlag;						// �����������t���O
	int										m_ErrorNo;							// �G���[�ԍ�
	int										m_epfd;								// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�ڑ��Ď��X���b�h�Ŏg�p�j
	
	CEvent*									m_pcServerDisconnectEvent;			// �ؒf�C�x���g
	SERVER_INFO_TABLE						m_tServerInfo;						// �T�[�o�[���
	char									m_szIpAddr[IP_ADDR_BUFF_SIZE + 1];	// IP�A�h���X
	uint16_t								m_Port;								// �|�[�g�ԍ�

public:
	CClientTcpRecvThread(SERVER_INFO_TABLE& tServerInfo, CEvent* pcServerDisconnectEvent);
	~CClientTcpRecvThread();
	int GetErrorNo();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM GetRecvData(RECV_INFO_TABLE& tRecvInfo);
	RESULT_ENUM SetRecvData(RECV_INFO_TABLE& tRecvInfo);


private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void RecvInfoList_Clear();
	RESULT_ENUM TcpRecvProc();
	RESULT_ENUM TcpRecvDataAnalyze(char* pRecvData, ssize_t RecvDataNum);
};

