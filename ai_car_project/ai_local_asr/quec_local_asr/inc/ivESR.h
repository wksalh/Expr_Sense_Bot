/*****************************************************************
//  ivESR   version:  1.1   ? date: 2012/09/26
//  -------------------------------------------------------------
//  API
//  -------------------------------------------------------------
//  Copyright (C) 2012 - All Rights Reserved
// ***************************************************************
// 	YF Shan
// ***************************************************************/

#ifndef ES_TEAMM__2012_04_12__ESR__H
#define ES_TEAMM__2012_04_12__ESR__H

#include "ivEsrDefine.h"
#include "ivErrorCode.h"

/* Message ID description */

/* Audio Control */
#define ivMsg_ToStartAudioRec			(0x310)
#define ivMsg_ToStopAudioRec			(0x311)
/*----------------------------------------------------------------------------------
	Notify application to Start/Stop the audio record.
	Strongly recommend that you use this Audio Control mechanism,
	Parameter: NA
	Return: NA
==================================================================================*/

/* Recognize information and result */
#define ivMsg_SpeechStart					(0x401)
/*----------------------------------------------------------------------------------
	Notify application that the speech start Detected, the Speech maybe start 
	probably. As the auto VAD(voice-active-detection) is not Completely accurate, 
	this message may be sent several times in process of one recognizing task.
	Parameter: NA
	Return: NA
==================================================================================*/
#define ivMsg_SpeechEnd						(0x402)
/*----------------------------------------------------------------------------------
	Notify application that the Speech End Detected.
	Parameter: NA
	Return: NA
==================================================================================*/
#define ivMsg_SpeechFlushEnd			(0x403)
/*----------------------------------------------------------------------------------
	Notify application that the Speech ended because EsrEndAudioData called while
	the ESR object had NOT detected speech end, The	Speech start must be Detected
	previously already. If speech end was detected firstly, ivMsg_SpeechEnd will be
	send no matter whether EsrEndAudioData was called.
	Parameter: NA
	Return: NA
==================================================================================*/
#define ivMsg_NoSpeechDetected		(0x40f)
/*----------------------------------------------------------------------------------
	Notify application that the ESR object has NOT detected any speech while 
	the application called EsrEndAudioData.
	Parameter: NA
	Return: NA
==================================================================================*/
#define ivMsg_ResponseTimeout			(0x410)
/*----------------------------------------------------------------------------------
	Notify application that EsrObject can not detect speech start after run over
	the agreed Response-Time,
	Parameter: NA
	Return:
		ivErr_Reset	: Ignore this message and Re-timing.
		Any Other	: The Process was failed and over!
==================================================================================*/
#define ivMsg_SpeechTimeout				(0x411)
/*----------------------------------------------------------------------------------
	Notify application that the speech run over the agreed Time before EsrObject
	produce some result.
	Parameter:
	wParam : [as ivBool] Indicate whether can force produce the recognized result.
	lParam : NA
	return:
		ivErr_OK:	 Try to force produce the recognized result if possible, 
		ivErr_FALSE: The process was failed and over!
==================================================================================*/
#define ivMsg_Result							(0x500)
/*----------------------------------------------------------------------------------
	Notify application that user ended the process before EsrObject produce some result.
	Parameter: NA
	Return: NA
==================================================================================*/
#define ivMsg_RecognizeEnd                      (0x502)
/*----------------------------------------------------------------------------------
	Notify application that one recognize produce is over and maybe has no result.
	Parameter: NA
	Return: NA
==================================================================================*/

#define ivMsg_QuickPartResult                      (0x503)
/*----------------------------------------------------------------------------------
	Notify application that one recognize produce is over and maybe has no result.
	Parameter: NA
	Return: NA
==================================================================================*/

/* kernel  state */
#define ivMsg_DestroyWaiting                    (0x603)
/*----------------------------------------------------------------------------------
	Notify application that the EsrDestroy has been processing, but EsrObject is busy 
	to be destroyed right now.
	Parameter: NA
	Return: NA
==================================================================================*/
#define ivMsg_GRMDestroyWaiting                 (0x604)
/*----------------------------------------------------------------------------------
	Notify application that the GrmDestroy has been processing, but GrmObject is busy 
	to be destroyed right now.
	Parameter: NA
	Return: NA
==================================================================================*/
#define ivMsg_EsrEngineState                    (0x605)
/*----------------------------------------------------------------------------------
	Notify application that the EsrStart or EsrStop has been processed.
	Parameter:
	wParam : [as ivStatus] the return state of kernel process.
	lParam : [as ivCStrW] the case of EsrStart: the recognize scene or 
	the case of EsrStop: ivNull.
	Return: NA
==================================================================================*/

/* Grammar series*/
#define ivMsg_GRMNetWorkBuilt                   (0x606)
/*----------------------------------------------------------------------------------
	Notify application that the bnf grammar file has been compiled to WNet in grammar 
	workspace and can be used to recognize after moving to recognize workspace by 
	API EsrCommitNetWork.
	Parameter:
	wParam : NA.
	lParam : [as ivCStrW] the grammar name.
	Return: NA
==================================================================================*/
#define ivMsg_CopyGrmRes                        (0x607)
/*----------------------------------------------------------------------------------
	Notify application that the copying action in CommitNetWork is done.
	Parameter:
	wParam : NA.
	lParam : [as ivCStrW] the copied file name.
	Return: NA
==================================================================================*/
#define ivMsg_CommitNetWork                     (0x608)
/*----------------------------------------------------------------------------------
	Notify application that the request of moving WNet from grammar workspace to 
	recognize workspace is processed.
	Parameter:
	wParam : [as ivStatus] the return state of kernel process..
	lParam : [as ivCStrW] the WNet Name which is committed.
	Return: NA
==================================================================================*/

typedef struct tagWordItem{
	ivUInt32	nID;
	ivCStrW		pText;
}TWordItem, ivPtr PWordItem;

typedef struct tagSlotInfo{
	ivCharW		szName[16];
	ivUInt32	iSlotType;
	ivInt32	nConfidenceScore;		/* Reserved */
	ivUInt32	nItem;
	PWordItem	pItems;
}TSlotInfo,ivPtr PSlotInfo;

typedef struct tagEsrResult{
	ivUInt32	iSyntaxID;						/* Sentence ID */
	ivInt32	nConfidenceScore;					/* Score of Confidence 1-100 */
	ivUInt32	nSlot;							/* Slot count of this sentence */
	PSlotInfo	pSlots;							/* Recognize result array of the slot list */
	ivInt32     nStartFrame;
	ivInt32     nEndFrame;
}TEsrResult, ivPtr PEsrResult;


/*----------------------------------------------------------------------------------
	Notify application that EsrObject produced some result or refuse.
	EsrObject output result by this message. When some results were produced,
	the wParam indicate the count of probable result, and lParam point to the
	array of probable result information. The Array is decreasing ordered by the
	member nConfidenceScore.
	However, if the EsrObject detected the speech is over, but can NOT produce
	any	result (that is, Refused), This message will still be sent, wParam will
	be zero	and lParam will be ivNull.
	Parameter: 
		wParam : [as ivUInt32] N-Best count
		lParam : [as PCEsrResult] The Array of the result information structures.
				The content of this pointer keep valid in 30000 ms or before next 
				recognize task start.
	return: NA
==================================================================================*/

#define SAMPLERATE16K_CHECK         (160)
#define SAMPLERATE8K_CHECK          (80)

/* Parameter ID for Samplerate */
#define ES_PARAM_SAMPLERATE				(101)
	#define ES_SAMPLERATE_16K				((ivCPointer)16000)
	#define ES_SAMPLERATE_8K				((ivCPointer)8000)

/* Parameter ID for Discard (millisecond as unit)*/
#define ES_PARAM_DISCARD				(102)
	#define ES_DEFAULT_DISCARD			((ivCPointer)0)

/* Parameter ID for VAD */
#define ES_PARAM_VAD					(103)
	#define ES_PARAM_VAD_ON				((ivCPointer)1)
	#define ES_PARAM_VAD_OFF			((ivCPointer)0)
	#define ES_PARAM_VAD_DEFAULT		ES_PARAM_VAD_ON

/* Parameter ID for AcModel Scaling */
#define ES_PARAM_ACSCALE				(104)

/* Parameter ID for Pre Prune */
#define ES_PARAM_PREPRUNE				(105)
		#define ES_PARAM_PREPRUNE_ON		((ivCPointer)1)
		#define ES_PARAM_PREPRUNE_OFF		((ivCPointer)0)

/* Parameter ID for Beam and Threshold */
#define ES_PARAM_BEAM_THRESHOLD				(106)
#define ES_PARAM_HISTOGRAM_THRESHOLD		(107)

/* Parameter ID for shortpause */
#define ES_PARAM_SHORTPAUSE					(108)
		#define ES_PARAM_SHORTPAUSE_ON		((ivCPointer)1)
		#define ES_PARAM_SHORTPAUSE_OFF		((ivCPointer)0)
		
/* Parameter ID for progressive */
#define ES_PARAM_PROGRESSIVE				(109)
		#define ES_PARAM_PROGRESSIVE_ON		((ivCPointer)1)
		#define ES_PARAM_PROGRESSIVE_OFF	((ivCPointer)0)
		
/* Parameter ID for ESR VAD Speech End 语音说完，多久认为语音结束*/
#define ES_PARAM_SPEECHEND			(110)
		#define ES_VAD_SPEECHEND_ID		(2)
		#define ES_MIN_SPEECHEND		((ivCPointer)650)		/* In milliseconds */
		#define ES_MAX_SPEECHEND		((ivCPointer)5000)
		#define ES_COMMEND_SPEECHEND	((ivCPointer)1200)		/* In milliseconds */
		#define ES_DEFAULT_SPEECHEND	ES_COMMEND_SPEECHEND
		
/* Parameter ID for ESR VAD ShortPause Length */
#define ES_PARAM_SHORTPAUSE_END			(111)
		#define ES_VAD_SHORTPAUSEEND_ID		(1)
		#define ES_MIN_SHORTPAUSE_SPEECHEND		((ivCPointer)200)		/* In milliseconds */
		#define ES_MAX_SHORTPAUSE_SPEECHEND		((ivCPointer)600)
		#define ES_COMMEND_SHORTPAUSE_SPEECHEND	((ivCPointer)400)		/* In milliseconds */

/* Parameter ID for sentence full stop */
#define ES_PARAM_SENTENCE_END			(112)		
		#define SENTENCE_END_NOTHING	((ivCPointer)0)
		#define SENTENCE_END_FULL_STOP	((ivCPointer)2)

/* Parameter ID for speech timeout result */
#define ES_PARAM_SPEECHTIMEOUT_RESULT			(113)		
		#define SPEECHTIMEOUT_HAS_RESULT	((ivCPointer)0)
		#define SPEECHTIMEOUT_NO_RESULT		((ivCPointer)1)

/* Parameter ID for ESR Response Timeout 说完以后，识别的时间有多长*/
#define ES_PARAM_RESPONSETIMEOUT		(2)
	#define ES_MIN_RESPONSETIMEOUT		((ivCPointer)1000)		/* In milliseconds */
	#define ES_MAX_RESPONSETIMEOUT		((ivCPointer)50000)
	#define ES_COMMEND_RESPONSETIMEOUT	((ivCPointer)3000)		/* In milliseconds */
	#define ES_DEFAULT_RESPONSETIMEOUT	ES_COMMEND_RESPONSETIMEOUT

/* Parameter ID for ESR Speech Timeout  说话时间有多长*/
#define ES_PARAM_SPEECHTIMEOUT			(3)
	#define ES_MIN_SPEECHETIMEOUT		((ivCPointer)3000)		/* In milliseconds */
	#define ES_MAX_SPEECHETIMEOUT		((ivCPointer)20000)
	#define ES_COMMEND_SPEECHETIMEOUT	((ivCPointer)4000)		/* In milliseconds */
	#define ES_DEFAULT_SPEECHETIMEOUT	ES_COMMEND_SPEECHETIMEOUT

/* Parameter ID for ESR Disable CM */
#define ES_PARAM_CM				(7)
#define ES_CM_ON			((ivCPointer)1)
#define ES_CM_OFF			((ivCPointer)0)
#define ES_DEFAULT_CM		ES_CM_ON 

/* Parameter ID for gauss selection */
#define ES_PARAM_GS				(10)
#define ES_GS_ON				((ivCPointer)1)
#define ES_GS_OFF				((ivCPointer)0)
#define ES_DEFAULT_GS			((ivCPointer)0)

/* Parameter ID for two pass */
#define ES_PARAM_DENOISE		(11)
#define ES_DENOISE_ON			((ivCPointer)1)
#define ES_DENOISE_OFF			((ivCPointer)0)
#define ES_DEFAULT_DENOISE		((ivCPointer)0)

/* Parameter ID for ESR Sensitivity */
#define ES_CM_THRESHOLD			(1)
#define CM_THRESHOLD_DEFAULT			((ivCPointer)0)

#define ivESR_CP_GBK					936		/* GBK (default) */
#define ivESR_CP_BIG5					950		/* Big5 */
#define ivESR_CP_UTF16LE			1200	/* UTF-16 little-endian */
#define ivESR_CP_UTF16BE			1201	/* UTF-16 big-endian */
#define ivESR_CP_UTF8					65001	/* UTF-8 */
#define ivESR_CP_GB18030			ivESR_CP_GBK
#define ivESR_CP_UNICODE			ivESR_CP_UTF16LE
#define ivESR_CP_UNICODEBE		ivESR_CP_UTF16BE
#define ivESR_CP_DEFAULT			ivESR_CP_UTF16LE

/*
*	Interface
*/
#define  GrmCreate            IAT500f2ce01685054bfd90899c4af0dd7812
#define  GrmDestroy           IAT50ff3a47b0034e45c0a9691d0968449c7e
#define  GrmBnfParser         IAT502e12edda1ede47fb83141f34d310d24b
#define  GrmBuildNetWork      IAT502c305192c79e4ab790034185ee480b7f
#define  GrmDictBegin         IAT50008834303b10484cb01068ba515f74b3
#define  GrmDictEnd           IAT50a6909816a2ec40a39646d094040b7524
#define  GrmDictInsertItem    IAT509690b0f75cfc4f42885b488b9711f201

#define  EsrCreate            IAT505c9a44e85e264df98783b89e8770f5f7
#define  EsrDestroy           IAT50aef9e0eddccf4fcaa64c1f03e9b462e4
#define  EsrRunService        IAT50c912c5127b18470893f260e34ee90bb3
#define  EsrExitService       IAT5031acb3cedd6844dab32c78bff8116322
#define  EsrStart             IAT50235a201ead2f45dfbbf145e456925788
#define  EsrStop              IAT50f6c290ffc5e84124aff1f9bc6aab7cfa


#define  EsrCommitNetWork     IAT50f751fbe379944a0580d4f64883e2cfa3
#define  EsrCommitNetWorkOffline IAT50f751fbe379944a0580d4f64883e2cfa4
#define  EsrLoadNetWork       IAT50590bf29b1871447c9f4bb13464cccba4
#define  EsrUnLoadNetWork     IAT50f2550b232c434b7abb87bf8b199e5577

#define  EsrAppendAudioData   IAT50c5d4225f588f42e4af59e507d3636930
#define  EsrEndAudioData      IAT50566b3d6d51fe4da986d8017480df507e

#define  EsrSetParam          IAT509939d2c89c7d45f984e3c5ec2e361b6c
#define  EsrRecognize         IAT5067e42e0adb4c498dabc6f1755c3ce387

#define  EsrRecognizeFeature  IAT5067e42e0adb4c458dabc621755c3ce38f

#define  EsrAToU              IAT509A0F8329E5AE43bd9367B7DED1A00297
#define  GrmAToU              IAT507E72D96A77F741a18CFA3D42F9B3E784



#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Create ESR  Grm object */
ivStatus                                /* Returned Error Info */
ivCall GrmCreate(
        ivHandle ivPtr  phEsrGrmObj,    /* [Out] To Receive the ESR Grm object handle */
        ivPCUserOS      pUserOS         /* [In] Describe the user's OS and Interface */
     );

/* Destroy ESR  Grm object */
ivStatus                                /* Returned Error Info */
ivCall GrmDestroy(
        ivHandle        hEsrGrmObj      /* The ESR Grm object handle */
     );

/* Build Grammar from text*/
ivStatus                                /* Returned Error Info */
ivCall GrmBnfParser(
        ivHandle        hEsrGrmObj,     /* The ESR Grm object handle */
        ivPCByte        lpText,         /* A bnf-format text buffer*/
        ivSize          len,            /* the length of the buffer */
		ivStrW          pGrmName
     );
 
/* Update Grammar to NetWork*/
ivStatus                                /* Returned Error Info */
ivCall GrmBuildNetWork(
        ivHandle        hEsrGrmObj,     /* The ESR Grm object handle */
        ivCStrW         pGrmName,       /* Name of grammar, Grammar must be builded previously */
        ivByte          nSamleRateCheck /* The SampleRate the Out Grammar want be used to */
     );

/* Begin of Create a Dict*/
ivStatus                                /* Returned Error Info */
ivCall GrmDictBegin(
        ivHandle        hEsrGrmObj,     /* The ESR Grm object handle */
        ivCStrW         pDictName,       /* Name of Dict */
		ivInt			DictType
     );

/* End of Create a Dict*/
ivStatus                                /* Returned Error Info */
ivCall GrmDictEnd(
        ivHandle        hEsrGrmObj      /* The ESR Grm object handle */
      );

/* Insert Item to a Dict*/
ivStatus                                /* Returned Error Info */
ivCall GrmDictInsertItem(
        ivHandle        hEsrGrmObj,     /* The ESR Grm object handle */
        ivCStrW         pItemText,          /* Text of the Item to be Inserted */
        ivInt32         nID             /* ID of the Item */
     );

/* Create ESR  Rec object */

#ifdef VOLICOPEN
ivStatus								/* Returned Error Info */
ivCall EsrCreate(
		ivHandle ivPtr phEsrObj,		/* [Out] To Receive the ESR Rec object handle */
		ivPCUserOS pUserOS,				/* [In] Describe the user's OS and Interface */
		ivCStrA szKey					/* [In] volic keyword */
		);
#else
ivStatus								/* Returned Error Info */
ivCall EsrCreate(
		ivHandle ivPtr phEsrObj,		/* [Out] To Receive the ESR Rec object handle */
		ivPCUserOS pUserOS				/* [In] Describe the user's OS and Interface */
		);
#endif

/* Destroy ESR Rec object */
ivStatus 
ivCall EsrDestroy(
        ivHandle        hEsrRecObj      /* The ESR Rec object handle */
     );

/* Do all the operation, It Occupies a thread exclusively */
ivStatus                                /* Returned Error Info */
ivCall EsrRunService(
        ivHandle        hEsrRecObj      /* The ESR Rec object handle */
     );

/* Notify the run Service to terminate when finished current recognize 
   process (if there is), The ESR Rec object must be in run-Service State 
   That is, EsrRecRunService must be called previously */
ivStatus                                /* Returned Error Info */
ivCall EsrExitService(
        ivHandle        hEsrRecObj      /* The ESR Rec object handle */
     );
     
/* Start one process of recognition */
ivStatus                                /* Returned Error Info */
ivCall EsrStart(
        ivHandle        hEsrRecObj,     /* The ESR Rec object handle */
        ivCStrW         lpGrmrName      /* Name of grammar, Grammar must be updated to network previously */
     );

/* Stop one process of recognition */
ivStatus                                /* Returned Error Info */
ivCall EsrStop(
        ivHandle        hEsrRecObj      /* The ESR Rec object handle */
     );
     
/* Append Audio data to the ESR object,In general, Call this function in record thread */
ivStatus                                                    /* Returned Error Info */
ivCall EsrAppendAudioData(
        ivHandle        hEsrRecObj,     /* The ESR Rec object handle */
        ivPInt16        pAudioData,     /* [In] Input Audio data buffer */
        ivSize          nSamples        /* Specifies the length, in samples, of Audio data */
     );

/* Tell ESR object that there's no more data, If you control record end Timing
   by yourself, Call this function after stop audio record is recommended. */
ivStatus                                /* Returned Error Info */
ivCall EsrEndAudioData(
        ivHandle        hEsrRecObj      /* The ESR Rec object handle */
     );
     
/* Commit NetWork to WorkSpace */
ivStatus                                /* Returned Error Info */
ivCall EsrCommitNetWork(
        ivHandle        hEsrRecObj,     /* The ESR Grm object handle */
        ivCStrW         pGrmName,       /* Name of NetWork, NetWork must be builded previously */
        ivByte          nSamleRateCheck /* The SampleRate of the NetWork want to be Commited */
     );

/* Commit NetWork to WorkSpace in Offline Mode*/
ivStatus                                /* Returned Error Info */
ivCall EsrCommitNetWorkOffline(
						ivHandle        hEsrRecObj,     /* The ESR Grm object handle */
						ivCStrW         pGrmName,       /* Name of NetWork, NetWork must be builded previously */
						ivByte          nSamleRateCheck /* The SampleRate of the NetWork want to be Commited */
						);
/* Load Net for ESR Rec */
ivStatus                                /* Returned Error Info */
ivCall EsrLoadNetWork(
        ivHandle        hEsrRecObj,     /* The ESR Rec object handle */
        ivCStrW         lpGrmrName      /* Name of grammar */
     );

/*  UnLoad Net for ESR Rec */
ivStatus                                /* Returned Error Info */
ivCall EsrUnLoadNetWork(
        ivHandle        hEsrRecObj,     /* The ESR Rec object handle */
        ivCStrW         lpGrmrName      /* Name of grammar */
     );

/* Recognize synchronous */
ivStatus                                                /* Returned Error Info */
ivCall EsrRecognize(
        ivHandle        hEsrRecObj,     /* The ESR Rec object handle */
        ivCStrW         lpGrmrName,     /* Name of grammar, Grammar must be updated to network previously */
        ivPInt16        pAudioData,     /* [In] Input Audio data buffer */
        ivSize          nSamples,       /* Specifies the length, in samples, of Audio data */
        ivPUInt32       pnBest,         /* [Out] To Receive Recognize result nBest Count */
        PEsrResult ivPtr ppResult        /* [Out] To Receive Recognize result */
     );

/* Recognize from feature */
ivStatus                                                /* Returned Error Info */
ivCall EsrRecognizeFeature(
		ivHandle        hEsrRecObj,     /* The ESR Rec object handle */
		ivCStrW         lpGrmrName,     /* Name of grammar, Grammar must be updated to network previously */
		ivPInt16		FeatureInput,	/* the feature input */
		ivInt16			RecStatus,       /* if the feature is the first(1)/middle(0)/last(-1) frame */
		ivPUInt32       pnBest,         /* [Out] To Receive Recognize result nBest Count */
		PEsrResult ivPtr ppResult        /* [Out] To Receive Recognize result */
		);

/* Set ESR object parameter */
ivStatus                                                /* Returned Error Info */
ivCall EsrSetParam(
        ivHandle        hEsrRecObj,     /* The ESR Rec object handle */
        ivUInt32        nParamID,       /* Parameter ID */
        ivCPointer      nParamValue     /* Parameter Value */
     );

ivStatus ivCall GrmAToU(ivHandle hGrmObj, ivStrW pBuffer, ivPUInt8 pnBufSize, ivCStrA pStrA, ivUInt16 nEncoding);
ivStatus ivCall EsrAToU(ivHandle hEsrObj, ivStrW pBuffer, ivPUInt8 pnBufSize, ivCStrA pStrA, ivUInt16 nEncoding);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !defined(ESR_TEAM__2007_08_08__ESR__H) */
