/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* Byte length pseudo-converter                                 *
* Lowri Jenkins April 2024                                     *
*                                                              *
* This pseudo converter pre-pends or appends length in bytes   *
*                                                              *
***************************************************************/

#include "StreamFormatConverter.h"
#include "StreamError.h"

// Raw Bytes Converter %r

class LengthConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printPseudo(const StreamFormat& fmt, StreamBuffer& output); 
    ssize_t scanPseudo(const StreamFormat& fmt, StreamBuffer& inputLine, size_t& cursor);
};

int LengthConverter::
parse(const StreamFormat&, StreamBuffer& info, const char*& source, bool)
{
    return pseudo_format;
}

bool LengthConverter::
printPseudo(const StreamFormat& fmt, StreamBuffer& output)
{
    const size_t length = output.length();
    int n;
    char* tempArray = new char[length+4];
    tempArray[0] = (length >> 24) & 0xFF;
    tempArray[1] = (length >> 16) & 0xFF;
    tempArray[2] = (length >> 8) & 0xFF;
    tempArray[3] = length & 0xFF;
    for (n = 0; n < length; n++){
        tempArray[n+4]=output[n];
    }
    output.set(tempArray, length+4);
    delete tempArray;
    return true;
}

ssize_t LengthConverter::
scanPseudo(const StreamFormat& fmt, StreamBuffer& inputLine, size_t& cursor)
{
    return 4;
}

RegisterConverter (LengthConverter, "l");
