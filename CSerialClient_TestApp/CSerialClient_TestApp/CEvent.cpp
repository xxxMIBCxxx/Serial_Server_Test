//*****************************************************************************
// Eventクラス
//*****************************************************************************
#include "CEvent.h"
#include <sys/eventfd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#define _CEVENT_DEBUG_


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CEvent::CEvent()
{
	m_ErrorNo = 0;
	m_efd = -1;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CEvent::~CEvent()
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
CEvent::RESULT_ENUM CEvent::Init(unsigned int initval)
{
	// イベントファイルディスクリプタを生成
	m_efd = eventfd(initval, 0);
	if (m_efd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_DEBUG_
		perror("CEvent::Init - eventfd");
#endif	// #ifdef _CEVENT_DEBUG_
		return RESULT_ERROR_EVENT_FD;
	}
	else
	{
#ifdef _CEVENT_DEBUG_
		printf("CEvent::Init - m_efd : %d\n", m_efd);
#endif	// #ifdef _CEVENT_DEBUG_
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// イベントファイルディスクリプタを取得
//-----------------------------------------------------------------------------
int CEvent::GetEventFd()
{
	return m_efd;
}


//-----------------------------------------------------------------------------
// エラー番号を取得
//-----------------------------------------------------------------------------
int CEvent::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// イベント設定
//-----------------------------------------------------------------------------
CEvent::RESULT_ENUM CEvent::SetEvent()
{
	uint64_t				event = 1;
	int						iRet = 0;


	// イベントファイルディスクリプタのチェック
	if (m_efd == -1)
	{
		return RESULT_ERROR_EVENT_FD;
	}

	// イベント設定
	iRet = write(m_efd, &event, sizeof(event));
	if (iRet != sizeof(event))
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_DEBUG_
		perror("CEvent::SetEvent - write");
#endif	// #ifdef _CEVENT_DEBUG_
		return RESULT_ERROR_EVENT_SET;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// イベントリセット
//-----------------------------------------------------------------------------
CEvent::RESULT_ENUM CEvent::ClearEvent()
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
#ifdef _CEVENT_DEBUG_
		perror("CEvent::ClearEvent - select");
#endif	// #ifdef _CEVENT_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}
	if (iRet == 0)
	{
		// 何もしない（正常終了）
		// ※既にリセットされているときにResetEventを行うと、このルートを通る
	}
	else
	{
		// イベントリセット
		iRet = read(m_efd, &event, sizeof(event));
		if (iRet < 0)
		{
			m_ErrorNo = errno;
#ifdef _CEVENT_DEBUG_
			perror("CEvent::ClearEvent - read");
#endif	// #ifdef _CEVENT_DEBUG_
			return RESULT_ERROR_EVENT_CLEAR;
		}
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// イベント待ち
//-----------------------------------------------------------------------------
CEvent::RESULT_ENUM CEvent::Wait(unsigned int Timeout)
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
#ifdef _CEVENT_DEBUG_
		perror("CEvent::Wait - select");
#endif	// #ifdef _CEVENT_DEBUG_
		return RESULT_ERROR_EVENT_WAIT;
	}
	else if (iRet == 0)				// タイムアウト
	{
		return RESULT_WAIT_TIMEOUT;
	}

	return RESULT_RECIVE_EVENT;
}
