#pragma once
//*****************************************************************************
// LOGクラス
//*****************************************************************************
#include "CMutex.h"
#include "CEvent.h"
#include "CThread.h"
#include "list"


#define	CLOG_MAX_SIZE					( 1024 )				// ログファイル１行の最大文字数
#define CLOG_FILE_NAME_SIZE				( 256 )					// ログファイル名のサイズ

class CLog : public CThread
{
public:

	// ログ種別(bit)
	typedef enum
	{
		LOG_OUTPUT_ERROR		= 0x80000000,				// エラーログ
		LOG_OUTPUT_WARNING		= 0x40000000,				// 警告ログ
		LOG_OUTPUT_TRACE		= 0x20000000,				// トレースログ
		LOG_OUTPUT_DEBUG		= 0x10000000,				// デバッグログ
		LOG_OUTPUT_RESERVED_01	= 0x08000000,				// 予約01
		LOG_OUTPUT_RESERVED_02	= 0x04000000,				// 予約02
		LOG_OUTPUT_RESERVED_03	= 0x02000000,				// 予約03
		LOG_OUTPUT_RESERVED_04	= 0x01000000,				// 予約04
		LOG_OUTPUT_RESERVED_05	= 0x00800000,				// 予約05
		LOG_OUTPUT_RESERVED_06	= 0x00400000,				// 予約06
		LOG_OUTPUT_RESERVED_07	= 0x00200000,				// 予約07
		LOG_OUTPUT_RESERVED_08	= 0x00100000,				// 予約08
		LOG_OUTPUT_RESERVED_09	= 0x00080000,				// 予約09
		LOG_OUTPUT_RESERVED_10	= 0x00040000,				// 予約10
		LOG_OUTPUT_RESERVED_11	= 0x00020000,				// 予約11
		LOG_OUTPUT_RESERVED_12	= 0x00010000,				// 予約12
		LOG_OUTPUT_RESERVED_13	= 0x00008000,				// 予約13
		LOG_OUTPUT_RESERVED_14	= 0x00004000,				// 予約14
		LOG_OUTPUT_RESERVED_15	= 0x00002000,				// 予約15
		LOG_OUTPUT_RESERVED_16	= 0x00001000,				// 予約16
		LOG_OUTPUT_RESERVED_17	= 0x00000800,				// 予約17
		LOG_OUTPUT_RESERVED_18	= 0x00000400,				// 予約18
		LOG_OUTPUT_RESERVED_19	= 0x00000200,				// 予約19
		LOG_OUTPUT_RESERVED_20	= 0x00000100,				// 予約20
		LOG_OUTPUT_RESERVED_21	= 0x00000080,				// 予約21
		LOG_OUTPUT_RESERVED_22	= 0x00000040,				// 予約22
		LOG_OUTPUT_RESERVED_23	= 0x00000020,				// 予約23
		LOG_OUTPUT_RESERVED_24	= 0x00000010,				// 予約24
		LOG_OUTPUT_RESERVED_25	= 0x00000008,				// 予約25
		LOG_OUTPUT_RESERVED_26	= 0x00000004,				// 予約26
		LOG_OUTPUT_RESERVED_27	= 0x00000002,				// 予約27
		LOG_OUTPUT_RESERVED_28	= 0x00000001,				// 予約28

	} LOG_OUTPUT_ENUM;


	// ログ設定情報構造体
	typedef struct
	{
		unsigned int					LogFileSize;								// ログファイルのサイズ
		unsigned int					LogOutputBit;								// ログ出力判定用ビット
		char							szFileName[CLOG_FILE_NAME_SIZE + 1];		// ログファイル名
	} LOG_SETTING_INFO_TABLE;


	// ログ出力情報テーブル
	typedef struct
	{
		unsigned int					Length;										// ログ長さ
		char*							pszLog;										// ログ
	} LOG_OUTPUT_INFO_TABLE;

	

	CMutex								m_cLogOutputListMutex;						// ログ出力リスト用ミューテックス
	bool								m_bStartFlag;								// ログ開始フラグ
	LOG_SETTING_INFO_TABLE				m_tLogSettingInfo;							// ログ設定情報
	std::list<LOG_OUTPUT_INFO_TABLE>	m_LogOutputList;							// ログ出力リスト
	char								m_szLogBuff[CLOG_MAX_SIZE + 1];				// ログ出力作業用バッファ
	char								m_szSettingFile[CLOG_FILE_NAME_SIZE + 1];	// 設定情報ファイル
	
	int									m_ErrorNo;									// エラー番号
	int									m_epfd;										// epollファイルディスクリプタ




	CLog();
	~CLog();


	bool Start();
	void Output(LOG_OUTPUT_ENUM eLog, const char* format, ...);

	static char* GetProcessName(char* pszProcessName, unsigned int BuffSize);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void LogWrite(bool bEnd);
};



