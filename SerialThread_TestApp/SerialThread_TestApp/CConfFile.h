#pragma once
//*****************************************************************************
// ConfFileクラス
//*****************************************************************************
#include <stdio.h>
#include "string"
#include "list"





class CConfFile
{
private:
	// 行の種別
	typedef enum
	{
		LINE_KIND_COMMENT = 0,
		LINE_KIND_SECTION = 1,
		LINE_KIND_KEY = 2,
	} LINE_KIND_ENUM;


	// 行情報
	typedef struct
	{
		LINE_KIND_ENUM						eLineKind;					// 行種別（コメント , SECTION , KEY)
		std::string							strBuff;					// 設定内容
	} LINE_INFO_TABLE;


	// 設定情報
	typedef struct
	{
		unsigned long						LineNo;						// 行番号
		std::string							strSection;					// セクション
		std::string							strKey;						// 設定項目
		std::string							strValue;					// 設定値
	} KEY_INFO_TABLE;



	// セクション情報
	typedef struct
	{
		std::string							strSection;					// セクション
		std::list<KEY_INFO_TABLE>			tKeyInfoList;				// 設定情報
	} SECTION_INFO_TABLE;


private:
	bool								m_bInitFlag;
	int									m_ErrorNo;
	std::string							m_strFilePath;

	std::list<LINE_INFO_TABLE>			m_tLineList;					// 行リスト（設定時のファイル作成する際に使用）
	std::list<SECTION_INFO_TABLE>		m_tSectionInfoList;				// セクション情報リスト（設定値を取得する際に使用）


public:
	CConfFile(const char* pszFilePath=NULL);
	~CConfFile();
	bool GetValue(const char* pszSection, const char* pszKey, std::string& strValue);
	bool SetValue(const char* pszSection, const char* pszKey, const char* pszValue);

private:
	bool CreateLineList(FILE* pFile);
	bool SectionCheck(char* pszString, std::string& strSection);
	bool ValueCheck(char* pszString, std::string& strKey, std::string& strValue);
	void Clear_SectionInfoList();
	bool Add_SectionInfo(KEY_INFO_TABLE& tKeyInfo);
	bool Set_SectionInfo(KEY_INFO_TABLE& tKeyInfo);
	bool CreateConfFile(const char* pszFilePath);
	bool ReadConfFile(const char* pszFilePath);

	
	void Insert_LineInfo(KEY_INFO_TABLE& tKeyInfo);
	void Set_LineInfo(KEY_INFO_TABLE& tKeyInfo);
	void Set_SectionInfoList(KEY_INFO_TABLE& tKeyInfo);

public:
	static char* LTrim(char* pszString);
	static char* RTrim(char* pszString);
	static char* Trim(char* pszString);
	static char* GetProcessName(char* pszProcessName, unsigned int BuffSize);
};






