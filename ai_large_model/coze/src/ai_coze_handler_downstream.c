
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include "ai_common_base64.h"
#include "ai_websocket_coze.h"
#include "ai_log.h"
#include "ai_audio_play.h"
#include "ai_websocket.h"
#include  "ai_websocket_config.h"

#define HANDLER_EVENT_NUM   22
#define AGENT_ECHO_GUARD_MS 1500

struct timeval maix_mic_end_time;

// extern FILE* audio_file_1;

static volatile bool g_interrupt_in_progress = false;  // 标记是否正在打断（speech_started 之后，直到 canceled 或新的 sentence_start）
static sem_t g_interrupt_sem;  // 信号量：用于及时通知 audio.delta 停止处理（speech_started 时发送信号）
static pthread_once_t g_interrupt_sem_once = PTHREAD_ONCE_INIT;
// 初始化信号量（线程安全，只初始化一次）
static void init_interrupt_sem_impl(void)
{
    if (sem_init(&g_interrupt_sem, 0, 0) == 0) {
        quec_ai_log(LOG_AI_INFO, "[Init] Interrupt semaphore initialized\n");
    } else {
        quec_ai_log(LOG_AI_ERR, "[Init] Interrupt semaphore init failed\n");
    }
}

static void init_interrupt_sem_once(void)
{
    pthread_once(&g_interrupt_sem_once, init_interrupt_sem_impl);
}

extern int quec_ai_write_file(char *in,size_t len);
static int quec_ai_handler_downstream_conversation_audio_delta(struct lws *wsi,cJSON *json_str,size_t len){
    (void)wsi;
    (void)len;
    // 如果收到打断信号（speech_started），立即丢弃数据，不解码也不播放
    if (sem_trywait(&g_interrupt_sem) == 0) {
        quec_ai_log(LOG_AI_ERR, "[Audio Delta] [INTERRUPT] Received interrupt signal (speech_started), discarding delta data immediately\n");
        return 0;  // 直接返回，不解码也不播放
    }
    
    // 检查 g_interrupt_in_progress（标志位，用于持续阻止后续的 audio.delta）
    bool interrupt_in_progress = g_interrupt_in_progress;
    
    if (interrupt_in_progress) {
        quec_ai_log(LOG_AI_ERR, "[Audio Delta] [INTERRUPT] Interrupt in progress (g_interrupt_in_progress=true), discarding delta data immediately\n");
        return 0;  // 直接返回，不解码也不播放
    }
    
    cJSON *content = cJSON_GetObjectItem(json_str, "content");
    if (!content || !cJSON_IsString(content) || content->valuestring == NULL) {
        quec_ai_log(LOG_AI_ERR, "[Audio Delta] Invalid content field\n");
        return -1;
    }

    const char *audio_delta = content->valuestring;
    size_t audio_delta_len = strlen(audio_delta);
    quec_ai_log(LOG_AI_ERR,"[Audio Delta] Length: %zu\n", audio_delta_len);

    size_t base64_audio_len = 0;
    base64_decode_openssl(NULL, 0, &base64_audio_len, (const unsigned char *)audio_delta, audio_delta_len);
    if (base64_audio_len == 0) {
        quec_ai_log(LOG_AI_ERR, "[Audio Delta] Invalid decoded audio length\n");
        return -1;
    }

    unsigned char *base64_audio = (unsigned char *)malloc(base64_audio_len);
    if (!base64_audio) {
        quec_ai_log(LOG_AI_ERR, "[Audio Delta] Audio buffer malloc failed\n");
        return -1;
    }

    if (base64_decode_openssl(base64_audio, base64_audio_len, &base64_audio_len, (const unsigned char *)audio_delta, audio_delta_len) != 0) {
        quec_ai_log(LOG_AI_ERR, "[Audio Delta] Base64 decode failed\n");
        free(base64_audio);
        return -1;
    }

    quec_ai_audio_play((char *)base64_audio,base64_audio_len);
    free(base64_audio);
    return 0;
}

static int quec_ai_handler_downstream_chat_created(struct lws *wsi,cJSON *json_str,size_t len){
    (void)json_str;
    (void)wsi;
    (void)len;
    quec_ai_log(LOG_AI_ERR,"[Event] downstream_chat.created\n");
    quec_ai_set_websocket_state(true);
    return 0;
}

static int quec_ai_handler_downstream_chat_updated(struct lws *wsi,cJSON *json_str,size_t len){
    (void)wsi;
    (void)len;

    char *json_str1 = cJSON_PrintUnformatted(json_str);
    if (!json_str1) {
        quec_ai_log(LOG_AI_ERR, "Failed to convert cJSON to string\n");
        return -1;
    }
    quec_ai_log(LOG_AI_INFO, "[Event] downstream chat.updated: %s\n", json_str1);

    cJSON *turn_detection = cJSON_GetObjectItem(json_str, "turn_detection");
    if (turn_detection && cJSON_IsObject(turn_detection)) {
        cJSON *type = cJSON_GetObjectItem(turn_detection, "type");
        cJSON *interrupt_config = cJSON_GetObjectItem(turn_detection, "interrupt_config");
        if (type && cJSON_IsString(type)) {
            quec_ai_log(LOG_AI_INFO, "[chat.updated] turn_detection.type = %s\n", type->valuestring);
        }
        if (interrupt_config) {
            char *interrupt_str = cJSON_PrintUnformatted(interrupt_config);
            quec_ai_log(LOG_AI_INFO, "[chat.updated] turn_detection.interrupt_config = %s\n", interrupt_str ? interrupt_str : "null");
            if (interrupt_str) {
                free(interrupt_str);
            }
        } else {
            quec_ai_log(LOG_AI_INFO, "[chat.updated] turn_detection.interrupt_config is NULL (may affect interruption)\n");
        }
    }
    free(json_str1);
    return 0;
}

static int quec_ai_handler_downstream_conversation_chat_created(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   quec_ai_log(LOG_AI_ERR,"[Event] conversation_chat_created\n");
   return 0;
}

static int quec_ai_handler_downstream_conversation_chat_in_progress(struct lws *wsi,cJSON *json_str,size_t len){
    (void)wsi;
   (void)json_str;
   (void)len;
   quec_ai_log(LOG_AI_ERR,"[Event] chat_in_progress\n");
   return 0;
}

static void quec_ai_save_emotion(const char *emotion_text) {
    FILE *file = fopen("/tmp/emotion_log.txt", "w");
    if (file) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        fprintf(file, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec, emotion_text);
        fclose(file);
    }
}

static int quec_ai_handler_downstream_conversation_message_delta(struct lws *wsi, cJSON *json_str, size_t len)
{
   (void)wsi;
   (void)len;
   
   cJSON *content = cJSON_GetObjectItem(json_str, "content");
   if (!content || !cJSON_IsString(content)) {
      return -1;
   }
   
   const char *content_str = content->valuestring;
   char emotion_text[128] = "自然";
   int emotion_found = 0;
    
   const char *right_bracket = strstr(content_str, "）");
   if (right_bracket != NULL) {
    size_t emotion_len = right_bracket - content_str;
    if (emotion_len > 0 && emotion_len < 100) {
        strncpy(emotion_text, content_str, emotion_len);
        emotion_text[emotion_len] = '\0';
        
        // 快乐相关表情
        if (strstr(emotion_text, "喜爱") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "喜爱");
        }
        else if (strstr(emotion_text, "开心") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "开心");
        }
        else if (strstr(emotion_text, "崇拜") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "崇拜");
        }

        
        // 悲伤相关表情
        else if (strstr(emotion_text, "委屈") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "委屈");

        }
        else if (strstr(emotion_text, "难过") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "难过");
        }

        
        // 惊讶相关表情
        else if (strstr(emotion_text, "眩晕") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "眩晕");
        }
        else if (strstr(emotion_text, "疑问") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "疑问");
        }
        else if (strstr(emotion_text, "震惊") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "震惊");
        }

        
        // 愤怒相关表情
        else if (strstr(emotion_text, "生气") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "生气");
        }

        // 恐惧相关表情
        else if (strstr(emotion_text, "害怕") != NULL) {
            emotion_found = 1;
            strcpy(emotion_text, "害怕");
        }
        else {
            strcpy(emotion_text, "自然");
        }
     }
   }
   if (!emotion_found) {
      strcpy(emotion_text, "自然");
   }
   quec_ai_log(LOG_AI_INFO, "[当前情绪] %s\n", emotion_text);
   quec_ai_save_emotion(emotion_text);
   
   return 0;
}

static int quec_ai_handler_downstream_conversation_audio_sentence_start(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   return 0;
}

static void save_complete_content(const char *content) {
    quec_ai_log(LOG_AI_ERR, "[DEBUG] save_complete_content: Saving to /tmp/complete_content.txt, content=%s\n", content ? content : "NULL");
    FILE *fp = fopen("/tmp/complete_content.txt", "w");
    if (fp) {
        fprintf(fp, "%s", content);
        fclose(fp);
        quec_ai_log(LOG_AI_ERR, "[DEBUG] Content saved successfully\n");
    } else {
        quec_ai_log(LOG_AI_ERR, "[DEBUG] Failed to open file for writing\n");
    }
}

static int quec_ai_handler_downstream_conversation_message_completed(struct lws *wsi, cJSON *json_str, size_t len) {
    (void)wsi;
    (void)len;

    quec_ai_log(LOG_AI_ERR, "[Event] conversation_message_completed\n");
    char *json_str1 = cJSON_PrintUnformatted(json_str);
    if (!json_str1) {
        return -1;
    }

    cJSON *type = cJSON_GetObjectItem(json_str, "type");
    if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "answer") != 0) {
        free(json_str1);
        return 0;
    }

    // 检查 role 字段，如果是 assistant 的回答，需要进一步判断
    cJSON *role = cJSON_GetObjectItem(json_str, "role");
    if (role && cJSON_IsString(role) && strcmp(role->valuestring, "assistant") == 0)
    {
        // 这是扣子的回答，需要检查 content 中是否包含图片URL
        // 如果包含URL，说明是扣子对图片的描述，不应该再次提交
        cJSON *content_check = cJSON_GetObjectItem(json_str, "content");
        if (content_check && cJSON_IsString(content_check)) 
        {
            const char *content_str = content_check->valuestring;
            // 检查是否包含图片URL
            if (strstr(content_str, "http://") != NULL || strstr(content_str, "https://") != NULL) 
            {
                // 包含URL，这是扣子的回答，不应该处理
                quec_ai_log(LOG_AI_INFO, "Received assistant answer with image URL, ignoring to prevent loop\n");
                free(json_str1);
                return 0;
            }
            // 如果不包含URL，但text看起来像是描述性的回答（不是问题），也不应该处理
            // 通过检查text是否以问号结尾来判断是否是问题
            // 但这里我们先检查URL，如果没有URL，继续处理（可能是用户的问题）
        }
    }


    // 检查 content 字段
    cJSON *content = cJSON_GetObjectItem(json_str, "content");
    if (!content || !cJSON_IsString(content)) {
        free(json_str1);
        return -1;
    }
    
    char *fixed_json = malloc(strlen(content->valuestring) + 3);
    if (!fixed_json) {
        quec_ai_log(LOG_AI_ERR, "Memory allocation for fixed JSON failed\n");
        free(json_str1);
        return -1;
    }

    const char *src = content->valuestring;
    char *dst = fixed_json;

    while (*src && (*src == '(' || *src == ' ' || *src == '\t' || *src == '\n' || *src == '\r')) {
        src++;
    }

    while (*src && *src != ')') {
        *dst++ = *src++;
    }
    *dst = '\0';

    cJSON *content_json = cJSON_Parse(fixed_json);
    free(fixed_json);

    if (!content_json) {
        quec_ai_log(LOG_AI_ERR, "Failed to parse content as JSON\n");
        free(json_str1);
        return -1;
    }

    cJSON *action = cJSON_GetObjectItem(content_json, "action");
    if (!action) {
        quec_ai_log(LOG_AI_ERR, "No action field found\n");
        cJSON_Delete(content_json);
        free(json_str1);
        return 0;
    }

    if (!cJSON_IsString(action)) {
        cJSON_Delete(content_json);
        free(json_str1);
        return 0;
    }

    quec_ai_log(LOG_AI_INFO, "[conversation_message_completed] action=%s\n", action->valuestring);

    if (strcmp(action->valuestring, "shotscreen") != 0) {
        cJSON_Delete(content_json);
        free(json_str1);
        return 0;
    }

    // 提取 text 字段
    cJSON *text_field = cJSON_GetObjectItem(content_json, "text");
    if (!text_field) {
        quec_ai_log(LOG_AI_ERR, "No text field found in content\n");
        cJSON_Delete(content_json);
        free(json_str1);
        return -1;
    }

    if (!cJSON_IsString(text_field)) {
        quec_ai_log(LOG_AI_ERR, "Text field is not a string\n");
        cJSON_Delete(content_json);
        free(json_str1);
        return -1;
    }

    const char *text_content = text_field->valuestring;
    
    // 再次检查 text 内容中是否包含图片URL（双重保险）
    if (strstr(text_content, "http://") != NULL || strstr(text_content, "https://") != NULL) 
    {
        // text 中包含URL，这是扣子的回答，不应该处理
        cJSON_Delete(content_json);
        free(json_str1);
        return 0;
    }
    
    // 检查是否是扣子的描述性回答（不是问题）
    // 如果text以句号、感叹号结尾，且不是问号，很可能是扣子的回答
    size_t text_len = strlen(text_content);
    if (text_len > 0) {
        // 检查是否包含问号（问题通常包含问号）
        int has_question_mark = (strstr(text_content, "？") != NULL || strstr(text_content, "?") != NULL);
        
        // 检查是否以句号、感叹号结尾（描述性回答通常以这些结尾）
        // 注意：中文字符是多字节的，需要检查最后几个字节
        int ends_with_period = 0;
        int ends_with_exclamation = 0;
        
        if (text_len >= 3) {
            // 检查是否以中文句号结尾（UTF-8编码：0xE3 0x80 0x82）
            if ((unsigned char)text_content[text_len - 3] == 0xE3 &&
                (unsigned char)text_content[text_len - 2] == 0x80 &&
                (unsigned char)text_content[text_len - 1] == 0x82) {
                ends_with_period = 1;
            }
            // 检查是否以中文感叹号结尾（UTF-8编码：0xEF 0xBC 0x81）
            if ((unsigned char)text_content[text_len - 3] == 0xEF &&
                (unsigned char)text_content[text_len - 2] == 0xBC &&
                (unsigned char)text_content[text_len - 1] == 0x81) {
                ends_with_exclamation = 1;
            }
        }
        // 检查英文标点
        char last_char = text_content[text_len - 1];
        if (last_char == '.') {
            ends_with_period = 1;
        } else if (last_char == '!') {
            ends_with_exclamation = 1;
        }
        
        // 如果以句号或感叹号结尾，且不包含问号，且role是assistant，可能是扣子的回答
        if ((ends_with_period || ends_with_exclamation) && !has_question_mark) {
            if (role && cJSON_IsString(role) && strcmp(role->valuestring, "assistant") == 0) {
                cJSON_Delete(content_json);
                free(json_str1);
                return 0;
            }
        }
    }

    quec_ai_log(LOG_AI_ERR, "[DEBUG] Detected shotscreen action, saving content and sending message\n");
    save_complete_content(text_content);
    quec_ai_coze_handler_upstream_event(wsi,"conversation.message.create",NULL,0);
    cJSON_Delete(content_json);
    free(json_str1);
    return 0;
}

static int quec_ai_handler_downstream_conversation_audio_completed(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   quec_ai_log(LOG_AI_ERR,"[Event] conversation_audio_completed\n");
   return 0;
}

static int quec_ai_handler_downstream_conversation_chat_completed(struct lws *wsi,cJSON *json_str,size_t len){
    (void)wsi;
    (void)len;
    (void)json_str;
    return 0;
}
static int quec_ai_handler_downstream_conversation_chat_failed(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   quec_ai_log(LOG_AI_ERR,"[Event] conversation_chat_failed\n");
   return 0;
}
static int quec_ai_handler_downstream_error(struct lws *wsi,cJSON *json_str,size_t len){
    (void)wsi;
    (void)len;
    cJSON *code_item = cJSON_GetObjectItem(json_str, "code");
    cJSON *msg_item = cJSON_GetObjectItem(json_str, "msg");
    int code = cJSON_IsNumber(code_item) ? code_item->valueint : -1;
    const char *msg = (cJSON_IsString(msg_item) && msg_item->valuestring) ? msg_item->valuestring : "";
    quec_ai_log(LOG_AI_ERR, "[Error] Code: %d, Msg: %s\n", code, msg);
    return cJSON_IsNumber(code_item) && cJSON_IsString(msg_item) ? 0 : -1;
}
static int quec_ai_handler_downstream_input_audio_buffer_completed(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   return 0;
}
static int quec_ai_handler_downstream_input_audio_buffer_cleared(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   return 0;
}
static int quec_ai_handler_downstream_conversation_chat_canceled(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)len;
   (void)json_str;
   return 0;
}
static int quec_ai_handler_downstream_conversation_audio_transcript_update(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   return 0;
}
static int quec_ai_handler_downstream_conversation_audio_transcript_completed(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   return 0;
}
static int quec_ai_handler_downstream_conversation_chat_requires_action(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   return 0;
}
static int quec_ai_handler_downstream_conversation_cleared(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   return 0;
}
static int quec_ai_handler_downstream_input_audio_buffer_speech_started(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
    quec_ai_audio_play_flush();
    quec_ai_log(LOG_AI_ERR,"[Event] input_audio_buffer.speech_started\n");
    //     	int16_t tag_value = -32767;  
    //  fwrite(&tag_value, sizeof(int16_t), 1, audio_file_1);
    // 	fflush(audio_file_1); 

    return 0;
}

static int quec_ai_handler_downstream_input_audio_buffer_stopped(struct lws *wsi,cJSON *json_str,size_t len){
   (void)wsi;
   (void)json_str;
   (void)len;
   quec_ai_log(LOG_AI_ERR,"[Event] _input_audio_buffer_stopped\n");
   gettimeofday(&maix_mic_end_time, NULL);
    // 	int16_t tag_value = 32767;  
    //  fwrite(&tag_value, sizeof(int16_t), 1, audio_file_1);
    // 	fflush(audio_file_1); 
   return 0;
}

quec_ai_coze_handle_downstream_map coze_downstream_handle_map[HANDLER_EVENT_NUM]={
    {"conversation.audio.delta"         ,       quec_ai_handler_downstream_conversation_audio_delta},
    {"chat.created",                            quec_ai_handler_downstream_chat_created},
    {"chat.updated",                            quec_ai_handler_downstream_chat_updated},
    {"conversation.chat.created",               quec_ai_handler_downstream_conversation_chat_created},
    {"conversation.chat.in_progress",           quec_ai_handler_downstream_conversation_chat_in_progress},
    {"conversation.message.delta",              quec_ai_handler_downstream_conversation_message_delta},
    {"conversation.audio.sentence_start",       quec_ai_handler_downstream_conversation_audio_sentence_start},
    {"conversation.message.completed",          quec_ai_handler_downstream_conversation_message_completed},
    {"conversation.audio.completed",            quec_ai_handler_downstream_conversation_audio_completed},
    {"conversation.chat.completed",             quec_ai_handler_downstream_conversation_chat_completed},
    {"conversation.chat.failed",                quec_ai_handler_downstream_conversation_chat_failed},
    {"error",                                   quec_ai_handler_downstream_error},
    {"input_audio_buffer.completed",            quec_ai_handler_downstream_input_audio_buffer_completed},
    {"input_audio_buffer.cleared",              quec_ai_handler_downstream_input_audio_buffer_cleared},
    {"conversation.chat.canceled",              quec_ai_handler_downstream_conversation_chat_canceled},
    {"conversation.audio_transcript.update",    quec_ai_handler_downstream_conversation_audio_transcript_update},
    {"conversation.audio_transcript.completed", quec_ai_handler_downstream_conversation_audio_transcript_completed},
    {"conversation.chat.requires_action",       quec_ai_handler_downstream_conversation_chat_requires_action},
    {"conversation.cleared",                    quec_ai_handler_downstream_conversation_cleared},
    {"input_audio_buffer.speech_started",       quec_ai_handler_downstream_input_audio_buffer_speech_started},
    {"input_audio_buffer.speech_stopped",       quec_ai_handler_downstream_input_audio_buffer_stopped},
};


int quec_ai_coze_handle_downstream_event(struct lws *wsi,char *json_str,size_t len) {
    // 确保信号量已初始化（只在第一次调用时初始化）
    init_interrupt_sem_once();
    int ret=0;
    //quec_ai_log(LOG_AI_INFO, "[DOWNSTREAM RAW] %s\n", json_str);
    if (json_str == NULL || len == 0) {
        return -1;
    }
    cJSON *root = cJSON_ParseWithLength(json_str, len);

    if (!root) return -1;
    cJSON *event_type_item = cJSON_GetObjectItem(root, "event_type");
    if (!event_type_item || !cJSON_IsString(event_type_item) || event_type_item->valuestring == NULL) {
        quec_ai_log(LOG_AI_ERR, "downstream event_type invalid\n");
        cJSON_Delete(root);
        return -1;
    }
    const char *event_type = event_type_item->valuestring;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        quec_ai_log(LOG_AI_ERR, "downstream data missing\n");
        cJSON_Delete(root);
        return -1;
    }

    long unsigned int i = 0;
    for(i = 0; i<sizeof(coze_downstream_handle_map)/sizeof(coze_downstream_handle_map[0]);i++){
        if ( coze_downstream_handle_map[i].event_type != NULL && strlen(coze_downstream_handle_map[i].event_type) > 0 && coze_downstream_handle_map[i].func != NULL ){
            if ( strcmp(coze_downstream_handle_map[i].event_type,event_type) == 0){
                if(i==1 ||i==19 ||i==20)
                {
                    quec_ai_log(LOG_AI_INFO,"downstream event matched: %s\n", event_type);
                }
                ret=coze_downstream_handle_map[i].func(wsi,data,len);
                cJSON_Delete(root);
                return ret;
            }
        }
    }
    cJSON_Delete(root);
    return 0;
}