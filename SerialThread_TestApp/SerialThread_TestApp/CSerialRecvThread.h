#pragma once
//*****************************************************************************
// CSerialRecvThreadクラス
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
	// シリアル受信スレッドクラスのパラメータ
	typedef struct
	{
		CLog*									pcLog;							// CLogクラス
		int										SerialFd;						// シリアル通信用ファイルディスクリプタ

	} CLASS_PARAM_TABLE;


private:
	bool										m_bInitFlag;					// 初期化完了フラグ
	int											m_ErrorNo;						// エラー番号
	int											m_epfd;							// epollファイルディスクリプタ

	CLASS_PARAM_TABLE							m_tClassParam;					// シリアル受信スレッドクラスのパラメータ
	std::list<int>								m_SerialRecvDataList;			// シリアル通信受信データリスト
	CMutex										m_cSerialRecvDataListMutex;		// シリアル通信受信データリスト用ミューテックス
	CEvent										m_cSerialRecvDataListEvent;		// シリアル通信受信データリスト用イベント



public:
	CSerialRecvThread(CLASS_PARAM_TABLE& tClassParam);
	virtual ~CSerialRecvThread();


	RESULT_HEADER_ENUM Start();
	RESULT_HEADER_ENUM Stop();
	
private:	
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);

};







