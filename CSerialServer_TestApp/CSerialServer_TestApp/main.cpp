#include <cstdio>
#include <stdlib.h>
#include <time.h>
#include "kbhit.h"
#include "CSerialServer.h"
//#include "mcheck.h"

#define ESC						( 27 )


int main()
{
//	mtrace();

	CSerialServer* pcSerialServer = (CSerialServer*)new CSerialServer();


	timespec		tTimeSpec;
	tTimeSpec.tv_sec = 1;
	tTimeSpec.tv_nsec = 0;


	malloc(200);

	CSerialServer::RESULT_ENUM eRet =  pcSerialServer->Start();
	if (eRet == CSerialServer::RESULT_SUCCESS)
	{
		printf("-----[ CSerialServer Demo ]-----\n");
		printf(" [Enter] key : Demo End\n");


		while (1)
		{
			if (kbhit())
			{
				break;
			}
			nanosleep(&tTimeSpec, NULL);

		}
	}
	else
	{
		printf("CSerialServer::Start Error. [eRet:0x%08X]\n", eRet);
	}

	delete pcSerialServer;

//	muntrace();

	return 0;
}
