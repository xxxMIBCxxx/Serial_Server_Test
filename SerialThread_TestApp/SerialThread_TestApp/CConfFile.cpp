//*****************************************************************************
// ConfFile�N���X
//*****************************************************************************
#include "CConfFile.h"
#include <string.h>


#define _CCONF_FILE_DEBUG_
#define TEMP_BUFF_SIZE				512


//-----------------------------------------------------------------------------
// �R���X�g���N�^
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

	// �s���X�g���N���A
	m_tLineList.clear();

	// �Z�N�V������񃊃X�g���N���A
	Clear_SectionInfoList();

	// �p�����[�^�`�F�b�N
	if (pszFilePath == NULL)
	{
		GetProcessName(szProcessName, 64);
		sprintf(szFilePath,"/etc/%s.conf", szProcessName);
	}
	m_strFilePath = pszFilePath;

	// �ݒ�t�@�C���̓ǂݍ���
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
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CConfFile::~CConfFile()
{
	// �s���X�g���N���A
	m_tLineList.clear();

	// �Z�N�V������񃊃X�g���N���A
	Clear_SectionInfoList();
}



//-----------------------------------------------------------------------------
// �ݒ�t�@�C���̍s���X�g�쐬
// �ˎw�肳�ꂽ�t�@�C������s���X�g���쐬����
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


	// �s���X�g���N���A
	m_tLineList.clear();
	
	// �Z�N�V������񃊃X�g���N���A
	Clear_SectionInfoList();

	// �p�����[�^�`�F�b�N
	if (pFile == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::CreateLineList - Param Error.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}
	fseek(pFile, SEEK_SET, 0);

	// EOF�܂�1�s���ǂݍ���
	while (fgets(szBuff, TEMP_BUFF_SIZE, pFile) != NULL)
	{
		LINE_INFO_TABLE			tTempLineInfo;
		tTempLineInfo.eLineKind = LINE_KIND_COMMENT;
		tTempLineInfo.strBuff = szBuff;
		Trim(szBuff);

		// �s��1�����ڂ�'#' or ';'�̏ꍇ�R�����g�s�Ƃ���
		if ((szBuff[0] == '#') || (szBuff[0] == ';'))
		{
			tTempLineInfo.eLineKind = LINE_KIND_COMMENT;
		}
		// �����L�ڂ��Ă��Ȃ��ꍇ���R�����g�s�ɂ���
		else if (strlen(szBuff) == 0)
		{
			tTempLineInfo.eLineKind = LINE_KIND_COMMENT;
		}
		else
		{
			// �Z�N�V�����s�����ׂ�
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
					// �ݒ�s�����ׂ�
					bValue = ValueCheck(szBuff, strKey, strValue);
					if (bValue == true)
					{
						tTempLineInfo.eLineKind = LINE_KIND_KEY;

						// �Z�N�V������񃊃X�g�ɐݒ����ǉ�
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
							// ���G���[�ł��������p��������
						}
					}
				}
			}
		}

		// ���X�g�s�ɓo�^
		m_tLineList.push_back(tTempLineInfo);
	}

	return true;
}


//-----------------------------------------------------------------------------
// �Z�N�V������񃊃X�g�ɐݒ����ǉ�
//-----------------------------------------------------------------------------
bool CConfFile::Add_SectionInfo(KEY_INFO_TABLE& tKeyInfo)
{
	SECTION_INFO_TABLE						tSectionInfo;
	std::list<SECTION_INFO_TABLE>::iterator	it_SectionInfo;
	std::list<KEY_INFO_TABLE>::iterator		it_KeyInfo;


	// �Z�N�V������񃊃X�g�ɃZ�N�V���������ɓo�^����Ă��邩�T��
	it_SectionInfo = m_tSectionInfoList.begin();
	while (it_SectionInfo != m_tSectionInfoList.end())
	{
		if (it_SectionInfo->strSection == tKeyInfo.strSection)
		{
			// �ݒ荀�ڂ�T��
			it_KeyInfo = it_SectionInfo->tKeyInfoList.begin();
			while (it_KeyInfo != it_SectionInfo->tKeyInfoList.end())
			{
				// ���ɓo�^�ς�
				if (it_KeyInfo->strKey == tKeyInfo.strKey)
				{
					return false;
				}
				it_KeyInfo++;
			}

			// �ݒ荀�ڂ�������Ȃ������̂ŁA�ݒ荀�ڂ�ǉ�
			it_SectionInfo->tKeyInfoList.push_back(tKeyInfo);
			return true;
		}
		it_SectionInfo++;
	}

	// �Z�N�V������������Ȃ������ꍇ�A�V�����Z�N�V�����Ƃ��Ēǉ�
	tSectionInfo.strSection = tKeyInfo.strSection;
	tSectionInfo.tKeyInfoList.push_back(tKeyInfo);
	m_tSectionInfoList.push_back(tSectionInfo);

	return true;
}


//-----------------------------------------------------------------------------
// �Z�N�V������񃊃X�g�����ƂɁA�s���X�g�ɐݒ荀�ڂ��Z�b�g����
//-----------------------------------------------------------------------------
bool CConfFile::Set_SectionInfo(KEY_INFO_TABLE& tKeyInfo)
{
	SECTION_INFO_TABLE						tSectionInfo;
	std::list<SECTION_INFO_TABLE>::iterator	it_SectionInfo;
	std::list<KEY_INFO_TABLE>::iterator		it_KeyInfo;
	unsigned long							LineNo = 0;


	// �Z�N�V������񃊃X�g�ɃZ�N�V���������ɓo�^����Ă��邩�T��
	it_SectionInfo = m_tSectionInfoList.begin();
	while (it_SectionInfo != m_tSectionInfoList.end())
	{
		if (it_SectionInfo->strSection == tKeyInfo.strSection)
		{
			// �ݒ荀�ڂ�T��
			it_KeyInfo = it_SectionInfo->tKeyInfoList.begin();
			while (it_KeyInfo != it_SectionInfo->tKeyInfoList.end())
			{
				// ���ɓo�^�ς�
				if (it_KeyInfo->strKey == tKeyInfo.strKey)
				{
					// �s���X�g�̐ݒ����ύX
					tKeyInfo.LineNo = it_KeyInfo->LineNo;
					Set_LineInfo(tKeyInfo);
					return true;
				}
				// �s����ێ�
				LineNo = it_KeyInfo->LineNo;
				it_KeyInfo++;
			}

			// �s���X�g�ɐݒ����ǉ�	
			tKeyInfo.LineNo = LineNo + 1;
			Insert_LineInfo(tKeyInfo);
			return true;
		}
		it_SectionInfo++;
	}

	// �s���X�g�ɃZ�N�V�����E�ݒ����ǉ�
	Set_SectionInfoList(tKeyInfo);

	return true;
}


//-----------------------------------------------------------------------------
// �s���X�g�̐ݒ����ύX
//-----------------------------------------------------------------------------
void CConfFile::Insert_LineInfo(KEY_INFO_TABLE& tKeyInfo)
{
	std::list<LINE_INFO_TABLE>::iterator	it_listinfo;
	LINE_INFO_TABLE							tLineInfo;
	unsigned long							LineNo = tKeyInfo.LineNo;


	// �}������s�܂Ń��[�v
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
// �s���X�g�ɐݒ����ǉ�
//-----------------------------------------------------------------------------
void CConfFile::Set_LineInfo(KEY_INFO_TABLE& tKeyInfo)
{
	std::list<LINE_INFO_TABLE>::iterator	it_listinfo;
	unsigned long							LineNo = tKeyInfo.LineNo;


	// ���e��ύX����s�܂Ń��[�v
	it_listinfo = m_tLineList.begin();
	for (unsigned long i = 0; i < LineNo; i++)
	{
		it_listinfo++;
	}

	// �s�̓��e��ύX
	it_listinfo->strBuff = tKeyInfo.strKey + " = " + tKeyInfo.strValue + "\n";
}


//-----------------------------------------------------------------------------
// �s���X�g�ɃZ�N�V�����E�ݒ����ǉ�
//-----------------------------------------------------------------------------
void CConfFile::Set_SectionInfoList(KEY_INFO_TABLE& tKeyInfo)
{
	LINE_INFO_TABLE							tLineInfo;


	// �s���X�g�̍Ō�ɃZ�N�V�����E�ݒ����ǉ�
	tLineInfo.eLineKind = LINE_KIND_SECTION;
	tLineInfo.strBuff = "[" + tKeyInfo.strSection + "]\n";
	m_tLineList.push_back(tLineInfo);

	tLineInfo.eLineKind = LINE_KIND_KEY;
	tLineInfo.strBuff = tKeyInfo.strKey + " = " + tKeyInfo.strValue + "\n";
	m_tLineList.push_back(tLineInfo);
}


//-----------------------------------------------------------------------------
// �Z�N�V������񃊃X�g���N���A
//-----------------------------------------------------------------------------
void CConfFile::Clear_SectionInfoList()
{
	if (m_tSectionInfoList.size() != 0)
	{
		std::list< SECTION_INFO_TABLE>::iterator		it = m_tSectionInfoList.begin();
		while (it != m_tSectionInfoList.end())
		{
			// �ݒ��񃊃X�g�N���A
			it->tKeyInfoList.clear();
			it++;
		}
	}

	// �Z�N�V������񃊃X�g�N���A
	m_tSectionInfoList.clear();
}


//-----------------------------------------------------------------------------
// �Z�N�V�����s�`�F�b�N
// �ˈ����œn�����s���Z�N�V�����s�����ׂ�
//-----------------------------------------------------------------------------
bool CConfFile::SectionCheck(char* pszString, std::string& strSection)
{
	char*			pEndPos = NULL;
	char*			pStartPos = NULL;
	char			szTemp[TEMP_BUFF_SIZE + 1];


	strSection = "";

	// �p�����[�^�`�F�b�N
	if (pszString == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::SectionCheck - Param Error. [pszString:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// �ŏ��̕�����'['�ȊO�̏ꍇ�i�Z�N�V�����s�łȂ��j
	if (pszString[0] != '[')
	{
		return false;
	}
	pStartPos = &pszString[1];

	// ']'��T��
	pEndPos = strstr(pszString, "]");
	if (pEndPos == NULL)
	{
		return false;
	}
	*pEndPos = '\0';

	// �Z�N�V�������̓r����'#'���Ȃ������ׂ�
	if (strstr(pStartPos, "#") != NULL)
	{
		return false;
	}

	// �Z�N�V���������擾
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
// �ݒ�s�`�F�b�N
// �ˈ����œn�����s���ݒ�s�����ׂ�
//-----------------------------------------------------------------------------
bool CConfFile::ValueCheck(char* pszString, std::string& strKey, std::string& strValue)
{
	char*			pEndPos = NULL;
	char*			pStartPos = NULL;
	char			szTemp[TEMP_BUFF_SIZE + 1];


	strKey = "";
	strValue = "";

	// �p�����[�^�`�F�b�N
	if (pszString == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::ValueCheck - Param Error. [pszString:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// Key��T��
	pStartPos = pszString;
	pEndPos = strstr(pStartPos, "=");
	if (pEndPos == NULL)
	{
		return false;
	}
	*pEndPos = '\0';

	// key�̓r����'#'���Ȃ������ׂ�
	if (strstr(pStartPos, "#") != NULL)
	{
		return false;
	}

	// Key���擾
	strcpy(szTemp, pStartPos);
	Trim(szTemp);
	strKey = szTemp;
	if (strKey.length() == 0)
	{
		return false;
	}

	// Value��T��
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
// �ݒ�l�擾
//-----------------------------------------------------------------------------
bool CConfFile::GetValue(const char* pszSection, const char* pszKey, std::string& strValue)
{
	bool										bFind = false;
	SECTION_INFO_TABLE							tSectionInfo;
	std::list<SECTION_INFO_TABLE>::iterator		it;
	std::list<KEY_INFO_TABLE>::iterator			it2;


	// �����`�F�b�N
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

	// �Z�N�V������񃊃X�g����Z�N�V������T��
	bFind = false;
	it = m_tSectionInfoList.begin();
	while (it != m_tSectionInfoList.end())
	{
		if (it->strSection.compare(pszSection) == 0)
		{
			// �ݒ��񂩂�ݒ荀�ڂ�T��
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
// �ݒ�l�̐ݒ�
//-----------------------------------------------------------------------------
bool CConfFile::SetValue(const char* pszSection, const char* pszKey, const char* pszValue)
{
	bool			bRet = false;


	// �����`�F�b�N
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

	// �Z�N�V������񃊃X�g�ɐݒ����ǉ�
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

	// �ݒ�t�@�C���쐬
	bRet = CreateConfFile(m_strFilePath.c_str());
	if (bRet == false)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::SetValue - CreateConfFile Error.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// �ݒ�t�@�C���Ǎ�
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
// �ݒ�t�@�C���쐬
//-----------------------------------------------------------------------------
bool CConfFile::CreateConfFile(const char* pszFilePath)
{
	FILE*									pFile = NULL;
	std::list<LINE_INFO_TABLE>::iterator	it_ListInfo;


	// �����`�F�b�N
	if (pszFilePath == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::CreateConfFile - Param Error. [pszFilePath:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// �t�@�C���I�[�v��
	pFile = fopen(pszFilePath, "w+");
	if (pFile == NULL)
	{
		m_ErrorNo = errno;
#ifdef _CCONF_FILE_DEBUG_
		perror("CConfFile::CreateConfFile - fopen.");
#endif	// #ifdef _CCONF_FILE_DEBUG_		
		return false;
	}

	// �s���X�g���g�p���āA�ݒ�t�@�C������������
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
// �ݒ�t�@�C���Ǎ�
//-----------------------------------------------------------------------------
bool CConfFile::ReadConfFile(const char *pszFilePath)
{
	bool			bRet = false;
	FILE*			pFile = NULL;

	
	// �����`�F�b�N
	if (pszFilePath == NULL)
	{
#ifdef _CCONF_FILE_DEBUG_
		printf("CConfFile::ReadConfFile - Param Error. [pszFilePath:NULL]");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// �t�@�C���I�[�v��
	pFile = fopen(pszFilePath, "r");
	if (pFile == NULL)
	{
		m_ErrorNo = errno;
#ifdef _CCONF_FILE_DEBUG_
		perror("CConfFile::ReadConfFile - fopen.");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return false;
	}

	// �s���X�g�쐬
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
// ������̍����̋󔒂���菜��
//-----------------------------------------------------------------------------
char* CConfFile::LTrim(char* pszString)
{
	char*			pPos = pszString;
	unsigned int	i = 0;


	// ��������󔒂łȂ��ʒu��T��
	while (*pPos != '\0')
	{
		if (*pPos != 0x20)
		{
			break;
		}
		pPos++;
	}

	// NULL�I�[�܂ŃR�s�[
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
// ������̉E���̋󔒂���菜��
//-----------------------------------------------------------------------------
char* CConfFile::RTrim(char* pszString)
{
	char*				pPos = pszString;
	unsigned int		i = 0;


	// �I�[�ʒu��T��
	i = 0;
	while (*pPos != '\0')
	{
		pPos++;
		i++;
	}

	// �I�[����󔒂łȈʒu��T���Ȃ���ANULL�I�[���i�[���Ă���
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
// ������̗����i���E�j�̋󔒂���菜��
//-----------------------------------------------------------------------------
char* CConfFile::Trim(char* pszString)
{
	return (LTrim(RTrim(pszString)));
}


//-----------------------------------------------------------------------------
// �v���Z�X���̂��擾
//-----------------------------------------------------------------------------
char* CConfFile::GetProcessName(char* pszProcessName, unsigned int BuffSize)
{
	char* pPos = NULL;


	// �����`�F�b�N
	if ((pszProcessName == NULL) || (BuffSize == 0))
	{
		return NULL;
	}

	// �v���Z�X���̂��R�s�[
	strncpy(pszProcessName, program_invocation_short_name, BuffSize);

	// �g���q������ꍇ�́A�g���q���폜
	pPos = strchr(pszProcessName, '.');
	if (pPos != NULL)
	{
		*pPos = '\0';
	}

	return pszProcessName;
}


