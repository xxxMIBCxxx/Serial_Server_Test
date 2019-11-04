#pragma once
//*****************************************************************************
// クライアント応答スレッド
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CClientTcpRecvThread.h"


class CClientResponseThread : public CThread
{
public:
	// クライアント応答スレッドクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,										// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,										// スレッド開始に失敗しました

		RESULT_ERROR_PARAM = 0xE1000001,										// パラメータエラー
		RESULT_ERROR_CREATE_SOCKET = 0xE1000002,								// ソケット生成に失敗
		RESULT_ERROR_CONNECT = 0xE1000003,										// 接続に失敗
		RESULT_ERROR_LISTEN = 0xE1000004,										// 接続待ちに失敗

		RESULT_ERROR_SYSTEM = 0xE9999999,										// システムエラー
	} RESULT_ENUM;


	// サーバー情報構造体
	typedef struct
	{
		int									Socket;								// ソケット
		struct sockaddr_in					tAddr;								// インターネットソケットアドレス構造体
	} SERVER_INFO_TABLE;


	CClientTcpRecvThread*					m_pcClientTcpRecvThread;			// TCP受信スレッド
	CEvent*									m_pcServerDisconnectEvent;			// サーバー切断イベント

private:
	bool									m_bInitFlag;						// 初期化完了フラグ
	int										m_ErrorNo;							// エラー番号

	SERVER_INFO_TABLE						m_tServerInfo;						// サーバー情報
	char									m_szIpAddr[IP_ADDR_BUFF_SIZE + 1];	// IPアドレス
	uint16_t								m_Port;								// ポート番号
	int										m_epfd;								// epollファイルディスクリプタ（クライアント応答スレッドで使用）

public:
	CClientResponseThread(CEvent* pcServerDisconnectEvent);
	~CClientResponseThread();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();

	RESULT_ENUM ServerConnect(SERVER_INFO_TABLE& tServerInfo);
	void ServerDisconnect(SERVER_INFO_TABLE& tServerInfo);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	RESULT_ENUM CreateTcpThread(SERVER_INFO_TABLE& tServerInfo);
	void DeleteTcpThread();
};

