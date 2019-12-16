//*****************************************************************************
// LOGクラス
//*****************************************************************************
#include "CLog.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>


#define _CLOG_DEBUG_

#define DEFAULT_LOG_FILE_SIZE			( 1024 * 1024 * 4 )							// デフォルトログファイルサイズ(4MB)
#define MIN_LOG_FILE_SIZE				( 1024 * 1 )								// 最小ログファイルサイズ(1MB)
#define MAX_LOG_FILE_SIZE				( 1024 * 1024 * 10 )						// 最大ログファイルサイズ(10MB)
#define	DEFAULT_LOG_OUTPUT				( CLog::LOG_OUTPUT_ERROR )					// デフォルトログ出力ビット
#define LOG_OUTPUT_COUNT				( 25 )										// １回の処理に出力できるログ数

#define EPOLL_MAX_EVENTS				( 10 )										// epoll最大イベント
#define EPOLL_TIMEOUT_TIME				( 1000 )									// epollタイムアウト(ms) ※1秒毎にログ書込みを行います※


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CLog::CLog()
{
	char				szProcessName[256 + 1];


	m_bStartFlag = false;
	m_epfd = -1;
	m_ErrorNo = 0;

	// ログ設定情報をデフォルト値で初期化
	memset(&m_tLogSettingInfo, 0x00, sizeof(m_tLogSettingInfo));
	m_tLogSettingInfo.LogOutputBit = DEFAULT_LOG_OUTPUT;
	m_tLogSettingInfo.LogFileSize = DEFAULT_LOG_FILE_SIZE;
	memset(szProcessName, 0x00, sizeof(szProcessName));
	sprintf(m_tLogSettingInfo.szFileName, "/var/tmp/%s.log", GetProcessName(szProcessName, 256));			// ログ出力先

	// ログ出力リストをクリア
	m_LogOutputList.clear();

	memset(m_szLogBuff, 0x00, sizeof(m_szLogBuff));
	memset(m_szSettingFile, 0x00, sizeof(m_szSettingFile));

	// ログ出力スレッド開始
	Start();
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CLog::~CLog()
{
	// ログ出力スレッドを停止させる
	CThread::Stop();
}


//-----------------------------------------------------------------------------
// ログ開始
//-----------------------------------------------------------------------------
bool CLog::Start()
{
	FILE*					pFile = NULL;
	CThread::RESULT_ENUM	eThreadRet = CThread::RESULT_SUCCESS;


	// 既にログ開始している場合
	if (m_bStartFlag == true)
	{
		return true;
	}

	// ファイルの存在有無を確認するため、一時的にオープンする
	pFile = fopen(m_tLogSettingInfo.szFileName, "r+");
	if (errno == ENOENT)
	{
		pFile = fopen(m_tLogSettingInfo.szFileName, "w+");
	}

	if (pFile == NULL)
	{
#ifdef _CLOG_DEBUG_
		perror("CLog::Start - fopen");
#endif	// #ifdef _CLOG_DEBUG_
		return false;
	}
	fclose(pFile);

	// ログ書き込み用スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		return false;
	}

	m_bStartFlag = true;

	return true;
}



//-----------------------------------------------------------------------------
// ログ出力
//-----------------------------------------------------------------------------
void CLog::Output(LOG_OUTPUT_ENUM eLog, const char* format, ...)
{
	struct timespec		tTimeSpec;
	struct tm			tTm;
	char				szTime[64 + 1];
	unsigned int		Length;


	// ログ開始していない場合
	if (m_bStartFlag == false)
	{
		return;
	}

	// ログ出力判定
	if (!(m_tLogSettingInfo.LogOutputBit & (unsigned int)eLog))
	{
		return;
	}

	// 現在時刻を取得
	memset(szTime, 0x00, sizeof(szTime));
	clock_gettime(CLOCK_REALTIME, &tTimeSpec);
	localtime_r(&tTimeSpec.tv_sec, &tTm);
	sprintf(szTime, "[%04d/%02d/%02d %02d:%02d:%02d.%03ld] ",	
		tTm.tm_year + 1900,
		tTm.tm_mon + 1,
		tTm.tm_mday,
		tTm.tm_hour,
		tTm.tm_min,
		tTm.tm_sec,
		tTimeSpec.tv_nsec / 1000000);

	// 可変引数を展開
	memset(m_szLogBuff, 0x00, sizeof(m_szLogBuff));
	va_list			ap;
	va_start(ap, format);
	vsprintf(m_szLogBuff, format, ap);
	va_end(ap);

	// ログ出力リストに登録
	LOG_OUTPUT_INFO_TABLE		tLogOutput;
	tLogOutput.Length = strlen(szTime) + strlen(m_szLogBuff) + 1 + 1;			// CR + NULL
	tLogOutput.pszLog = (char*)malloc(tLogOutput.Length);
	if (tLogOutput.pszLog != NULL)
	{
		memset(tLogOutput.pszLog, 0x00, tLogOutput.Length);
		Length = strlen(szTime);
		strncpy(tLogOutput.pszLog, szTime, Length);
		strcat(&tLogOutput.pszLog[Length], m_szLogBuff);
		Length += strlen(m_szLogBuff);
		strcat(&tLogOutput.pszLog[Length], "\n");

		// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽
		m_cLogOutputListMutex.Lock();

		m_LogOutputList.push_back(tLogOutput);

		m_cLogOutputListMutex.Unlock();
		// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△
	}
}


//-----------------------------------------------------------------------------
// ログ出力スレッド
//-----------------------------------------------------------------------------
void CLog::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;


	// スレッドが終了する際に呼ばれる関数を登録
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epollファイルディスクリプタ生成
	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
	if (m_epfd == -1)
	{
		m_ErrorNo = errno;
		return;
	}

	// スレッド終了要求イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->GetThreadEndReqEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->GetThreadEndReqEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
		return;
	}

	// スレッド開始イベントを送信
	this->m_cThreadStartEvent.SetEvent();

	// ▼--------------------------------------------------------------------------▼
	// スレッド終了要求が来るまでループ
	// ※勝手にループを抜けるとスレッド終了時にタイムアウトで終了となるため、スレッド終了要求以外は勝手にループを抜けないでください
	while (bLoop)
	{
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT_TIME);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
			continue;
		}
		// タイムアウト（ログ書き込み）
		else if (nfds == 0)
		{

			// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽
			m_cLogOutputListMutex.Lock();

			// ログ出力
			LogWrite(false);

			m_cLogOutputListMutex.Unlock();
			// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△

			continue;
		}

		for (int i = 0; i < nfds; i++)
		{
			// スレッド終了要求イベント受信
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
		}
	}
	// ▲--------------------------------------------------------------------------▲

	pthread_cleanup_pop(1);

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();
}



//-----------------------------------------------------------------------------
// クライアント応答スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CLog::ThreadProcCleanup(void* pArg)
{
	CLog* pcLog = (CLog*)pArg;

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽
	pcLog->m_cLogOutputListMutex.Lock();

	// ログ出力
	pcLog->LogWrite(true);

	pcLog->m_cLogOutputListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△

	// epollファイルディスクリプタ解放
	if (pcLog->m_epfd != -1)
	{
		close(pcLog->m_epfd);
		pcLog->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// ログ出力処理
//-----------------------------------------------------------------------------
void CLog::LogWrite(bool bEnd)
{
	char					szBuff[16];
	unsigned int			Pos = 0;
	unsigned int			Count = 0;
	FILE*					pFile = NULL;


	// ログ出力データがなければ処理を行わない
	if (m_LogOutputList.size() == 0)
	{
		return;
	}

	// ファイルをオープンする
	pFile = fopen(m_tLogSettingInfo.szFileName, "r+");
	if (errno == ENOENT)
	{
		pFile = fopen(m_tLogSettingInfo.szFileName, "w+");
	}
	if (pFile == NULL)
	{
#ifdef _CLOG_DEBUG_
		perror("CLog::LogWrite - fopen");
#endif	// #ifdef _CLOG_DEBUG_
		return;
	}

	// ログ出力データがなくなるまでループ
	while (m_LogOutputList.size() != 0)
	{
		// ログ出力の先頭データを取得
		std::list<LOG_OUTPUT_INFO_TABLE>::iterator			it = m_LogOutputList.begin();

		// ファイル書込み位置を取得
		memset(szBuff, 0x00, sizeof(szBuff));
		fseek(pFile, 0, SEEK_SET);
		fread(szBuff, 8, 1, pFile);
		Pos = atol(szBuff);
		if (Pos == 0)
		{
			sprintf(szBuff, "%08d\n", Pos);
			fseek(pFile, 0, SEEK_SET);
			fwrite(szBuff, 9, 1, pFile);
			Pos = 9;
		}


		// ログを書込む際、ファイル最大サイズを超えているか調べる
		if ((Pos + it->Length) > m_tLogSettingInfo.LogFileSize)
		{
			Pos = 9;
		}

		// ログ書込み
		fseek(pFile, Pos, SEEK_SET);
		fwrite(it->pszLog, (it->Length - 1), 1, pFile);
		Pos += (it->Length - 1);

		// ファイル位置書込み
		memset(szBuff, 0x00, sizeof(szBuff));
		sprintf(szBuff, "%08d\n", Pos);
		fseek(pFile, 0, SEEK_SET);
		fwrite(szBuff, 9, 1, pFile);

		// ログ出力リストの先頭ログ情報を削除
		if (it->pszLog != NULL)
		{
			free(it->pszLog);
		}
		m_LogOutputList.pop_front();

		// 終了処理から呼ばれていない場合
		if (bEnd == false)
		{
			Count++;

			if (Count >= LOG_OUTPUT_COUNT)
			{
				break;
			}
		}
	}

	// ファイルを閉じる
	fclose(pFile);
}


//-----------------------------------------------------------------------------
// プロセス名称を取得
//-----------------------------------------------------------------------------
char* CLog::GetProcessName(char* pszProcessName, unsigned int BuffSize)
{
	char*			pPos = NULL;


	// 引数チェック
	if ((pszProcessName == NULL) || (BuffSize == 0))
	{
		return NULL;
	}

	// プロセス名称をコピー
	strncpy(pszProcessName, program_invocation_short_name, BuffSize);

	// 拡張子がある場合は、拡張子を削除
	pPos = strchr(pszProcessName, '.');
	if (pPos != NULL)
	{
		*pPos = '\0';
	}

	return pszProcessName;
}

	