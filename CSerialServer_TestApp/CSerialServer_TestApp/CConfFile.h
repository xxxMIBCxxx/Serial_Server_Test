#pragma once
//*****************************************************************************
// ConfFileƒNƒ‰ƒX
//*****************************************************************************
#include <stdio.h>
#include "string"
#include "list"


typedef struct 
{
	std::string							strSection;
	std::string							strKey;
	std::string							strValue;
} CONFFILE_INFO_TABLE;


class CConfFile
{
private:
	bool								m_bInitFlag;
	int									m_ErrorNo;
	FILE*								m_pFile;
	std::list<CONFFILE_INFO_TABLE>		m_tConfigList;

public:
	CConfFile(const char* pszFilePath);
	~CConfFile();
	bool CreateConfigList(FILE* pFile, std::list<CONFFILE_INFO_TABLE>& tConfigList);
	bool SectionCheck(char* pszString, std::string& strSection);
	bool ValueCheck(char* pszString, std::string& strKey, std::string& strValue);
	bool GetValue(const char* pszSection, const char* pszKey, std::string& strValue);


	static char* LTrim(char* pszString);
	static char* RTrim(char* pszString);
	static char* Trim(char* pszString);
};






