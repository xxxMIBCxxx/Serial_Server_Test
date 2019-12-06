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


	m_bInitFlag = false;
	m_pFile = NULL;

	// �p�����[�^�`�F�b�N
	if (pszFilePath == NULL)
	{
		return;
	}

	// �t�@�C���`�F�b�N
	m_pFile = fopen(pszFilePath, "r");
	if (m_pFile == NULL)
	{
		m_ErrorNo = errno;
#ifdef _CCONF_FILE_DEBUG_
		perror("CConfFile::CConfFile - fopen");
#endif	// #ifdef _CCONF_FILE_DEBUG_
		return;
	}

	// �ݒ胊�X�g�쐬
	bRet = CreateConfigList(m_pFile, m_tConfigList);
	if (bRet == false)
	{
		return;
	}

	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CConfFile::~CConfFile()
{
	// �t�@�C���N���[�Y
	if (m_pFile != NULL)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}

	// �ݒ胊�X�g�j��
	m_tConfigList.clear();
}


//-----------------------------------------------------------------------------
// �ݒ胊�X�g�쐬
// �ˎw�肳�ꂽ�t�@�C������ݒ胊�X�g���쐬����
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


	// ���X�g���N���A
	tConfigList.clear();

	// �p�����[�^�`�F�b�N
	if (pFile == NULL)
	{
		return false;
	}
	fseek(pFile, SEEK_SET, 0);

	// EOF�܂�1�s���ǂݍ���
	while (fgets(szBuff, TEMP_BUFF_SIZE, pFile) != NULL)
	{
		// �R�����g('#')�����̕�����NULL�����ɕϊ�
		pPos = strstr(szBuff, "#");
		if (pPos != NULL)
		{
			*pPos = '\0';
		}

		Trim(szBuff);
	
		// �Z�N�V�����s�����ׂ�
		bSection = SectionCheck(szBuff, strTempSection);
		if (bSection == true)
		{
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
					CONFFILE_INFO_TABLE			tTempConfig;
					tTempConfig.strSection = strSection;
					tTempConfig.strKey = strKey;
					tTempConfig.strValue = strValue;

					// ���X�g�ɓo�^
					tConfigList.push_back(tTempConfig);
				}
			}
		}
	}

	return true;
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
	bool						bFind = false;
	CONFFILE_INFO_TABLE			tTempConfig;


	// �����`�F�b�N
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

		// �Z�N�V�����ƃL�[����v�����ꍇ
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



