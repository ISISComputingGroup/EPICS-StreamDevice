/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* Sycom Multi Drop Protocol pseudo-converter                   *
* Pete Owens - December 2011                                   *
*                                                              *
* This pseudo converter pre-process raw input to substitute    *
* escape sequences:-                                           *
* ESC '0' (0x07,0x30) -> STX  (0x02)                           *
* ESC '1' (0x07,0x31) -> '\r' (0x0D)                           *
* ESC '2' (0x07,0x32) -> ESC  (0x07)                           *
* Apart from the first character of input (which is STX)       *
*                                                              *
* Checksum (last two chars) converted to standard hex format:- *
* ':' (0xA+0x30)-> 'A'                                         *
* ';' (0xB+0x30)-> 'B'                                         *
* '<' (0xC+0x30)-> 'C'                                         *
* '=' (0xD+0x30)-> 'D'                                         *
* '>' (0xE+0x30)-> 'E'                                         *
* '?' (0xF+0x30)-> 'F'                                         *
* so that normal <sum> converter will work                     *
***************************************************************/

#include "StreamFormatConverter.h"
#include "StreamError.h"
#if defined(__vxworks) || defined(vxWorks) || defined(_WIN32) || defined(__rtems__)
// These systems have no strncasecmp
#include <epicsString.h>
#define strncasecmp epicsStrnCaseCmp
#endif
#include <ctype.h>

typedef unsigned long ulong;
typedef unsigned char uchar;

// escape characters
const char STX = '\x02', ESC_STX = '0';
const char CR  = '\r'  , ESC_CR  = '1';
const char ESC = '\x07', ESC_ESC = '2';

class SmdpConverter : public StreamFormatConverter
{
    int parse (const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printPseudo(const StreamFormat&, StreamBuffer&);
    int scanPseudo(const StreamFormat&, StreamBuffer&, long& cursor);
};

int SmdpConverter::
parse(const StreamFormat&, StreamBuffer& info, const char*& source, bool)
{
    return pseudo_format;
}

bool SmdpConverter::
printPseudo(const StreamFormat& format, StreamBuffer& output)
{
    int n;
    int start = (format.width < 1) ? 1 : format.width;

// Insert escape sequences
    for (n = start; n < output.length(); n++)
        if (output[n] == STX || output[n] == CR || output[n] == ESC)
        {
            output.insert(n++, ESC);
            switch (output[n])
            {
                case STX: output.replace(n, 1, &ESC_STX, 1); break;
                case CR:  output.replace(n, 1, &ESC_CR,  1); break;
                case ESC: output.replace(n, 1, &ESC_ESC, 1); break;
            }
        }

// Convert Checksum
    char c;

    for (n = -2; n < 0; n++)
        if (output[n] > '9')
        {
            c = output[n] + ':' - 'A';
            output.replace(n, 1, &c, 1);
        }

    return true;
}

int SmdpConverter::
scanPseudo(const StreamFormat& format, StreamBuffer& input, long& cursor)
{
    int n;
    int start = (format.width < 1) ? 1 : format.width;

    if (cursor > start)
        start = cursor;

// Extract escape sequences
    for (n = start; (n = input.find(ESC, n)) > 0; n++)
        switch (input[n+1])
        {
            case ESC_STX: input.replace(n, 2, &STX, 1); break;
            case ESC_CR:  input.replace(n, 2, &CR,  1); break;
            case ESC_ESC: input.replace(n, 2, &ESC, 1); break;
            default:
                error("Unknown escape character: 0x%02X\n", input[n+1]);
                return -1;
        }

// Convert Checksum
    n = ((input[-2] - '0') << 4) + (input[-1] - '0');
    input.truncate(-2);
    input.printf("%02X", n);

// strip off checksum
    if (format.flags & skip_flag)
        input.truncate(-2);

    return 0;
}

RegisterConverter (SmdpConverter, "!");
