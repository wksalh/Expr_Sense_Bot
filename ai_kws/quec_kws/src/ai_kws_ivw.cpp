#include "quec_ai_kws_ivw.h"
#include "IvwWrapper.h"
#include "defines.h"
#include "ArgumentParser.h"
#include "errorCode.h"
#include "stringUtil.h"
#include "ivw/ivw_err.h"
#include "ivw/wivw_api.h"
#include <stdio.h>
#include <string>
#include <thread>
#include "ai_audio.h"
#include "ai_log.h"
#include "ai_kws_ivw.h"
static IvwWrapper* g_ivwWrapper = nullptr;  // 全局实例指针

extern "C" {

void start_kws_deal()
{

    if (g_ivwWrapper != nullptr) {
        quec_ai_log(LOG_AI_INFO,"kws get sem\n");
        g_ivwWrapper->post_sem();
    }
    return;
}

void kws_audio_callbnack(void* data, int data_len)
{
    if (g_ivwWrapper != nullptr) {
        g_ivwWrapper->audio_kws_callback(data,data_len);
    }
}

int kws_destroy()
{
    if (g_ivwWrapper != nullptr) 
    {
        delete g_ivwWrapper;
        g_ivwWrapper = nullptr;
    }
    return 0;
}


int kws_init(module_desc_t *self_ops)
{
    std::string IVW_MLP="./../config/res_shuffnet_v2/mlp.bin";
    std::string IVW_FILLER="./../config/res_shuffnet_v2/Filler/state_filler_3000s_kladi_1179.txt";
    std::string IVW_KEYWORD="./../config/res_shuffnet_v2/keyword_xiaoyuantongxue.bin";
    g_ivwWrapper = new IvwWrapper();
    int ret=0;
    ret = g_ivwWrapper->loadLibrary("w_ivw");
	CHECK_FUNC_RET(ret, loadLibrary);
    ret = g_ivwWrapper->initIvwManager();
	CHECK_FUNC_RET(ret, initIvwMagager);
    ret = g_ivwWrapper->loadAllRes(IVW_MLP.c_str(),IVW_FILLER.c_str(), IVW_KEYWORD.c_str());
	CHECK_FUNC_RET(ret, loadAllRes);
    ret = g_ivwWrapper->createIvwInstAndSetCallback();
	CHECK_FUNC_RET(ret, createIvwInstAndSetCallback);
    ret = g_ivwWrapper->startIvwInst(NULL);
	CHECK_FUNC_RET(ret, startIvwInst);
    g_ivwWrapper->kws_set_ops(self_ops);
    ret = g_ivwWrapper->audio_thread_start();
    CHECK_FUNC_RET(ret, audio_thread_start);
    quec_ai_audio_register_data_callback(kws_audio_callbnack);
    return 0; 
}
}