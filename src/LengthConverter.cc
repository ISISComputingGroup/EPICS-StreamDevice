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

// LengthConverter %l

class LengthConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printPseudo(const StreamFormat &fmt, StreamBuffer &output);
    void convertBytesBigEndian(int width, char *tempArray, const size_t length);
    void convertBytesLittleEndian(int width, char *tempArray, const size_t length);

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
    int n, width;
    if(fmt.width == 0){
        width = 4;
    }else{
        width = fmt.width;
    }
    char* tempArray = new char[length+width];
    if (fmt.flags & alt_flag){
        convertBytesLittleEndian(width, tempArray, length);
    }else{
        convertBytesBigEndian(width, tempArray, length);
    }
    for (n = 0; n < length; n++){
        tempArray[n+width]=output[n];
    }
    output.set(tempArray, width+length);
    delete tempArray;
    return true;
}

void LengthConverter::convertBytesBigEndian(int width, char *tempArray, const size_t length)
{
    for (int i = 0; i < width; i++)
    {
        size_t shift = (width - 1 - i) * 8;
        tempArray[i] = (length >> (shift)) & 0xFF;
    }
}

void LengthConverter::convertBytesLittleEndian(int width, char *tempArray, const size_t length)
{
    for (int i = 0; i < width; i++)
    {
        size_t shift = i * 8;
        tempArray[i] = (length >> (shift)) & 0xFF;
    }
}

ssize_t LengthConverter::
scanPseudo(const StreamFormat& fmt, StreamBuffer& inputLine, size_t& cursor)
{
    if(fmt.width == 0){
        return 4;
    }else{
        return fmt.width;
    }
}

RegisterConverter (LengthConverter, "l");
