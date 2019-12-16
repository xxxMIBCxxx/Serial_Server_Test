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
	char			szProcessName[64 + 1];
	char			szFilePath[256 + 1];


	memset(szProcessName, 0x00, sizeof(szProcessName));
	memset(szFilePath, 0x00, sizeof(szFilePath));

	m_bInitFlag = false;
	m_strFilePath = "";
	m_tLineList.clear();

	// 行リストをクリア
	m_tLineList.clear();

	// セクション情報リストをクリア
	Clear_SectionInfoList();

	// パラメータチェック
	if (pszFilePath == NULL)
	{
		GetProcessName(szProcessName, 64);
		sprintf(szFilePath,"/etc/%s.conf", szProcessName);
	}
	m_strFilePath = pszFilePath;

	// 設定ファイルの読み込み
	bRet = ReadConfFile(m_strFilePath.c_str());
	if (bRet == false)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::CConfFile - ReadConfFile Error.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return;
	}

	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CConfFile::~CConfFile()
{
	// 行リストをクリア
	m_tLineList.clear();

	// セクション情報リストをクリア
	Clear_SectionInfoList();
}



//-----------------------------------------------------------------------------
// 設定ファイルの行リスト作成
// ⇒指定されたファイルから行リストを作成する
//-----------------------------------------------------------------------------
bool CConfFile::CreateLineList(FILE* pFile)
{
	bool			bRet = false;
	bool			bSection = false;
	bool			bValue = false;
	std::string		strTempSection;
	std::string		strSection;
	std::string		strKey;
	std::string		strValue;
	char			szBuff[TEMP_BUFF_SIZE + 1];


	// 行リストをクリア
	m_tLineList.clear();
	
	// セクション情報リストをクリア
	Clear_SectionInfoList();

	// パラメータチェック
	if (pFile == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::CreateLineList - Param Error.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}
	fseek(pFile, SEEK_SET, 0);

	// EOFまで1行ずつ読み込む
	while (fgets(szBuff, TEMP_BUFF_SIZE, pFile) != NULL)
	{
		LINE_INFO_TABLE			tTempLineInfo;
		tTempLineInfo.eLineKind = LINE_KIND_COMMENT;
		tTempLineInfo.strBuff = szBuff;
		Trim(szBuff);

		// 行の1文字目が'#' or ';'の場合コメント行とする
		if ((szBuff[0] == '#') || (szBuff[0] == ';'))
		{
			tTempLineInfo.eLineKind = LINE_KIND_COMMENT;
		}
		// 何も記載していない場合もコメント行にする
		else if (strlen(szBuff) == 0)
		{
			tTempLineInfo.eLineKind = LINE_KIND_COMMENT;
		}
		else
		{
			// セクション行か調べる
			bSection = SectionCheck(szBuff, strTempSection);
			if (bSection == true)
			{
				tTempLineInfo.eLineKind = LINE_KIND_SECTION;
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
						tTempLineInfo.eLineKind = LINE_KIND_KEY;

						// セクション情報リストに設定情報を追加
						KEY_INFO_TABLE			tKeyInfo;
						tKeyInfo.LineNo = m_tLineList.size();
						tKeyInfo.strSection = strSection;
						tKeyInfo.strKey = strKey;
						tKeyInfo.strValue = strValue;
						bRet = Add_SectionInfo(tKeyInfo);
						if (bRet == false)
						{
#ifdef _CCONF_FILE_DEBUG_
							printf("Add_SectionInfo Error. [LineNo:%lu, Section:%s, Key:%s, Value:%s]\n", tKeyInfo.LineNo, tKeyInfo.strSection.c_str(), tKeyInfo.strKey.c_str(), tKeyInfo.strValue.c_str());
#endif	// #ifdef _CCONF_FILE_DEBUG_
							// ※エラーでも処理を継続させる
						}
					}
				}
			}
		}

		// リスト行に登録
		m_tLineList.push_back(tTempLineInfo);
	}

	return true;
}


//-----------------------------------------------------------------------------
// セクション情報リストに設定情報を追加
//-----------------------------------------------------------------------------
bool CConfFile::Add_SectionInfo(KEY_INFO_TABLE& tKeyInfo)
{
	SECTION_INFO_TABLE						tSectionInfo;
	std::list<SECTION_INFO_TABLE>::iterator	it_SectionInfo;
	std::list<KEY_INFO_TABLE>::iterator		it_KeyInfo;


	// セクション情報リストにセクションが既に登録されているか探す
	it_SectionInfo = m_tSectionInfoList.begin();
	while (it_SectionInfo != m_tSectionInfoList.end())
	{
		if (it_SectionInfo->strSection == tKeyInfo.strSection)
		{
			// 設定項目を探す
			it_KeyInfo = it_SectionInfo->tKeyInfoList.begin();
			while (it_KeyInfo != it_SectionInfo->tKeyInfoList.end())
			{
				// 既に登録済み
				if (it_KeyInfo->strKey == tKeyInfo.strKey)
				{
					return false;
				}
				it_KeyInfo++;
			}

			// 設定項目が見つからなかったので、設定項目を追加
			it_SectionInfo->tKeyInfoList.push_back(tKeyInfo);
			return true;
		}
		it_SectionInfo++;
	}

	// セクションが見つからなかった場合、新しいセクションとして追加
	tSectionInfo.strSection = tKeyInfo.strSection;
	tSectionInfo.tKeyInfoList.push_back(tKeyInfo);
	m_tSectionInfoList.push_back(tSectionInfo);

	return true;
}


//-----------------------------------------------------------------------------
// セクション情報リストをもとに、行リストに設定項目をセットする
//-----------------------------------------------------------------------------
bool CConfFile::Set_SectionInfo(KEY_INFO_TABLE& tKeyInfo)
{
	SECTION_INFO_TABLE						tSectionInfo;
	std::list<SECTION_INFO_TABLE>::iterator	it_SectionInfo;
	std::list<KEY_INFO_TABLE>::iterator		it_KeyInfo;
	unsigned long							LineNo = 0;


	// セクション情報リストにセクションが既に登録されているか探す
	it_SectionInfo = m_tSectionInfoList.begin();
	while (it_SectionInfo != m_tSectionInfoList.end())
	{
		if (it_SectionInfo->strSection == tKeyInfo.strSection)
		{
			// 設定項目を探す
			it_KeyInfo = it_SectionInfo->tKeyInfoList.begin();
			while (it_KeyInfo != it_SectionInfo->tKeyInfoList.end())
			{
				// 既に登録済み
				if (it_KeyInfo->strKey == tKeyInfo.strKey)
				{
					// 行リストの設定情報を変更
					tKeyInfo.LineNo = it_KeyInfo->LineNo;
					Set_LineInfo(tKeyInfo);
					return true;
				}
				// 行数を保持
				LineNo = it_KeyInfo->LineNo;
				it_KeyInfo++;
			}

			// 行リストに設定情報を追加	
			tKeyInfo.LineNo = LineNo + 1;
			Insert_LineInfo(tKeyInfo);
			return true;
		}
		it_SectionInfo++;
	}

	// 行リストにセクション・設定情報を追加
	Set_SectionInfoList(tKeyInfo);

	return true;
}


//-----------------------------------------------------------------------------
// 行リストの設定情報を変更
//-----------------------------------------------------------------------------
void CConfFile::Insert_LineInfo(KEY_INFO_TABLE& tKeyInfo)
{
	std::list<LINE_INFO_TABLE>::iterator	it_listinfo;
	LINE_INFO_TABLE							tLineInfo;
	unsigned long							LineNo = tKeyInfo.LineNo;


	// 挿入する行までループ
	it_listinfo = m_tLineList.begin();
	for (unsigned long i = 0; i < LineNo; i++)
	{
		it_listinfo++;
	}
	tLineInfo.eLineKind = LINE_KIND_KEY;
	tLineInfo.strBuff = tKeyInfo.strKey + " = " + tKeyInfo.strValue + "\n";
	it_listinfo = m_tLineList.insert(it_listinfo, tLineInfo);
}


//-----------------------------------------------------------------------------
// 行リストに設定情報を追加
//-----------------------------------------------------------------------------
void CConfFile::Set_LineInfo(KEY_INFO_TABLE& tKeyInfo)
{
	std::list<LINE_INFO_TABLE>::iterator	it_listinfo;
	unsigned long							LineNo = tKeyInfo.LineNo;


	// 内容を変更する行までループ
	it_listinfo = m_tLineList.begin();
	for (unsigned long i = 0; i < LineNo; i++)
	{
		it_listinfo++;
	}

	// 行の内容を変更
	it_listinfo->strBuff = tKeyInfo.strKey + " = " + tKeyInfo.strValue + "\n";
}


//-----------------------------------------------------------------------------
// 行リストにセクション・設定情報を追加
//-----------------------------------------------------------------------------
void CConfFile::Set_SectionInfoList(KEY_INFO_TABLE& tKeyInfo)
{
	LINE_INFO_TABLE							tLineInfo;


	// 行リストの最後にセクション・設定情報を追加
	tLineInfo.eLineKind = LINE_KIND_SECTION;
	tLineInfo.strBuff = "[" + tKeyInfo.strSection + "]\n";
	m_tLineList.push_back(tLineInfo);

	tLineInfo.eLineKind = LINE_KIND_KEY;
	tLineInfo.strBuff = tKeyInfo.strKey + " = " + tKeyInfo.strValue + "\n";
	m_tLineList.push_back(tLineInfo);
}


//-----------------------------------------------------------------------------
// セクション情報リストをクリア
//-----------------------------------------------------------------------------
void CConfFile::Clear_SectionInfoList()
{
	if (m_tSectionInfoList.size() != 0)
	{
		std::list< SECTION_INFO_TABLE>::iterator		it = m_tSectionInfoList.begin();
		while (it != m_tSectionInfoList.end())
		{
			// 設定情報リストクリア
			it->tKeyInfoList.clear();
			it++;
		}
	}

	// セクション情報リストクリア
	m_tSectionInfoList.clear();
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
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::SectionCheck - Param Error. [pszString:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
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
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::ValueCheck - Param Error. [pszString:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
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
	bool										bFind = false;
	SECTION_INFO_TABLE							tSectionInfo;
	std::list<SECTION_INFO_TABLE>::iterator		it;
	std::list<KEY_INFO_TABLE>::iterator			it2;


	// 引数チェック
	if (pszSection == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::GetValue - Param Error. [pszSection:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	if (pszKey == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::GetValue - Param Error. [pszKey:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	if (m_tSectionInfoList.size() == 0)
	{
		return false;
	}

	// セクション情報リストからセクションを探す
	bFind = false;
	it = m_tSectionInfoList.begin();
	while (it != m_tSectionInfoList.end())
	{
		if (it->strSection.compare(pszSection) == 0)
		{
			// 設定情報から設定項目を探す
			it2 = it->tKeyInfoList.begin();
			while (it2 != it->tKeyInfoList.end())
			{
				if (it2->strKey.compare(pszKey) == 0)
				{
					strValue = it2->strValue;
					bFind = true;
					break;
				}
				it2++;
			}

			if (bFind == true)
			{
				break;
			}
		}
		it++;
	}

	return bFind;
}


//-----------------------------------------------------------------------------
// 設定値の設定
//-----------------------------------------------------------------------------
bool CConfFile::SetValue(const char* pszSection, const char* pszKey, const char* pszValue)
{
	bool			bRet = false;


	// 引数チェック
	if (pszSection == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::SetValue - Param Error. [pszSection:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	if (pszKey == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::SetValue - Param Error. [pszKey:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	if (pszValue == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::SetValue - Param Error. [pszValue:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// セクション情報リストに設定情報を追加
	KEY_INFO_TABLE			tKeyInfo;
	tKeyInfo.LineNo = m_tLineList.size();
	tKeyInfo.strSection = pszSection;
	tKeyInfo.strKey = pszKey;
	tKeyInfo.strValue = pszValue;
	bRet = Set_SectionInfo(tKeyInfo);
	if (bRet == false)
	{
		return false;
	}

	// 設定ファイル作成
	bRet = CreateConfFile(m_strFilePath.c_str());
	if (bRet == false)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::SetValue - CreateConfFile Error.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// 設定ファイル読込
	bRet = ReadConfFile(m_strFilePath.c_str());
	if (bRet == false)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::SetValue - ReadConfFile Error.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// 設定ファイル作成
//-----------------------------------------------------------------------------
bool CConfFile::CreateConfFile(const char* pszFilePath)
{
	FILE*									pFile = NULL;
	std::list<LINE_INFO_TABLE>::iterator	it_ListInfo;


	// 引数チェック
	if (pszFilePath == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::CreateConfFile - Param Error. [pszFilePath:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// ファイルオープン
	pFile = fopen(pszFilePath, "w+");
	if (pFile == NULL)
	{
		m_ErrorNo = errno;
#ifdef _CCONF_FILE_DEBUG_
		perror("CConfFile::CreateConfFile - fopen.");
#endif	// #ifdef _CCONF_FILE_DEBUG_		
		return false;
	}

	// 行リストを使用して、設定ファイルを書き込み
	it_ListInfo = m_tLineList.begin();
	while (it_ListInfo != m_tLineList.end())
	{
		fputs(it_ListInfo->strBuff.c_str(), pFile);
		it_ListInfo++;
	}
	fclose(pFile);

	return true;
}


//-----------------------------------------------------------------------------
// 設定ファイル読込
//-----------------------------------------------------------------------------
bool CConfFile::ReadConfFile(const char *pszFilePath)
{
	bool			bRet = false;
	FILE*			pFile = NULL;

	
	// 引数チェック
	if (pszFilePath == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::ReadConfFile - Param Error. [pszFilePath:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// ファイルオープン
	pFile = fopen(pszFilePath, "r");
	if (pFile == NULL)
	{
		m_ErrorNo = errno;
#ifdef _CCONF_FILE_DEBUG_
		perror("CConfFile::ReadConfFile - fopen.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// 行リスト作成
	bRet = CreateLineList(pFile);
	if (bRet == false)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::ReadConfFile - CreateLineList Error.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		fclose(pFile);
		return false;
	}
	fclose(pFile);

	return true;
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


//-----------------------------------------------------------------------------
// プロセス名称を取得
//-----------------------------------------------------------------------------
char* CConfFile::GetProcessName(char* pszProcessName, unsigned int BuffSize)
{
	char* pPos = NULL;


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


