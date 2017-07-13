#include <windows.h>
#include <iostream>
#include <errlog.h>
#include <epicsExit.h>
#include <shareLib.h>
#include "StreamCheckProtocol.h"

int main(int argc, char* argv[])
{
	int ret;
	DebugBreak();
	if (argc >= 2) // argv[2] will then be protocol name or NULL
	{
		ret = checkProtocol(argv[1], argv[2]);
	}
	else
	{
		std::cerr << "No filename given" << std::endl;
		ret = 1;
	}
	errlogFlush();
	if (ret == 0)
	{
		std::cerr << "File general syntax OK - ignore any Protocol '' not found error if you didn't specify an explict protocol function to check" << std::endl;
	}
	epicsExit(ret);
	return ret;/*NOTREACHED*/
}
