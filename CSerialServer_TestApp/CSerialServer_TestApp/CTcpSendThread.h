#pragma once
//*****************************************************************************
// TCP通信送信スレッドクラス
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CEvent.h"
#include "CMutex.h"
#include "list"


class CTcpSendThread : CThread
{
public:
	// TCP通信送信スレッドクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,										// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,									// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,							// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,									// スレッド開始に失敗しました

		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,								// スレッドが動作していない（または終了している）
		RESULT_ERROR_PARAM = 0xE1000002,									// パラメータエラー
		RESULT_ERROR_SYSTEM = 0xE9999999,									// システムエラー
	} RESULT_ENUM;

	// クライアント情報構造体
	typedef struct
	{
		int									Socket;							// ソケット
		struct sockaddr_in					tAddr;							// インターネットソケットアドレス構造体

	} CLIENT_INFO_TABLE;

	// TCP送信要求構造体 
	typedef struct
	{
		void*								pSenderClass;					// TCP送信元クラス
		ssize_t								SendDataSize;					// TCP送信データサイズ
		char*								pSendData;						// TCP送信データ（※送信する側でmallocを使用して領域を確保指定ください。解放は送信データが不要となったタイミングで本スレッド側で解放します）
	} SEND_REQUEST_TABLE;


	CEvent									m_cSendRequestEvent;			// TCP送信要求イベント
	CMutex									m_cSendRequestListMutex;		// TCP送信要求リスト用ミューテックス
	std::list<SEND_REQUEST_TABLE>			m_SendRequestList;				// TCP送信要求リスト


private:
	bool									m_bInitFlag;					// 初期化完了フラグ
	int										m_ErrorNo;						// エラー番号
	int										m_epfd;							// epollファイルディスクリプタ（クライアント接続監視スレッドで使用）
	CLIENT_INFO_TABLE						m_tClientInfo;					// クライアント情報

public:
	CTcpSendThread(CLIENT_INFO_TABLE& tClientInfo);
	~CTcpSendThread();
	int GetErrorNo();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM SetSendRequestData(SEND_REQUEST_TABLE& tSendReauest);
	RESULT_ENUM GetSendRequestData(SEND_REQUEST_TABLE& tSendReauest);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void SendRequestList_Clear();
};







