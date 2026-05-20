/**
 * @file        ai_websocket.c
 * @brief       WebSocket通信模块实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了WebSocket通信功能，包括Coze API连接、心跳保活等。
 */

#include "lws_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include <libwebsockets.h>
#include <alsa/asoundlib.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include "ai_websocket_coze.h"
#include "ai_log.h"
#include "ai_websocket.h"
#include "ai_audio_play.h"
#include "ai_websocket_coze.h"
#include "ai_common_shm.h"
#include "ai_websocket_config.h"
#include "ai_audio.h"
#include "ai_common_ringbuffer.h"
#include "ai_audio_init.h"
#include "ai_common_define.h"


static bool  online_runing_flag=false;
static sem_t kws_websocket_sem;
static sem_t init_websocket_sem;
static bool g_websocket_sem_initialized = false;
static bool g_websocket_init_thread_active = false;
static bool g_websocket_connect_ready = false;

static module_desc_t* websocket_module_desc = NULL;
// FILE* audio_file_1;

pthread_mutex_t recv_queue_lock;
static RingBuffer* websocket_buffer;
#define LWS_PROTOCOL_LIST_TERM { NULL, NULL, 0, 0, 0, NULL, 0 }
#define block_size (3 * 4096)

bool online_asr_flag=false;
pthread_mutex_t lws_mutx;
int  force_exit = 0;
static struct lws *wsi_mirror=NULL;
static struct lws *lws_ptr = NULL;
// 0 -init ,1 - send update success ,2 - recv update sucess 3 - send audio ,4 recv audio

char *audio_file_data = NULL;

static time_t g_last_ping_time = 0;
 time_t asr_connect_time = 0;
bool web_socket_state=false;
static pthread_mutex_t g_websocket_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_downstream_fragment_drop = false;
// static time_t g_last_activity_time = 0;
struct lws * quec_ai_websocket(void){
	return lws_ptr;
}

void quec_ai_set_websocket_state(bool state){
	pthread_mutex_lock(&g_websocket_state_mutex);
	web_socket_state = state;
	pthread_mutex_unlock(&g_websocket_state_mutex);
}

bool websocket_get_state( void ){
	bool state;
	pthread_mutex_lock(&g_websocket_state_mutex);
	state = web_socket_state;
	pthread_mutex_unlock(&g_websocket_state_mutex);
	return state;
}

void quec_ai_set_online_asr_flag(bool state){
	pthread_mutex_lock(&g_websocket_state_mutex);
	online_asr_flag = state;
	pthread_mutex_unlock(&g_websocket_state_mutex);
}

bool quec_ai_get_online_asr_flag(void){
	bool state;
	pthread_mutex_lock(&g_websocket_state_mutex);
	state = online_asr_flag;
	pthread_mutex_unlock(&g_websocket_state_mutex);
	return state;
}

void quec_ai_set_asr_connect_time(time_t connect_time){
	pthread_mutex_lock(&g_websocket_state_mutex);
	asr_connect_time = connect_time;
	pthread_mutex_unlock(&g_websocket_state_mutex);
}

time_t quec_ai_get_asr_connect_time(void){
	time_t connect_time;
	pthread_mutex_lock(&g_websocket_state_mutex);
	connect_time = asr_connect_time;
	pthread_mutex_unlock(&g_websocket_state_mutex);
	return connect_time;
}

static void quec_ai_set_websocket_sem_initialized(bool state){
	pthread_mutex_lock(&g_websocket_state_mutex);
	g_websocket_sem_initialized = state;
	pthread_mutex_unlock(&g_websocket_state_mutex);
}

static bool quec_ai_get_websocket_sem_initialized(void){
	bool state;
	pthread_mutex_lock(&g_websocket_state_mutex);
	state = g_websocket_sem_initialized;
	pthread_mutex_unlock(&g_websocket_state_mutex);
	return state;
}

static void quec_ai_set_websocket_init_thread_active(bool state){
	pthread_mutex_lock(&g_websocket_state_mutex);
	g_websocket_init_thread_active = state;
	pthread_mutex_unlock(&g_websocket_state_mutex);
}

static bool quec_ai_get_websocket_init_thread_active(void){
	bool state;
	pthread_mutex_lock(&g_websocket_state_mutex);
	state = g_websocket_init_thread_active;
	pthread_mutex_unlock(&g_websocket_state_mutex);
	return state;
}

static void quec_ai_set_websocket_connect_ready(bool state){
	pthread_mutex_lock(&g_websocket_state_mutex);
	g_websocket_connect_ready = state;
	pthread_mutex_unlock(&g_websocket_state_mutex);
}

static bool quec_ai_get_websocket_connect_ready(void){
	bool state;
	pthread_mutex_lock(&g_websocket_state_mutex);
	state = g_websocket_connect_ready;
	pthread_mutex_unlock(&g_websocket_state_mutex);
	return state;
}

static int ratelimit_connects(unsigned int *last, unsigned int secs)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	if ((unsigned long)tv.tv_sec - (unsigned long)(*last) < (unsigned long)secs)
		return 0;

	*last = (unsigned int)tv.tv_sec;

	return 1;
}

static void websocket_check_macro(void){
	#if defined(LWS_HAS_GETOPT_LONG)
	quec_ai_log(LOG_AI_INFO,"LWS_HAS_GETOPT_LONG");
	#endif 
	#if defined(WIN32)
	quec_ai_log(LOG_AI_INFO,"WIN32");
	#endif 
	#if defined(LWS_WITH_TLS)
	quec_ai_log(LOG_AI_INFO,"LWS_WITH_TLS");
	#endif 
	#if defined(LWS_HAVE_SSL_CTX_set1_param)
	quec_ai_log(LOG_AI_INFO,"LWS_HAVE_SSL_CTX_set1_param");
	#endif 
	#if defined(LWS_ROLE_WS)
	quec_ai_log(LOG_AI_INFO,"LWS_ROLE_WS");
	#endif 
	#if defined(LWS_WITHOUT_EXTENSIONS)
	quec_ai_log(LOG_AI_INFO,"LWS_WITHOUT_EXTENSIONS");
	#endif 
	#if defined(LWS_WITH_TLS)
	quec_ai_log(LOG_AI_INFO,"LWS_WITH_TLS");
	#endif 
	#if defined(LWS_WITH_MBEDTLS)
	quec_ai_log(LOG_AI_INFO,"LWS_WITH_MBEDTLS");
	#endif 
}

int quec_ai_check_websocket_state(void){

	return 0;
}

void quec_ai_set_exit(){
	quec_ai_log(LOG_AI_INFO,"Will exit!");
	force_exit = 1;
	online_runing_flag=false;
	common_msg_t *msg = ai_common_create_msg(KWS_START_EVENT,NULL,0,1);
	websocket_module_desc->private_data->ops->set_param(msg);
}

int quec_ai_disconnect_websocket(){
	quec_ai_set_exit();
	return 0;
}

void web_disply(char *data,int len){

	for(int i =0;i< len;i++){
	    quec_ai_log(LOG_AI_INFO,"%c",data[i]);
	}
	quec_ai_log(LOG_AI_INFO,"\n");
}

static int quec_ai_callback_event_client_append_hadnshake_header(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len){
	UNUSED(wsi);
	UNUSED(reason);
	UNUSED(user);
	quec_ai_log(LOG_AI_INFO,"mirror: LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER\n");
    // 追加Authorization头部
    const char *auth = "Authorization: Bearer ";
    char header[256];
	quec_ai_config *ai_config = quec_ai_get_config();
    snprintf(header, sizeof(header), "%s%s\r\n", auth, ai_config->token);
    
    unsigned char **p = (unsigned char **)in;
    unsigned char *end = (*p) + len;
    // 确保有足够空间写入头部
    if (*p + strlen(header) > end) {
        lwsl_err("Header buffer too small");
        return -1;
    }
    // 写入头部
    *p += lws_snprintf((char*)*p, end - *p, "%s", header);
	return 0;
}

static int quec_ai_callback_client_receive(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len){
	UNUSED(reason);
	UNUSED(user);
	quec_ai_log(LOG_AI_INFO,"quec_ai_callback_client_receive");
	if (lws_remaining_packet_payload(wsi) != 0 || !lws_is_final_fragment(wsi)) {
		g_downstream_fragment_drop = true;
		quec_ai_log(LOG_AI_ERR,"Discarding fragmented downstream websocket frame\n");
		return 0;
	}
	if (g_downstream_fragment_drop) {
		g_downstream_fragment_drop = false;
		quec_ai_log(LOG_AI_ERR,"Discarding final downstream websocket fragment\n");
		return 0;
	}
	//web_disply(in,len);
	online_runing_flag=true;
	quec_ai_coze_handle_downstream_event(wsi,(char *)in, len);
	return 0;
}

static int callback_lws_mirror(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	// quec_ai_log(LOG_AI_INFO,"mirror: Get reason %d\n",reason);
	time_t current_time = time(NULL);
	char audio_data[ONLINE_READ_LEN];
	switch (reason) {

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		quec_ai_callback_event_client_append_hadnshake_header(wsi,reason,user,in,len);
		lws_callback_on_writable(wsi);
		break;
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		break;
	case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
		break;
	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		break;
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		quec_ai_log(LOG_AI_INFO,"mirror:create web socket connect sucess!");
		g_last_ping_time = current_time;
        // g_last_activity_time = current_time;
            // 立即请求可写回调开始心跳
		lws_set_timer_usecs(wsi,  ONLINE_CYCLE_TIME); // 10ms = 10000us
		quec_ai_coze_handler_upstream_event(wsi,"chat.update",NULL,0);
		lws_ptr = wsi;
		break;
	case LWS_CALLBACK_TIMER: {
		// 定时器触发时，注册写事件
		lws_callback_on_writable(wsi);
		// 重新设置定时器
		lws_set_timer_usecs(wsi, ONLINE_CYCLE_TIME); // 10ms = 10000us
		break;
	}
	case LWS_CALLBACK_CLIENT_CLOSED:
		quec_ai_log(LOG_AI_INFO,"mirror:close web socket connect!");
		quec_ai_set_exit();
		quec_ai_set_websocket_state(false);
		break;
	case LWS_CALLBACK_CLIENT_WRITEABLE:
		if (current_time - g_last_ping_time >= 15) {
			unsigned char ping_frame[1] = {0};
			int ret = lws_write(wsi, ping_frame, 0, LWS_WRITE_PING);
			if (ret >= 0) {
				quec_ai_log(LOG_AI_INFO, "Sent ping frame for keepalive\n");
				g_last_ping_time = current_time;
			} else {
				quec_ai_log(LOG_AI_ERR, "Failed to send ping frame: %d\n", ret);
			}
		}
		while (ringbuffer_used(websocket_buffer) >= ONLINE_READ_LEN) {
			ringbuffer_read(websocket_buffer, audio_data, ONLINE_READ_LEN);
			quec_ai_coze_handler_upstream_event(wsi,"input_audio_buffer.append",audio_data,ONLINE_READ_LEN);
		}

		lws_ptr = wsi;
		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:
		quec_ai_callback_client_receive(wsi,reason,user,in,len);
		quec_ai_set_asr_connect_time(time(NULL));
        break;
	case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
        g_last_ping_time = current_time;
        quec_ai_log(LOG_AI_INFO, "Received PONG from server");
        // libwebsockets 会自动回复PONG，这里只需要记录
        break;
	default:
		break;
	}

	return 0;
}
enum demo_protocols {

	PROTOCOL_DUMB_INCREMENT,
	PROTOCOL_LWS_MIRROR,

	/* always last */
	DEMO_PROTOCOL_COUNT
};
void websocket_audio_callbnack(void* data, int data_len)
{
    ringbuffer_write(websocket_buffer,(char*)data,data_len);
}

static const struct lws_protocols protocols[] = {

	{
		"lws-mirror-protocol",
		callback_lws_mirror,
		0, 4096, 0, NULL, 0
	},
	LWS_PROTOCOL_LIST_TERM
};

void * quec_ai_outtime_websocket(void * argv)
{
	(void)argv;
	time_t current_time;
	while(1)
	{
		sleep(1);
		bool socket_state = websocket_get_state();
		bool asr_state = quec_ai_get_online_asr_flag();
		time_t connect_time = quec_ai_get_asr_connect_time();
		if(socket_state==true || asr_state==true)
		{
			current_time = time(NULL);
			if (connect_time != 0 && (current_time - connect_time) > 60) {
				quec_ai_log(LOG_AI_INFO,"WebSocket connection timeout, reconnecting...");
				quec_ai_set_exit();
			}
		}	
	}
	return NULL;
}


void * quec_ai_connect_websocket(void * argv){
    struct lws_context_creation_info info;
	struct lws_client_connect_info i;
	struct lws_context *context;
	quec_ai_config *confg = (quec_ai_config  *)argv;
	unsigned int rl_mirror = 0;
	unsigned long last = lws_now_secs();
	memset(&info, 0, sizeof info);
	pthread_mutex_init(&lws_mutx, NULL);
	websocket_check_macro();
	memset(&i, 0, sizeof(i));
	i.port = confg->port;
	i.path = confg->path;
	i.address = confg->adress;
	i.ssl_connection = LCCSCF_USE_SSL;
	i.host = i.address;
	i.origin = i.address;
	i.ietf_version_or_minus_one = -1;
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.gid = (gid_t)-1;
	info.uid = (uid_t)-1;
	info.fd_limit_per_thread = 2 + 4;
	// 增加发送和接收缓冲区大小
	// info.ws_ping_pong_interval = 30;        // PING/PONG间隔
	info.ka_time = 60;                      // 保持连接时间
	info.ka_probes = 10;                    // 探测次数
	info.ka_interval = 10;                  // 探测间隔
	// 调整缓冲区设置
	info.max_http_header_data = 2048;
	info.max_http_header_pool = 16;
	info.ssl_ca_filepath = "/etc/ssl/certs/ca-certificates.crt";
	info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.options |= LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW;
	while(1)
	{
		sem_wait(&kws_websocket_sem);
		wsi_mirror=NULL;
		context = lws_create_context(&info);
		if (context == NULL) {
			quec_ai_log(LOG_AI_INFO,"Creating libwebsocket context failed\n");
			quec_ai_set_websocket_connect_ready(false);
			return NULL;
		}
		i.context = context;
		quec_ai_log(LOG_AI_INFO,"start coze socket\n");
		force_exit=0;
		while (!force_exit){
			if ( !wsi_mirror && ratelimit_connects(&rl_mirror, 2u) ) {
				quec_ai_log(LOG_AI_INFO,"mirror: connecting\n");
				i.protocol = protocols[0].name;
				i.pwsi = &wsi_mirror;
				wsi_mirror = lws_client_connect_via_info(&i);
			}
			lws_service(context, 1000);  // 1s 超时
			usleep(10000); // 避免 CPU 占用过高
			if ( lws_now_secs() != last) {
				quec_ai_log(LOG_AI_INFO,"...\n");
				last = lws_now_secs();
			}
		}
		quec_ai_log(LOG_AI_INFO,"Exiting\n");
		if(websocket_module_desc!=NULL)
		{
			common_msg_t *msg = ai_common_create_msg(AUDIO_FILE_PLAY_EVENT,"./../config/5.wav",strlen("./../config/5.wav"),1);
			websocket_module_desc->private_data->ops->set_param(msg);
		}
		pthread_mutex_lock(&lws_mutx);
		lws_context_destroy(context);
		pthread_mutex_unlock(&lws_mutx);
		common_msg_t *msg = ai_common_create_msg(KWS_START_EVENT,NULL,0,1);
		websocket_module_desc->private_data->ops->set_param(msg);
	}
	return NULL;
}


int start_large_model_deal()
{
	if (!quec_ai_get_websocket_sem_initialized() || !quec_ai_get_websocket_connect_ready()) {
		quec_ai_log(LOG_AI_ERR,"websocket is not ready to start,%d,%d\n",g_websocket_sem_initialized,g_websocket_connect_ready);
		return -1;
	}

	int sem_value;
	if (sem_getvalue(&kws_websocket_sem, &sem_value) != 0) {
		quec_ai_log(LOG_AI_ERR,"get kws-websocket sem value error\n");
		return -1;
	}
	if(sem_value>0)
	{
		return 0;
	}else{
		if(online_runing_flag==false)
		{
			if (sem_post(&kws_websocket_sem) == -1) {
	            quec_ai_log(LOG_AI_INFO,"post kws-websocket sem error\n");
	            return -1;
	        }
		}
	}
	return 0;
}

int start_init_large_mode()
{
	if (!quec_ai_get_websocket_sem_initialized() || !quec_ai_get_websocket_init_thread_active()) {
		quec_ai_log(LOG_AI_ERR,"websocket init thread is not active,%d,%d\n",g_websocket_sem_initialized,g_websocket_init_thread_active);
		return -1;
	}
	if (sem_post(&init_websocket_sem) != 0) {
		quec_ai_log(LOG_AI_ERR,"post init-websocket sem error\n");
		return -1;
	}
	return 0;
}

void* quec_ai_waitinit_websocket(void * argv){
	(void)argv;
	pthread_t pthread_id;
	pthread_t pthread_id_web_outtime;
	while(1)
	{
		if(sem_wait(&init_websocket_sem)==0)
		{
			break;
		}
	}
	quec_ai_set_websocket_init_thread_active(false);
	// audio_file_1 = fopen("333444.wav", "wb");
	pthread_mutex_init(&recv_queue_lock, NULL);
	websocket_buffer=ringbuffer_init(READ_RING_BUFFER_SIZE);
	quec_ai_audio_register_data_callback(websocket_audio_callbnack);

	// 先检查并创建配置文件
    if (quec_ai_ensure_config_file("./../config/websocket_config.json") != 0) {
        quec_ai_log(LOG_AI_ERR, "Failed to ensure config file exists");
        goto exit;
    }
	// 先获取并保存token
    if (quec_ai_coze_token_init() != 0) {
        quec_ai_log(LOG_AI_ERR, "Failed to initialize Coze token");
        goto exit;
    }
	if ( quec_ai_set_config("./../config/websocket_config.json") != 0){
		goto exit;
	}
	if (pthread_create(&pthread_id, NULL, quec_ai_connect_websocket,(void *)quec_ai_get_config()) != 0) {
        quec_ai_log(LOG_AI_ERR,"create ai connect  pthread failed!");
        goto exit;
    }
	pthread_detach(pthread_id);
	quec_ai_log(LOG_AI_ERR,"shunze 111111111111111\n");
	quec_ai_set_websocket_connect_ready(true);
	quec_ai_log(LOG_AI_ERR,"shunze 2222222222222222222\n");
	if (pthread_create(&pthread_id_web_outtime, NULL, quec_ai_outtime_websocket,NULL) != 0) {
        quec_ai_log(LOG_AI_ERR,"create ai outime pthread failed!");
        return NULL;
    }
	pthread_detach(pthread_id_web_outtime);
	return NULL;
exit:
quec_ai_log(LOG_AI_ERR,"shunze 33333333333333333\n");
	quec_ai_set_websocket_connect_ready(false);
	quec_ai_log(LOG_AI_ERR,"shunze 44444444\n");
	quec_ai_set_websocket_init_thread_active(false);
	return NULL;
}



int quec_ai_web_socket_pthread_init(module_desc_t *self_ops){
	pthread_t pthread_id_init;
	websocket_module_desc = self_ops;
	if (sem_init(&kws_websocket_sem, 0, 0) != 0) {
		quec_ai_log(LOG_AI_ERR,"init kws-websocket sem failed!");
		return -1;
	}
	if (sem_init(&init_websocket_sem, 0, 0) != 0) {
		quec_ai_log(LOG_AI_ERR,"init websocket-init sem failed!");
		sem_destroy(&kws_websocket_sem);
		return -1;
	}
	quec_ai_set_websocket_sem_initialized(true);
	quec_ai_set_websocket_init_thread_active(true);
	if (pthread_create(&pthread_id_init, NULL, quec_ai_waitinit_websocket,NULL) != 0) {
        quec_ai_log(LOG_AI_ERR,"create ai waitinit pthread failed!");
		quec_ai_set_websocket_init_thread_active(false);
		quec_ai_set_websocket_sem_initialized(false);
		sem_destroy(&kws_websocket_sem);
		sem_destroy(&init_websocket_sem);
        return -1;
    }
	return 0;
}
