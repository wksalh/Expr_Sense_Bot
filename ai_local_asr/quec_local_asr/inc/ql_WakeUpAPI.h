#ifndef __QL_WAKEUPAPI_H__
#define __QL_WAKEUPAPI_H__
#include <stdio.h>
#include "ivPlatform.h"

#define BNFFILE ("default.bnf")
#define RECOGNITION_SCENE ("default") 
#define REC_SAMPLERATE (160)
#define BUILDGRAMMAR (1)
#define GRAMMAR_NAME ("default")
#define BUILD_SAMPLERATE (160)

#define ONCE_APPEND_SIZE (5120)
#define APPEND_PERIOD (2)      /* In Milli-second */
#define ENDDATA_WAITTIME (200) /* In Milli-second */

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define WRITE_IATFILE (1)
#define TOOl_VERSION ("1.01")

#define TIMELIMIT 360000

typedef void* (*callBackOpenFile)(
    void *pUser,
    const signed char *lpFileName,
    int enMod,
    int enType
);

typedef int (*callBackMSG)(
    void *pUser,
    void *hObj,          /* Handle to the object */
    unsigned int uMsg,   /* Message ID, Specifies the message */
    unsigned int wParam, /* Specifies additional message information.The contents of this parameter depend on the value of the uMsg parameter */
    const void *lParam   /* Specifies additional message information.The contents of this parameter depend on the value of the uMsg parameter */
);
typedef struct ql_WakeUpHandle
{
    /* data */
    void *hEsrHandle;
    void *hGrmHandle;
    void *pUserOS;
    callBackMSG pCallBackMSG;
    callBackOpenFile pCallBackOpenFile;
    char bnfFile[MAX_PATH] ;
    int nlimit;
}ql_WakeUpInterface, *pql_WakeUpInterface;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int ql_WakeUp_Create(pql_WakeUpInterface pInterface);

int ql_WakeUp_Destroy(pql_WakeUpInterface pInterface);

int ql_WakeUp_Start(pql_WakeUpInterface pInterface);

int ql_WakeUp_Stop(pql_WakeUpInterface pInterface);

int ql_WakeUp_AppendAudioData(pql_WakeUpInterface pInterface, short *pData, int nDataLen);

int ql_WakeUp_EndAudioData(pql_WakeUpInterface pInterface);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 