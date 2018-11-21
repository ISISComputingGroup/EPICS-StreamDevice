/***************************************************************
* Stream Device record interface for binary output records     *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is an EPICS record Interface for StreamDevice.          *
* Please refer to the HTML files in ../doc/ for a detailed     *
* documentation.                                               *
*                                                              *
* If you do any changes in this file, you are not allowed to   *
* redistribute it any more. If there is a bug or a missing     *
* feature, send me an email and/or your patch. If I accept     *
* your changes, they will go to the next release.              *
*                                                              *
* DISCLAIMER: If this software breaks something or harms       *
* someone, it's your problem.                                  *
*                                                              *
***************************************************************/

#include <string.h>
#include <busyRecord.h>
#include "devStream.h"
#include <epicsExport.h>
#include "devStream.h"

static long readData (dbCommon *record, format_t *format)
{
    busyRecord *br = (busyRecord *) record;
    unsigned long val;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            if (br->mask) val &= br->mask;
            br->rbv = val;
            if (INIT_RUN) br->rval = val;
            return OK;
        }
        case DBF_ENUM:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            br->val = (val != 0);
            return DO_NOT_CONVERT;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(br->znam)];
            if (streamScanfN (record, format, buffer, sizeof(buffer)))
                return ERROR;
            if (strcmp (br->znam, buffer) == 0)
            {
                br->val = 0;
                return DO_NOT_CONVERT;
            }
            if (strcmp (br->onam, buffer) == 0)
            {
                br->val = 1;
                return DO_NOT_CONVERT;
            }
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    busyRecord *br = (busyRecord *) record;
    long ret = ERROR;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        {
            ret = streamPrintf (record, format, br->rval);
        }
        case DBF_ENUM:
        {
            ret = streamPrintf (record, format, (long)br->val);
        }
        case DBF_STRING:
        {
            ret = streamPrintf (record, format,
                br->val ? br->onam : br->znam);
        }
    }
    printf("Called busyWritedata\n");
    if (ret != ERROR)
    {
        br->val = 0;
    }
    return ret;
}

static long initRecord (dbCommon *record)
{
    busyRecord *br = (busyRecord *) record;

    return streamInitRecord (record, &br->out, readData, writeData);
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devbusyStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devbusyStream);
