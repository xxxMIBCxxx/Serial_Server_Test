#include <cstdio>
#include <stdlib.h>
#include <time.h>
#include "kbhit.h"
#include "CSerialThread.h"
#include "CLog.h"
#include "CConfFile.h"
#include "mcheck.h"

#define ESC						( 27 )


int main()
{
	mtrace();


	CLog							cLog;
	CConfFile						cConfFile;


	CSerialThread::CLASS_PARAM_TABLE			tClassParam;
	tClassParam.pcLog = &cLog;
	tClassParam.pcConfFile = &cConfFile;
	CSerialThread* pcSerialThread = (CSerialThread*)new CSerialThread(tClassParam);

	timespec		tTimeSpec;
	tTimeSpec.tv_sec = 0;
	tTimeSpec.tv_nsec = 500000000;


	CSerialThread::RESULT_ENUM eRet = pcSerialThread->Start();
	if (eRet == CSerialThread::RESULT_SUCCESS)
	{
		printf("-----[ CSerialThread Demo ]-----\n");
		printf(" [Enter] key : Demo End\n");

		malloc(100);

		while (1)
		{
			if (kbhit())
			{
				break;
			}
			nanosleep(&tTimeSpec, NULL);

			char			*pBuff = (char *)malloc(6);
			strncpy(pBuff, "Hello", 5);

			CSerialThread::SERIAL_DATA_TABLE			tSerialData;
			tSerialData.pClas = (void *)0x12345678;
			tSerialData.pData = pBuff;
			tSerialData.Size = 5;
			pcSerialThread->SetSerialDataList(tSerialData);
			

		}
	}
	else
	{
		printf("CSerialThread::Start Error. [eRet:0x%08X]\n", eRet);
	}

	delete pcSerialThread;

	muntrace();

	return 0;
}
