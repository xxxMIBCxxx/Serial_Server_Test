//*****************************************************************************
// ConfFileクラス
//*****************************************************************************
#include "CConfFile.h"
#include <string.h>


#define _CCONF_FILE_DEBUG_
#define TEMP_BUFF_SIZE				512


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CConfFile::CConfFile(const char *pszFilePath)
{
	bool			bRet = false;


	m_bInitFlag = false;
	m_pFile = NULL;

	// パラメータチェック
	if (pszFilePath == NULL)
	{
		return;
	}

	// ファイルチェック
	m_pFile = fopen(pszFilePath, "r");
	if (m_pFile == NULL)
	{
		m_ErrorNo = errno;
#ifdef _CCONF_FILE_DEBUG_
		perror("CConfFile::CConfFile - fopen");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return;
	}

	// 設定リスト作成
	bRet = CreateConfigList(m_pFile, m_tConfigList);
	if (bRet == false)
	{
		return;
	}

	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CConfFile::~CConfFile()
{
	// ファイルクローズ
	if (m_pFile != NULL)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}

	// 設定リスト破棄
	m_tConfigList.clear();
}


//-----------------------------------------------------------------------------
// 設定リスト作成
// ⇒指定されたファイルから設定リストを作成する
//-----------------------------------------------------------------------------
bool CConfFile::CreateConfigList(FILE* pFile, std::list<CONFFILE_INFO_TABLE>& tConfigList)
{
	bool			bSection = false;
	bool			bValue = false;
	std::string		strTempSection;
	std::string		strSection;
	std::string		strKey;
	std::string		strValue;
	char			szBuff[TEMP_BUFF_SIZE + 1];
	char*			pPos = NULL;


	// リストをクリア
	tConfigList.clear();

	// パラメータチェック
	if (pFile == NULL)
	{
		return false;
	}
	fseek(pFile, SEEK_SET, 0);

	// EOFまで1行ずつ読み込む
	while (fgets(szBuff, TEMP_BUFF_SIZE, pFile) != NULL)
	{
		// コメント('#')文字の部分をNULL文字に変換
		pPos = strstr(szBuff, "#");
		if (pPos != NULL)
		{
			*pPos = '\0';
		}

		Trim(szBuff);
	
		// セクション行か調べる
		bSection = SectionCheck(szBuff, strTempSection);
		if (bSection == true)
		{
			strSection = strTempSection;
		}
		else
		{
			if (strSection.length() != 0)
			{
				// 設定行か調べる
				bValue = ValueCheck(szBuff, strKey, strValue);
				if (bValue == true)
				{
					CONFFILE_INFO_TABLE			tTempConfig;
					tTempConfig.strSection = strSection;
					tTempConfig.strKey = strKey;
					tTempConfig.strValue = strValue;

					// リストに登録
					tConfigList.push_back(tTempConfig);
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// セクション行チェック
// ⇒引数で渡した行がセクション行か調べる
//-----------------------------------------------------------------------------
bool CConfFile::SectionCheck(char* pszString, std::string& strSection)
{
	char*			pEndPos = NULL;
	char*			pStartPos = NULL;
	char			szTemp[TEMP_BUFF_SIZE + 1];


	strSection = "";

	// パラメータチェック
	if (pszString == NULL)
	{
		return false;
	}

	// 最初の文字が'['以外の場合（セクション行でない）
	if (pszString[0] != '[')
	{
		return false;
	}
	pStartPos = &pszString[1];

	// ']'を探す
	pEndPos = strstr(pszString, "]");
	if (pEndPos == NULL)
	{
		return false;
	}
	*pEndPos = '\0';

	// セクション名の途中に'#'がないか調べる
	if (strstr(pStartPos, "#") != NULL)
	{
		return false;
	}

	// セクション名を取得
	strcpy(szTemp, pStartPos);
	Trim(szTemp);
	strSection = szTemp;
	if (strSection.length() == 0)
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// 設定行チェック
// ⇒引数で渡した行が設定行か調べる
//-----------------------------------------------------------------------------
bool CConfFile::ValueCheck(char* pszString, std::string& strKey, std::string& strValue)
{
	char*			pEndPos = NULL;
	char*			pStartPos = NULL;
	char			szTemp[TEMP_BUFF_SIZE + 1];


	strKey = "";
	strValue = "";

	// パラメータチェック
	if (pszString == NULL)
	{
		return false;
	}

	// Keyを探す
	pStartPos = pszString;
	pEndPos = strstr(pStartPos, "=");
	if (pEndPos == NULL)
	{
		return false;
	}
	*pEndPos = '\0';

	// keyの途中に'#'がないか調べる
	if (strstr(pStartPos, "#") != NULL)
	{
		return false;
	}

	// Keyを取得
	strcpy(szTemp, pStartPos);
	Trim(szTemp);
	strKey = szTemp;
	if (strKey.length() == 0)
	{
		return false;
	}

	// Valueを探す
	pStartPos = pEndPos + 1;
	pEndPos = pStartPos;
	Trim(pStartPos);
	while ((*pEndPos != 0x20) && (*pEndPos != '#') && (*pEndPos != '\0') && (*pEndPos != '\n'))
	{
		pEndPos++;
	}
	*pEndPos = '\0';
	strcpy(szTemp, pStartPos);
	strValue = szTemp;

	return true;
}


//-----------------------------------------------------------------------------
// 設定値取得
//-----------------------------------------------------------------------------
bool CConfFile::GetValue(const char* pszSection, const char* pszKey, std::string& strValue)
{
	bool						bFind = false;
	CONFFILE_INFO_TABLE			tTempConfig;


	// 引数チェック
	if (pszSection == NULL)
	{
		return false;
	}

	if (pszKey == NULL)
	{
		return false;
	}

	bFind = false;
	std::list<CONFFILE_INFO_TABLE>::iterator it = m_tConfigList.begin();
	while (it != m_tConfigList.end())
	{
		tTempConfig = *it;

		// セクションとキーが一致した場合
		if ((strcmp(tTempConfig.strSection.c_str(), pszSection) == 0) && (strcmp(tTempConfig.strKey.c_str(), pszKey) == 0))
		{
			bFind = true;
			strValue = tTempConfig.strValue;
			break;
		}
		it++;
	}

	return bFind;
}


//-----------------------------------------------------------------------------
// 文字列の左側の空白を取り除く
//-----------------------------------------------------------------------------
char* CConfFile::LTrim(char* pszString)
{
	char*			pPos = pszString;
	unsigned int	i = 0;


	// 左側から空白でない位置を探す
	while (*pPos != '\0')
	{
		if (*pPos != 0x20)
		{
			break;
		}
		pPos++;
	}

	// NULL終端までコピー
	i = 0;
	while (*pPos != '\0')
	{
		pszString[i] = *pPos;
		pPos++;
		i++;
	}
	pszString[i] = '\0';

	return pszString;
}


//-----------------------------------------------------------------------------
// 文字列の右側の空白を取り除く
//-----------------------------------------------------------------------------
char* CConfFile::RTrim(char* pszString)
{
	char*				pPos = pszString;
	unsigned int		i = 0;


	// 終端位置を探す
	i = 0;
	while (*pPos != '\0')
	{
		pPos++;
		i++;
	}

	// 終端から空白でな位置を探しながら、NULL終端を格納していく
	while (i != 0)
	{
		if (pszString[(i - 1)] != 0x20)
		{
			break;
		}
		pszString[(i - 1)] = '\0';
		i--;
	}

	return pszString;
}


//-----------------------------------------------------------------------------
// 文字列の両側（左右）の空白を取り除く
//-----------------------------------------------------------------------------
char* CConfFile::Trim(char* pszString)
{
	return (LTrim(RTrim(pszString)));
}



