#include <stdio.h>
#include <string>
/*
 *  引入引擎头文件
 */
#include "ivw/wivw_api.h"
#include "ivw/ivw_err.h"

 /*
  *  Demo代码中依赖的头文件，主要用字符串拆分、异常检查
  */
#include "IvwWrapper.h"
#include "stringUtil.h"
#include "trans_code.h" 
#include "defines.h"
#include "errorCode.h"
#include "DynmaicLibLoader.h"
#include <semaphore>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <thread>
#include <semaphore.h>
#include <new>
#include <limits>
#include "ai_audio_play.h"
#include "ai_audio.h"
#include "ai_log.h"

/*
 *	检查管理类和唤醒实例的创建状态。对管理类和唤醒实例的存在性进行检查，非常有必要。
 */
#define CHECK_MANGER_IS_EXIST(magager)	CHECK_PTR(magager,	true,	CODE_IVW_MANGER_NOT_EXIST,	"管理类未创建，无法执行操作\n")
#define CHECK_MANGER_NOT_EXIST(magager) CHECK_PTR(magager,	false,	CODE_IVW_MANGER_RECREATE,	"管理类已创建，请勿重复创建\n")
#define CHECK_INST_IS_EXIST(inst)		CHECK_PTR(inst,		true,	CODE_IVW_INST_NOT_EXIST,	"唤醒实例未创建，无法执行操作\n")
#define CHECK_INST_NOT_EXIST(inst)		CHECK_PTR(inst,		false,	CODE_IVW_INST_RECREATE,		"唤醒实例已创建，请勿重复创建\n")


static int kws_tag=0;
static int save_file_flag=0;
static int add_audio_tag=0;

ivInt IvwCallBackWakeup(void *param, const ivChar* pUserParam)
{
	IvwWrapper *wapper = (IvwWrapper *)param;
	wapper->procIvwCallback(pUserParam);
	return CODE_SUCCESS;
}

/*
 * 启动唤醒任务需要的最少资源组合：IVW_MLP（神经网络资源）、IVW_FILLER（filler网络资源）、IVW_KEYWORD（唤醒词资源）
 * 
 * 其中id可以定义为不重复的int值，但是类型必须是IVW_MLP、IVW_FILLER和IVW_KEYWORD
 *
 * 同一个类型的资源可以通过不同的id去区分开。比如{3, "IVW_KEYWORD"} 和 {4, "IVW_KEYWORD"}可以标识两个不同的keyword资源
 */
IVW_RES_SET IvwWrapper::ivwInstResSet[3] = { { 1, "IVW_MLP" } ,{ 2, "IVW_FILLER" } ,{ 3, "IVW_KEYWORD" }};

IvwWrapper::IvwWrapper()
{
	pMgr_				= NULL;

	pIvwInst_			= NULL;
	bIvwInstStart_		= false;

	hdll_				= NULL;
	wIvwInitialize_		= NULL;
	wIvwUninitialize_	= NULL;

	pFile_				= NULL;
	kws_audio_buffer    = NULL;
	kws_ops             = NULL;
}


IvwWrapper::~IvwWrapper()
{
	quec_ai_log(LOG_AI_INFO,"kws class destroy\n");
	kws_exit_flag=false;
	if (kws_audio_thread.joinable()) {
		kws_audio_thread.join();
	}
	// if (playaudio_thread.joinable()) {
	// 	playaudio_thread.join();
	// }
	if (pFile_)
	{
		fclose(pFile_);
		pFile_ = NULL;
	}
	if(kws_audio_buffer)
	{
		ringbuffer_destroy(kws_audio_buffer);
	}
	destroyIvwInst();
	destoryAllRes();
	uninitIvwManager();
	freeLibrary();

}

/*
 * 功能:	加载引擎库
 *
 * 参数:	pDllPath  引擎库文件路径，为空时表示使用默认路径
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::loadLibrary(_In_ const ivChar* pDllPath)
{
	if (hdll_ != NULL)
	{
		quec_ai_log(LOG_AI_INFO,"引擎库已成功加载，不需要重复加载\n");
		return CODE_SUCCESS;
	}

	if (pDllPath == NULL || strlen(pDllPath) == 0)
	{
		quec_ai_log(LOG_AI_INFO,"引擎库路径为空，请设置正确的引擎库路径\n");
		return CODE_PARAM_IS_NULL;
	}

	hdll_ = DynmaicLibLoader::loadLibrary(pDllPath);
	if (hdll_ == NULL)
	{
		quec_ai_log(LOG_AI_INFO,"open engine lib failed\n");
		return CODE_IVW_DLL_LOAD_FAIL;
	}

	/* 
	 * 获取唤醒功能初始化函数地址
	 */
	wIvwInitialize_ = (Proc_wIvwInitialize)DynmaicLibLoader::getProcAddress(hdll_, "wIvwInitialize");
	/*
	 * 获取唤醒功能逆初始化函数地址
	 */
	wIvwUninitialize_ = (Proc_wIvwUninitialize)DynmaicLibLoader::getProcAddress(hdll_, "wIvwUninitialize");

	if (wIvwInitialize_ == NULL || wIvwUninitialize_ == NULL)
	{
		quec_ai_log(LOG_AI_INFO,"engine lib symbol lookup failed\n");
		wIvwInitialize_ = NULL;
		wIvwUninitialize_ = NULL;
		freeLibrary();
		return CODE_IVW_DLL_LOAD_FAIL;
	}

	quec_ai_log(LOG_AI_INFO,"engine lib load success\n");

	return CODE_SUCCESS;
}

/*
 * 功能:	释放引擎库
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::freeLibrary()
{
	if (hdll_ == NULL)
	{
		quec_ai_log(LOG_AI_INFO,"引擎库未成功加载，不需要释放\n");
		return CODE_SUCCESS;
	}

	DynmaicLibLoader::freeLibrary(hdll_);

	hdll_ = NULL;

	quec_ai_log(LOG_AI_INFO,"engine lib free success\n");
	return CODE_SUCCESS;
}

/*
 * 功能:	初始化引擎管理类，设置进程级参数
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::initIvwManager()
{
	CHECK_MANGER_NOT_EXIST(pMgr_);

	ivInt ret = CODE_SUCCESS;

	ret = wIvwInitialize_(&pMgr_, NULL);
	/*
	 * 检查初始化的结果，每次调用引擎接口，一定要检查引擎结果是否正常
	 */
	CHECK_IVW_RET(ret, wIvwInitialize_);

	quec_ai_log(LOG_AI_INFO,"wIvwInitialize success\n");

	/*
	 * 0: 只使用唤醒, 1:使用唤醒+声纹, 2:只使用声纹模式 3:只使用声纹中的年龄和性别
	 *
	 * 不设置时默认为0
	 */
	ret = pMgr_->wIvwSetParam("wivw_param_mode", "0");
	CHECK_IVW_RET(ret, wIvwSetParam_);

	/* 
	 * 使用CNN_FIX_SHUFFLEV2类型的mlp，此选项值需要与使用的mlp模型匹配 
	 * 0:FLOAT, 2:CHAR, 3:SPARSE_CHAR, 4:CNN_FLOAT, 5:CNN_FIX, 6:FLOAT_SHUFFLE, 7:FLOAT_SHUFFLEV2, 8:FIX_SHUFFLEV2
	 */
	ret = pMgr_->wIvwSetParam("wmlp_param_mlp_type", "8");
	CHECK_IVW_RET(ret, wIvwSetParam_);


	return ret;
}

/*
 * 功能:	销毁引擎管理类
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::uninitIvwManager()
{
	CHECK_MANGER_IS_EXIST(pMgr_);

	ivInt ret = wIvwUninitialize_(pMgr_);
	CHECK_IVW_RET(ret, wIvwUninitialize_);
	pMgr_ = NULL;

	quec_ai_log(LOG_AI_INFO,"wIvwUninitialize success\n");

	return CODE_SUCCESS;
}

/*
 *  功能:	 加载唤醒功能需要的所有资源
 *
 *  参数:	 pMlpResPath			IVW_MLP资源文件路径
 *           pFillerResPath		IVW_FILLER资源文件路径
 *           pKeywordResPath		IVW_KEYWORD资源文件路径
 *
 *  返回值:  0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::loadAllRes(_In_ const ivChar *pMlpResPath, _In_ const ivChar *pFillerResPath, _In_ const ivChar *pKeywordResPath)
{
	CHECK_MANGER_IS_EXIST(pMgr_);

	ivInt ret = CODE_SUCCESS;
	ivChar *pResBuf = NULL;
	ivUInt resSize = 0;

	/*
	 *  加载IVW_MLP资源
	 *
	 *  此处Demo里面将IVW_MLP资源的标识信息放到了ivwInstResSet[0]
     *
     *  备注：引擎采用了资源与实例分离的设计理念。即资源加载到唤醒引擎里面后，只存在于管理类（pMgr）中，不与任何实例（pIvwInst）相关联
     *
     *  每个实例可根据自身需要从管理类已加载的资源中选取不同的资源使用，具体方式是通过实例的wIvwStart接口传入，详见wIvwStart接口调用处
	 */
	ret = loadResFileIntoMemory(pMlpResPath, &pResBuf, &resSize);
	CHECK_FUNC_RET(ret, loadResFileIntoMemory);
	ret = pMgr_->wIvwResourceAdd(ivwInstResSet[0], pResBuf, IVW_RES_LOCATION_MEM, resSize);
	/*
	 * 资源加载到唤醒模块后，可以立即清除资源，不需要长期保存在内存中
	 */
	delete[] pResBuf;
	CHECK_FUNC_RET(ret, wIvwResourceAdd);

	/*
	 * 加载IVW_FILLER资源
	 *
	 * 此处Demo里面将IVW_FILLER资源的标识信息放到了ivwInstResSet[1]
	 */
	ret = loadResFileIntoMemory(pFillerResPath, &pResBuf, &resSize);
	CHECK_FUNC_RET(ret, loadResFileIntoMemory);
	ret = pMgr_->wIvwResourceAdd(ivwInstResSet[1], pResBuf, IVW_RES_LOCATION_MEM, resSize);
	delete[] pResBuf;
	CHECK_FUNC_RET(ret, wIvwResourceAdd);

	/*
	 * 加载IVW_KEYWORD资源
	 *
	 * 此处Demo里面将IVW_KEYWORD资源的标识信息放到了ivwInstResSet[2]
	 */
	ret = loadResFileIntoMemory(pKeywordResPath, &pResBuf, &resSize);
	CHECK_FUNC_RET(ret, loadResFileIntoMemory);
	ret = pMgr_->wIvwResourceAdd(ivwInstResSet[2], pResBuf, IVW_RES_LOCATION_MEM, resSize);
	delete[] pResBuf;
	CHECK_FUNC_RET(ret, wIvwResourceAdd);

	return CODE_SUCCESS;
}

/*
 * 功能:	释放所有资源
 *
 * 返回值:  0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::destoryAllRes()
{
	CHECK_MANGER_IS_EXIST(pMgr_);

	ivInt ret = 0;
	for (int i = 0; i < static_cast<int>(sizeof(ivwInstResSet) /sizeof(IVW_RES_SET)); i++)
	{
		ret = pMgr_->wIvwResourceDelete(ivwInstResSet[i]);
		CHECK_IVW_RET(ret, wIvwResourceDelete_);
	}

	quec_ai_log(LOG_AI_INFO,"destoryAllRes success\n");

	return CODE_SUCCESS;
}

/*
 * 功能:	创建唤醒实例，设置唤醒实例的回调函数
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::createIvwInstAndSetCallback()
{
	CHECK_INST_NOT_EXIST(pIvwInst_);
	CHECK_MANGER_IS_EXIST(pMgr_);

	ivInt ret = CODE_SUCCESS;
	ret = pMgr_->wIvwCreate(&pIvwInst_);
	CHECK_IVW_RET(ret, wIvwCreate_);

	quec_ai_log(LOG_AI_INFO,"wIvwCreate success\n");

	/*
	 * 设置唤醒实例的回调函数
	 */
	const ivChar * fun_name = "func_wake_up";
	void * callback_param = this;
	ret = pIvwInst_->wIvwRegisterCallBacks(fun_name, IvwCallBackWakeup, (void*)callback_param);
	CHECK_IVW_RET(ret, wIvwRegisterCallBacks_);

	std::cout << "wIvwRegisterCallBacks success," << static_cast<void*>(pIvwInst_);

	return CODE_SUCCESS;
}

/*
 * 功能:	销毁唤醒实例
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::destroyIvwInst()
{
	CHECK_INST_IS_EXIST(pIvwInst_);
	CHECK_MANGER_IS_EXIST(pMgr_);

	ivInt ret = pMgr_->wIvwDestroy(pIvwInst_);
	CHECK_IVW_RET(ret, wIvwDestory_);
	pIvwInst_ = NULL;
	quec_ai_log(LOG_AI_INFO,"wIvwDestroy success\n");

	return CODE_SUCCESS;
}

/*
 *	功能:	设置实例级参数，并开始唤醒实例
 *
 *	参数:
 *		szAudioFilePath	当前识别音频的标识
 *
 *	返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::startIvwInst(_In_opt_ const ivChar *pAudioFilePath)
{
	CHECK_INST_IS_EXIST(pIvwInst_);

	if (bIvwInstStart_)
	{
		quec_ai_log(LOG_AI_INFO,"唤醒实例已启动，请勿重复启动\n");
		return CODE_SUCCESS;
	}

	ivInt ret = CODE_SUCCESS;

	if (pAudioFilePath != NULL && strlen(pAudioFilePath) != 0)
	{
		/*
		 * 设置识别音频标识。这里用作展示实例级参数的设置方式，实际使用中可以根据情况选择是否调用
		 */
		ret = pIvwInst_->wIvwSetParameter("wivw_param_sid", pAudioFilePath);
		CHECK_IVW_RET(ret, wIvwSetParameter_);

		quec_ai_log(LOG_AI_INFO,"wIvwSetParameter success\n");

		/*
		 * 设置识别音频标识。这里用作展示实例级参数的获取方式，实际使用中可以根据情况选择是否调用。
		 */
		ivChar	paramValueRead[256];
		ret = pIvwInst_->wIvwGetParameter("wivw_param_sid", paramValueRead, sizeof(paramValueRead) - 1);
		CHECK_IVW_RET(ret, wIvwGetParameter_);

		quec_ai_log(LOG_AI_INFO,"wIvwGetParameter success\n");
	}

	/*
	 *  注意：上述唤醒实例的参数，在每次调用wIvwStart接口之前都需要进行设置。而不是只设置一次，后面直接调用wIvwStart和wIvwStop接口
     *
     *  备注：每次唤醒任务中使用的资源，由参数ivwInstResSet数组指定。也就是说，即使通过wIvwResourceAdd接口将资源加载到了引擎中，
     *
     *  但未通过wIvwStart接口传递给唤醒实例，则该资源不会生效使用
	 */
	ret = pIvwInst_->wIvwStart(ivwInstResSet, sizeof(ivwInstResSet) / sizeof(IVW_RES_SET));
	CHECK_IVW_RET(ret, wIvwStart_);
	


	bIvwInstStart_ = true;
	quec_ai_log(LOG_AI_INFO,"wIvwStart success,%d\n",bIvwInstStart_);

	return CODE_SUCCESS;
}

/*
 *功能:	停止唤醒实例
 *
 *返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::stopIvInst()
{
	CHECK_INST_IS_EXIST(pIvwInst_);

	if (!bIvwInstStart_)
	{
		quec_ai_log(LOG_AI_INFO,"唤醒实例未启动，不需要停止\n");
		return CODE_SUCCESS;
	}

	ivInt ret = pIvwInst_->wIvwStop();
	CHECK_IVW_RET(ret, wIvwStop_);
	quec_ai_log(LOG_AI_INFO,"wIvwStop success\n");

	bIvwInstStart_ = false;

	return CODE_SUCCESS;
}

/*
 * 功能:	 处理唤醒回调
 *
 * 参数:   pIvwResult 唤醒结果字符串
 *
 *	其他:
 *		唤醒识别结果如下：{"rlt":[{"sid":"ivw_test","istart":8,"iresid":3,"iresIndex":0,"iduration":96,"nfillerscore":18084,"nkeywordscore":121518,"ncm_keyword":1257,"ncm_filler":100,"ncm":1157,"ncmThresh":800,"decConfidence":0.000000,"keyword":"xiao3 wei1 xiao3 wei1"}]}
 *		其中，各个字段的含义如下：
 *			sid为当前识别音频的标识，请看startIvwInst函数如何设置sid
 *			iresid为唤醒资源对应的id，与wIvwStart传入的IVW_KEYWORD资源的id对应 
 *			istart为唤醒词起始帧数
 *			iduration为唤醒词持续帧数，即从开始阅读唤醒词到唤醒词全部读完，经过的帧数
 *			ncm为唤醒得分，得分越高，表示与唤醒词越匹配
 *			ncmThresh为唤醒门限，只有唤醒得分高于此门限值时，唤醒回调函数才会触发
 *			keyword为说话人所说的唤醒词
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::procIvwCallback(_In_ const ivChar *pIvwResult)
{
	/*
	 * 这里直接打印唤醒结果，输出到文件中。也可以解析字符串，从中获取后续处理过程中需要用到的参数值
	 *
	 * 如唤醒词，唤醒得分，开始帧，结束帧等
	 *
	 * ps:回调函数中不要使用耗时较长的操作，因为函数回调期间，会使引擎处理暂停，长时间暂停会造成音频积压，进而影响程序稳定性。
	 *
	 * 因为windows的控制台默认使用gbk编码，所以把引擎抛出的utf8格式的结果转换为gbk格式
	 *
	 * linux环境下不需要转换，可以直接使用utf8格式
	 */
	kws_tag=1;
	quec_ai_log(LOG_AI_INFO,"ivw wakeup tag=%d\n",kws_tag);
	kws_rung_flag=false;
	common_msg_t *msg = ai_common_create_msg(KWS_EVENT_WAKEUP_RESULT,nullptr,0,1);
	// my_callback();
	
	kws_ops->private_data->ops->set_param(msg);
	std::string szGbk = utf8gbk(pIvwResult);
	quec_ai_log(LOG_AI_INFO,"ivw wakeup reslut string is: %s \n", szGbk.c_str());

	return CODE_SUCCESS;
}

/*
 * 功能:	写入音频
 *
 * 参数:	audioBuffer 音频buf
 *	    	bufLen		音频buf长度
 *		    nWriteState	写入状态：IVW_WRITE_CONTINUE 表示持续写入, IVW_WRITE_STOP 表示写入结束
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::writeAudio(_In_ const ivChar *pAudioBuffer, _In_ ivInt bufLen, _In_ ivInt nWriteState)
{
	ivInt ret = CODE_SUCCESS;

	/* 
	 * nWriteState为IVW_WRITE_STOP表示写入数据结束。
	 *
	 * 目的是在音频结束时，告知唤醒引擎将缓存的音频特征全部进行计算
	 *
	 * 一般是在音频写入结束时使用IVW_WRITE_STOP。
	 */
	ret = pIvwInst_->wIvwWrite(pAudioBuffer, bufLen, nWriteState);

	CHECK_IVW_RET(ret, wIvwWrite_);

	return ret;
}

/*
 * 功能:	从资源文件中加载资源数据到内存中
 *
 * 参数:	resFilePath   资源文件路径
 *          pResBuf       存储资源的内存地址
 *          pResSize	  资源的长度，单位为字节
 *
 * 返回值: 0 表示成功,其他值表示失败
 */
ivInt IvwWrapper::loadResFileIntoMemory(const ivChar * pResFilePath, ivChar **pResBuf, ivUInt *pResSize)
{
	if (pResFilePath == NULL || pResBuf == NULL || pResSize == NULL)
	{
		return CODE_PARAM_IS_NULL;
	}

	*pResBuf = NULL;
	*pResSize = 0;

	size_t resSize = get_file_size(pResFilePath);
	if (resSize == 0 || resSize > static_cast<size_t>(std::numeric_limits<ivUInt>::max()))
	{
		return CODE_FILE_NOT_FOUND;
	}

	ivChar *pRes = new (std::nothrow) char[resSize];
	if (pRes == NULL)
	{
		return CODE_FILE_LOAD_FAIL;
	}
	ivInt ret = read_bin_file(pResFilePath, pRes, resSize);

	if (ret != 0)
	{
		delete[] pRes;
		*pResBuf = NULL;
		*pResSize = 0;
		return CODE_FILE_LOAD_FAIL;
	}

	*pResBuf = pRes;
	*pResSize = static_cast<ivUInt>(resSize);

	return CODE_SUCCESS;
}

void IvwWrapper::kws_thread_function()
{
	char audio_data[KWS_AUDIO_FRAME_LEN];
	FILE* audio_file = NULL;
	// 本地音频存储
	if(save_file_flag)
	{
		audio_file = fopen("captured_audio.pcm", "wb");
		if (audio_file == NULL) {
		    quec_ai_log(LOG_AI_INFO,"Failed to open audio file for writing!\n");
		    return;
		}
	}
   
	quec_ai_log(LOG_AI_INFO,"kws thread runing,%d,%d\n",kws_exit_flag,kws_rung_flag);
    memset(audio_data,0x00,KWS_AUDIO_FRAME_LEN);
	while(kws_exit_flag)
	{
		while(kws_rung_flag)
		{
			int len=ringbuffer_read_wait(kws_audio_buffer,audio_data,KWS_AUDIO_FRAME_LEN);
			if(len!=KWS_AUDIO_FRAME_LEN)
			{
				quec_ai_log(LOG_AI_INFO,"kws read data error,%d\n",len);
				continue;
			}
			
			writeAudio(audio_data, KWS_AUDIO_FRAME_LEN, IVW_WRITE_CONTINUE);
			if(save_file_flag)
			{
				fwrite(audio_data, 1, sizeof(audio_data), audio_file);
				fflush(audio_file);
			}  
			if(save_file_flag && add_audio_tag)
			{
				// if(kws_tag==1)
				// {
				// 	int16_t tag_value = 32767;  
				//  fwrite(&tag_value, sizeof(int16_t), 1, audio_file);
				// 	fflush(audio_file); 
				// 	kws_tag=0;
				// 	quec_ai_log(LOG_AI_INFO,"ivw wakeup tag=%d\n",kws_tag);
				// }
			}
			memset(audio_data,0x00,KWS_AUDIO_FRAME_LEN);
		}
		audio_run_sem.acquire();
		quec_ai_log(LOG_AI_INFO,"kws thread again runing\n");
	}
	if (audio_file)
	{
		fclose(audio_file);
	}
	/*等待信号量再次启动*/
}

ivInt IvwWrapper::post_sem()
{
	audio_run_sem.release();
	kws_rung_flag=true;
	return 0;
}


void IvwWrapper::audio_kws_callback(void* data, int data_len)
{
	ringbuffer_write(kws_audio_buffer,(char*)data,data_len);
}

ivInt IvwWrapper::audio_thread_start()
{
	kws_audio_buffer=ringbuffer_init(KWS_RING_BUFFER_SIZE);
	kws_audio_thread=std::thread (&IvwWrapper::kws_thread_function,this);
	return CODE_SUCCESS;
}


