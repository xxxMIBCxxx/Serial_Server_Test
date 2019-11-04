#include <cstdio>
#include <time.h>
#include "kbhit.h"
#include "CSerialClient.h"

#define ESC						( 27 )


int main()
{
	CSerialClient* pcSerialClient = (CSerialClient*)new CSerialClient();


	timespec		tTimeSpec;
	tTimeSpec.tv_sec = 1;
	tTimeSpec.tv_nsec = 0;


	CSerialClient::RESULT_ENUM eRet = pcSerialClient->Start();
	if (eRet == CSerialClient::RESULT_SUCCESS)
	{
		printf("-----[ CSerialClient Demo ]-----\n");
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
		printf("CSerialClient::Start Error. [eRet:0x%08X]\n", eRet);
	}

	delete pcSerialClient;

	return 0;
}