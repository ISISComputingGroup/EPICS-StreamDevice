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
    void convertBytesBigEndian(size_t width, char *tempArray, const size_t length);
    void convertBytesLittleEndian(size_t width, char *tempArray, const size_t length);
    size_t convertBigLength(size_t width, StreamBuffer &inputLine);
    size_t convertLittleLength(size_t width, StreamBuffer &inputLine);
    ssize_t scanPseudo(const StreamFormat &fmt, StreamBuffer &inputLine, size_t &cursor);
    
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
    size_t width;
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
    for (size_t i = 0; i < length; i++){
        tempArray[i+width]=output[i];
    }
    output.set(tempArray, width+length);
    delete tempArray;
    return true;
}

void LengthConverter::convertBytesBigEndian(size_t width, char *tempArray, const size_t length)
{
    for (size_t i = 0; i < width; i++)
    {
        size_t shift = (width - 1 - i) * 8;
        tempArray[i] = (length >> (shift)) & 0xFF;
    }
}

void LengthConverter::convertBytesLittleEndian(size_t width, char *tempArray, const size_t length)
{
    for (size_t i = 0; i < width; i++)
    {
        size_t shift = i * 8;
        tempArray[i] = (length >> (shift)) & 0xFF;
    }
}

size_t LengthConverter::convertBigLength(size_t width, StreamBuffer &inputLine)
{
    size_t length=0, n;
    for (size_t i = 0; i < width; i++)
    {
        size_t shift = (width - 1 - i) * 8;
        n = int((unsigned char)(inputLine[i]) << shift);
        length += n;
    }
    return length;
}

size_t LengthConverter::convertLittleLength(size_t width, StreamBuffer &inputLine)
{
    size_t length=0, n;
    for (size_t i = 0; i < width; i++)
    {
        size_t shift = i * 8;
        n = int((unsigned char)(inputLine[i]) << shift);
        length += n;
    }
    return length;
}

ssize_t LengthConverter::
scanPseudo(const StreamFormat& fmt, StreamBuffer& inputLine, size_t& cursor)
{
    size_t width;
    if(fmt.width == 0){
        width = 4;
    }else{
        width = fmt.width;
    }
    size_t length;

    if (fmt.flags & alt_flag){
        length = convertLittleLength(width, inputLine);
    }else{
        length = convertBigLength(width, inputLine);
    }
    if( inputLine.length() -width == length){
        return width;
    }
    error("LengthConverter: expcted %u bytes but read %u.\n", length, inputLine.length()-width);
    return -1;
}


RegisterConverter (LengthConverter, "l");
