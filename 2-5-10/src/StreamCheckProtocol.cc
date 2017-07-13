#include <iostream>
#include "StreamCore.h"

#include <epicsExport.h>
#include "StreamCheckProtocol.h"

class StreamCheckProtocol : public StreamCore
{
    void startTimer(unsigned long timeout) { }
    bool getFieldAddress(const char* fieldname,
        StreamBuffer& address) { return true; }
    bool formatValue(const StreamFormat&,
        const void* fieldaddress) { return true; }
    bool matchValue(const StreamFormat&,
        const void* fieldaddress) { return true; }
    void lockMutex() { }
    void releaseMutex() { }
};

epicsShareExtern int checkProtocol(const char* filename, const char* protocol)
{
	StreamCheckProtocol* sc = new StreamCheckProtocol();
	if (protocol == NULL)
	{
		protocol = "";
	}
	if (filename != NULL)
	{
		if ( sc->parse(filename, protocol) )
		{
			return 0;
		}
		else
		{
			std::cerr << "ERROR: protocol" << std::endl;
			return -1;
		}
	}
	else
	{
		std::cerr << "ERROR: filename or ptotocol NULL" << std::endl;
		return -1;
	}
}
