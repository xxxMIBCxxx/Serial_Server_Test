#pragma once
//*****************************************************************************
// ConfFile�N���X
//*****************************************************************************
#include <stdio.h>
#include "string"
#include "list"





class CConfFile
{
private:
	// �s�̎��
	typedef enum
	{
		LINE_KIND_COMMENT = 0,
		LINE_KIND_SECTION = 1,
		LINE_KIND_KEY = 2,
	} LINE_KIND_ENUM;


	// �s���
	typedef struct
	{
		LINE_KIND_ENUM						eLineKind;					// �s��ʁi�R�����g , SECTION , KEY)
		std::string							strBuff;					// �ݒ���e
	} LINE_INFO_TABLE;


	// �ݒ���
	typedef struct
	{
		unsigned long						LineNo;						// �s�ԍ�
		std::string							strSection;					// �Z�N�V����
		std::string							strKey;						// �ݒ荀��
		std::string							strValue;					// �ݒ�l
	} KEY_INFO_TABLE;



	// �Z�N�V�������
	typedef struct
	{
		std::string							strSection;					// �Z�N�V����
		std::list<KEY_INFO_TABLE>			tKeyInfoList;				// �ݒ���
	} SECTION_INFO_TABLE;


private:
	bool								m_bInitFlag;
	int									m_ErrorNo;
	std::string							m_strFilePath;

	std::list<LINE_INFO_TABLE>			m_tLineList;					// �s���X�g�i�ݒ莞�̃t�@�C���쐬����ۂɎg�p�j
	std::list<SECTION_INFO_TABLE>		m_tSectionInfoList;				// �Z�N�V������񃊃X�g�i�ݒ�l���擾����ۂɎg�p�j


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






