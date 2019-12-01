//*****************************************************************************
// EventExクラス
//*****************************************************************************
#include "CEventEx.h"
#include <sys/eventfd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#define _CEVENT_EX_DEBUG_
#define MONITORING_COUNTER_NUM				( 20 )

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CEventEx::CEventEx()
{
	m_ErrorNo = 0;
	m_efd = -1;
	m_Counter = 0;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CEventEx::~CEventEx()
{
	// イベントファイルディスクリプタを解放
	if (m_efd != -1)
	{
		close(m_efd);
		m_efd = -1;
	}
}


//-----------------------------------------------------------------------------
// 初期処理
//-----------------------------------------------------------------------------
CEventEx::RESULT_ENUM CEventEx::Init()
{
	// イベントファイルディスクリプタを生成
	m_efd = eventfd(0, 0);
	if (m_efd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
		perror("CEventEx::Init - eventfd");
#endif	// #ifdef _CEVENT_EX_DEBUG_
		return RESULT_ERROR_EVENT_FD;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// イベントファイルディスクリプタを取得
//-----------------------------------------------------------------------------
int CEventEx::GetEventFd()
{
	return m_efd;
}


//-----------------------------------------------------------------------------
// エラー番号を取得
//-----------------------------------------------------------------------------
int CEventEx::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// イベント設定
//-----------------------------------------------------------------------------
CEventEx::RESULT_ENUM CEventEx::SetEvent()
{
	uint64_t				event = 1;
	int						iRet = 0;


	// イベントファイルディスクリプタのチェック
	if (m_efd == -1)
	{
		return RESULT_ERROR_EVENT_FD;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cCounterMutex.Lock();

	// イベント設定（インクリメント）
	iRet = write(m_efd, &event, sizeof(event));
	if (iRet != sizeof(event))
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
		perror("CEventEx::SetEvent - write");
#endif	// #ifdef _CEVENT_EX_DEBUG_

		m_cCounterMutex.Unlock();
		return RESULT_ERROR_EVENT_SET;
	}

	// カウンター値をインクリメント
	m_Counter++;

	// この条件に入った場合は処理が溜まっているということなので、処理を検討してください
	if (m_Counter == MONITORING_COUNTER_NUM)
	{
#ifdef _CEVENT_EX_DEBUG_
		printf("CEventEx::SetEvent - <Warning> Processing has accumulated!\n");
#endif	// #ifdef _CEVENT_EX_DEBUG_
	}

	m_cCounterMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// イベントクリア
//-----------------------------------------------------------------------------
CEventEx::RESULT_ENUM CEventEx::ClearEvent()
{
	uint64_t				event = 0;
	int						iRet = 0;
	fd_set					readfds;
	timeval					tTimeval;


	// イベントファイルディスクリプタのチェック
	if (m_efd == -1)
	{
		return RESULT_ERROR_EVENT_FD;
	}

	tTimeval.tv_sec = 0;
	tTimeval.tv_usec = 0;


	// イベントリセットが行えれるかのチェック
	FD_ZERO(&readfds);
	FD_SET(m_efd, &readfds);
	iRet = select(m_efd + 1, &readfds, NULL, NULL, &tTimeval);
	if (iRet < 0)
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
		perror("CEventEx::ClearEvent - select");
#endif	// #ifdef _CEVENT_EX_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}
	if (iRet == 0)
	{
		// 何もしない（正常終了）
		// ※既にリセットされているときにResetEventを行うと、このルートを通る
	}
	else
	{
		// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
		m_cCounterMutex.Lock();

		// 一応馬鹿除け
		if (m_Counter != 0)
		{
			// カウンター値が0になったときに、イベントクリアを行う
			m_Counter--;
			if (m_Counter == 0)
			{
				// イベントクリア
				iRet = read(m_efd, &event, sizeof(event));
				if (iRet < 0)
				{
					m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
					perror("CEventEx::ClearEvent - read");
#endif	// #ifdef _CEVENT_EX_DEBUG_

					m_cCounterMutex.Unlock();
					return RESULT_ERROR_EVENT_CLEAR;
				}
			}
		}

		m_cCounterMutex.Unlock();
		// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// イベント待ち
//-----------------------------------------------------------------------------
CEventEx::RESULT_ENUM CEventEx::Wait(unsigned int Timeout)
{
	fd_set			fdRead;
	int				iRet = 0;
	timeval			timeout;


	// イベントファイルディスクリプタのチェック
	if (m_efd == -1)
	{
		return RESULT_ERROR_EVENT_FD;
	}

	// ウエイト処理
	FD_ZERO(&fdRead);
	FD_SET(m_efd, &fdRead);
	if (Timeout == 0)
	{
		iRet = select(m_efd + 1, &fdRead, NULL, NULL, NULL);
	}
	else
	{
		timeout.tv_sec = Timeout / 1000;
		timeout.tv_usec = (Timeout % 1000) * 1000;
		iRet = select(m_efd + 1, &fdRead, NULL, NULL, &timeout);
	}

	if (iRet < 0)					// エラー
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
		perror("CEventEx::Wait - select");
#endif	// #ifdef _CEVENT_EX_DEBUG_
		return RESULT_ERROR_EVENT_WAIT;
	}
	else if (iRet == 0)				// タイムアウト
	{
		return RESULT_WAIT_TIMEOUT;
	}

	return RESULT_RECIVE_EVENT;
}
