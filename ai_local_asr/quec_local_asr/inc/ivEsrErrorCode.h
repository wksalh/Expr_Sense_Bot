/*----------------------------------------------+
|												|
|	ivESRErrorCode.h - Basic Definitions		|
|												|
|		Copyright (c) 1999-2007, iFLYTEK Ltd.	|
|		All rights reserved.					|
|												|
+----------------------------------------------*/

#ifndef IFLYTEK_VOICE__ESRERRORCODE__H
#define IFLYTEK_VOICE__ESRERRORCODE__H

#include "ivDefine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef	ivCStr		EsErrID;

#define	EsErrID_OK	 ivNull
#define EsErr_InvCal	((EsErrID)1)
#define EsErr_InvArg	((EsErrID)2)
#define EsErr_TellSize	((EsErrID)3)
#define EsErr_OutOfMemory	((EsErrID)4)
#define EsErr_InvProperty	((EsErrID)5)
#define EsErr_CmdObj	((EsErrID)6)
#define EsErr_BufferFull	((EsErrID)7)
#define EsErr_ASREnded	((EsErrID)8)
#define EsErr_Result	((EsErrID)9)
#define EsErr_ForceResult	((EsErrID)10)
#define EsErr_BufferEmpty	((EsErrID)11)
#define EsErr_ResultNone	((EsErrID)12)
#define EsErr_SetFailed	((EsErrID)13)
#define EsErr_ParamNotSupport	((EsErrID)14)
#define EsErr_ReEnter	((EsErrID)15)
#define EsErr_InvRes	((EsErrID)16)
#define EsErr_CmdCountExceeded	((EsErrID)17)
#define EsErr_SpeechStart		((EsErrID)18)
#define EsErr_ResponseTimeOut	((EsErrID)19)
#define EsErr_CmdTooLong		((EsErrID)20)
#define EsErr_Limited			((EsErrID)21)
#define EsErr_InsufficientWorkBuffer		((EsErrID)22)
#define EsErr_InsufficientResidentBuffer	((EsErrID)23)
#define EsErr_InvSyntax			((EsErrID)24)
#define EsErr_RecStartFail		((EsErrID)27)
#define EsErr_RecStopFail		((EsErrID)28)
#define EsErr_InvHeader			((EsErrID)29)
#define EsErr_InvName			((EsErrID)30)
#define EsErr_LexNotFound		((EsErrID)31)
#define EsErr_SceneNotFound		((EsErrID)32)
#define EsErr_BeginProcess		((EsErrID)33)
#define EsErr_OnlyEnergy		((EsErrID)34)

#ifdef __cplusplus
}
#endif

#endif /* !IFLYTEK_VOICE__ESRERRORCODE__H */
