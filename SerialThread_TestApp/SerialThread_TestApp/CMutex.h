#pragma once
//*****************************************************************************
// Mutexクラス（スレッド間の排他のみ使用可能）
// ※リンカオプションに「-pthread」を追加すること
//*****************************************************************************
#include <cstdio>
#include <pthread.h>
#include <errno.h> 


class CMutex
{
private:
	bool								m_bInitFlag;			// 初期化フラグ
	int									m_ErrorNo;				// エラー番号

	pthread_mutexattr_t					m_tMutexAttr;			// ミューテックス属性
	pthread_mutex_t						m_hMutex;				// ミューテックスハンドル


public:
	CMutex(int Kind = PTHREAD_PROCESS_PRIVATE);
	~CMutex();
//	bool SetType(int Kind = PTHREAD_PROCESS_PRIVATE);
//	bool GetType(int* pKind);
	bool Lock();
	bool Unlock();
	bool TryLock();
	int GetErrorNo();
};