#pragma once
//*****************************************************************************
// TCP通信受信スレッドクラス
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CEvent.h"
#include "CEventEx.h"
#include "CMutex.h"
#include "list"


#define CSERVER_TCP_RECV_THREAD_RECV_BUFF_SIZE				( 100 )
#define CSERVER_TCP_RECV_THREAD_COMMAND_BUFF_SIZE			( 1000 )
#define	IP_ADDR_BUFF_SIZE									( 32 )


class CServerTcpRecvThread : public CThread
{
public:
	// TCP通信受信スレッドクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,										// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,										// スレッド開始に失敗しました

		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,									// スレッドが動作していない（または終了している）
		RESULT_ERROR_PARAM = 0xE1000002,										// パラメータエラー
		RESULT_ERROR_RECV = 0xE1000003,											// TCP受信エラー
		RESULT_ERROR_COMMAND_BUFF_OVER = 0xE1000004,							// コマンドバッファオーバー
		RESULT_ERROR_SYSTEM = 0xE9999999,										// システムエラー
	} RESULT_ENUM;


	// クライアント情報構造体
	typedef struct
	{
		int									Socket;								// ソケット
		struct sockaddr_in					tAddr;								// インターネットソケットアドレス構造体

	} CLIENT_INFO_TABLE;

	// TCP受信情報構造体 
	typedef struct
	{
		void*								pReceverClass;						// 受信先クラス
		ssize_t								DataSize;							// 受信データサイズ
		char*								pData;								// 受信データ（※受信先にてデータが不要となったら、freeを使用して領域を解放してください）
	} RECV_INFO_TABLE;

	// 解析種別
	typedef enum
	{
		ANALYZE_KIND_STX = 0,													// STX待ち
		ANALYZE_KIND_ETX = 1,													// ETX待ち
	} ANALYZE_KIND_ENUM;


	CEvent									m_cClientDisconnectEvent;			// クライアント切断イベント

	CEventEx								m_cRecvInfoEvent;					// TCP受信情報イベント
	CMutex									m_cRecvInfoListMutex;				// TCP受信情報リスト用ミューテックス
	std::list<RECV_INFO_TABLE>				m_RecvInfoList;						// TCP受信情報リスト


	ANALYZE_KIND_ENUM						m_eAnalyzeKind;													// 解析種別
	char									m_szRecvBuff[CSERVER_TCP_RECV_THREAD_RECV_BUFF_SIZE + 1];		// 受信バッファ
	char									m_szCommandBuff[CSERVER_TCP_RECV_THREAD_COMMAND_BUFF_SIZE + 1];	// 受信コマンドバッファ
	ssize_t									m_CommandPos;													// 受信コマンド格納位置

private:
	bool									m_bInitFlag;						// 初期化完了フラグ
	int										m_ErrorNo;							// エラー番号
	int										m_epfd;								// epollファイルディスクリプタ
	
	CLIENT_INFO_TABLE						m_tClientInfo;						// クライアント情報
	char									m_szIpAddr[IP_ADDR_BUFF_SIZE + 1];	// IPアドレス
	uint16_t								m_Port;								// ポート番号

public:
	CServerTcpRecvThread(CLIENT_INFO_TABLE& tClientInfo);
	~CServerTcpRecvThread();
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


