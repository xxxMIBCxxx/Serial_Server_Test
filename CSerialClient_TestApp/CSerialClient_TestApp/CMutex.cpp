//*****************************************************************************
// Mutexクラス（スレッド間の排他のみ使用可能）
// ※リンカオプションに「-pthread」を追加すること
//*****************************************************************************
#include "CMutex.h"


#define _CMUTEX_DEBUG_


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CMutex::CMutex(int Kind)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;

	int				iRet = 0;


	// ミューテックス属性オブジェクトの初期化
	iRet = pthread_mutexattr_init(&m_tMutexAttr);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::init - pthread_mutexattr_init");
#endif	// #ifdef _CMUTEX_DEBUG_
		return;
	}

	// ミューテックス属性オブジェクトのタイプの設定
	iRet = pthread_mutexattr_settype(&m_tMutexAttr, Kind);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::init - pthread_mutexattr_settype");
#endif	// #ifdef _CMUTEX_DEBUG_
		return;
	}

	// ミューテックスオブジェクトの初期化
	iRet = pthread_mutex_init(&m_hMutex, &m_tMutexAttr);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::init - pthread_mutex_init");
#endif	// #ifdef _CMUTEX_DEBUG_
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CMutex::~CMutex()
{
	// 初期化完了している場合
	if (m_bInitFlag == true)
	{
		// ミューテックスオブジェクトの解放
		pthread_mutex_destroy(&m_hMutex);
	}
}


////-----------------------------------------------------------------------------
//// 属性オブジェクトのタイプの設定
////-----------------------------------------------------------------------------
//bool CMutex::SetType(int Kind)
//{
//	int				iRet = 0;
//
//
//	// 属性オブジェクトのタイプの設定
//	iRet = pthread_mutexattr_settype(&m_tMutexAttr, Kind);
//	if (iRet == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CMUTEX_DEBUG_
//		perror("CMutex::SetType - pthread_mutexattr_settype");
//#endif	// #ifdef _CMUTEX_DEBUG_
//		return false;
//	}
//
//	return true;
//}
//
//
////-----------------------------------------------------------------------------
//// 属性オブジェクトのタイプの取得
////-----------------------------------------------------------------------------
//bool CMutex::GetType(int* pKind)
//{
//	int				iRet = 0;
//
//
//	// 引数チェック
//	if (pKind == NULL)
//	{
//		return false;
//	}
//
//	// 属性オブジェクトのタイプの設定
//	iRet = pthread_mutexattr_gettype(&m_tMutexAttr, pKind);
//	if (iRet == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CMUTEX_DEBUG_
//		perror("CMutex::GetType - pthread_mutexattr_gettype");
//#endif	// #ifdef _CMUTEX_DEBUG_
//		return false;
//	}
//
//	return true;
//}


//-----------------------------------------------------------------------------
// 排他開始
//-----------------------------------------------------------------------------
bool CMutex::Lock()
{
	int					iRet = 0;


	// 排他開始
	iRet = pthread_mutex_lock(&m_hMutex);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::Lock - pthread_mutex_lock");
#endif	// #ifdef _CMUTEX_DEBUG_
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// 排他終了
//-----------------------------------------------------------------------------
bool CMutex::Unlock()
{
	int					iRet = 0;


	// 排他開始
	iRet = pthread_mutex_unlock(&m_hMutex);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::Unlock - pthread_mutex_unlock");
#endif	// #ifdef _CMUTEX_DEBUG_
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// 排他再試行
//-----------------------------------------------------------------------------
bool CMutex::TryLock()
{
	int					iRet = 0;


	// 排他開始
	iRet = pthread_mutex_trylock(&m_hMutex);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::TryLock - pthread_mutex_trylock");
#endif	// #ifdef _CMUTEX_DEBUG_
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// エラー番号取得
//-----------------------------------------------------------------------------
int CMutex::GetErrorNo()
{
	return m_ErrorNo;
}