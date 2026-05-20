#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <curl/curl.h>

#include "ai_log.h"
#include "ai_websocket_coze.h"
#include "ai_common_base64.h"
#include "ai_websocket_config.h"
#include "ai_common_shm.h"

#define UPSTREAM_NUM        20
#ifndef UNUSED
#define UNUSED(x)           (void)x
#endif


static pthread_mutex_t g_event_id_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned long g_event_id_counter = 0;

static void generate_event_id(char *id, size_t id_size) {
    if (id == NULL || id_size == 0) {
        return;
    }

    pthread_mutex_lock(&g_event_id_mutex);
    unsigned long counter = ++g_event_id_counter;
    pthread_mutex_unlock(&g_event_id_mutex);

    snprintf(id, id_size, "event_%ld_%ld_%lu", (long)time(NULL), (long)clock(), counter);
}

static int quec_ai_websocket_send_json_data(struct lws *wsi,cJSON *data,bool output ){
    int len = -1;
    unsigned char *buf = NULL;
    // 检查WebSocket连接状态
    if (!wsi) {
        quec_ai_log(LOG_AI_ERR, "WebSocket connection is NULL\n");
        return -1;
    }
    // 检查发送管道是否阻塞
    if (lws_send_pipe_choked(wsi)) {
        quec_ai_log(LOG_AI_ERR, "WebSocket send pipe is choked, need to wait\n");
        return -1;
    }
	char *json_str = cJSON_PrintUnformatted(data);
    if (!json_str) {
        return -1;
    }
    size_t json_len = strlen(json_str);
    buf = (unsigned char *)malloc(LWS_PRE + json_len + 1);
    if (!buf) {
        quec_ai_log(LOG_AI_ERR, "WebSocket send buffer malloc failed\n");
        goto exit;
    }
    memcpy(&buf[LWS_PRE], json_str, json_len + 1);
    if ( output ){
        quec_ai_log(LOG_AI_INFO,"quec_ai_websocket_send_json_data:  %s\n",json_str);
    }
    len = lws_write(wsi, &buf[LWS_PRE], json_len, LWS_WRITE_TEXT);
    if (len < 0) {
        quec_ai_log(LOG_AI_ERR, "WebSocket send failed==========: %d\n", len);
        lws_callback_on_writable(wsi);
    }
exit:
    free(buf);
    free(json_str);
    return len;
}

static int quec_ai_handler_upstream_chat_update(struct lws *wsi,char *event_type,char *data,int data_len)
{
    UNUSED(wsi);
    UNUSED(data);
    UNUSED(data_len);

    quec_ai_config *config = quec_ai_get_config();
    if (!config) {
        quec_ai_log(LOG_AI_ERR, "Failed to get config\n");
        return -1;
    }

    cJSON *config_data = cJSON_CreateObject();
    cJSON *chat_config = cJSON_CreateObject();
    cJSON *parameters = cJSON_CreateObject();  // 新增 parameters 对象
    cJSON *voice_processing_config = cJSON_CreateObject(); // 声纹处理配置
    cJSON *voice_print_config = cJSON_CreateObject(); // 新增声纹识别配置

    cJSON *root = cJSON_CreateObject();
    cJSON *input_audio = cJSON_CreateObject();
    cJSON *output_audio = cJSON_CreateObject();
    cJSON *pcm_config = cJSON_CreateObject();
    cJSON *limit_config = cJSON_CreateObject();
    cJSON *subscriptions = cJSON_CreateArray();
    cJSON *turn_detection = cJSON_CreateObject();
    char event_id[64];
    generate_event_id(event_id, sizeof(event_id));

    cJSON_AddStringToObject(root, "id", event_id);
    cJSON_AddStringToObject(root, "event_type", event_type);

    cJSON_AddBoolToObject(chat_config, "auto_save_history", 1);

     // 获取配置
    cJSON_AddStringToObject(chat_config, "user_id", config->uid);
    // 添加 parameters 到 chat_config
    cJSON_AddItemToObject(chat_config, "parameters", parameters);
    // 设置 accessToken 参数
    if (config && config->parameters) {
        cJSON *stored_parameters = cJSON_Parse(config->parameters);
        if (stored_parameters) {
            cJSON_AddStringToObject(parameters, "accessToken", config->token);
            
            cJSON *item = stored_parameters->child;
            while (item) {
                if (strcmp(item->string, "accessToken") != 0) {
                    cJSON_AddItemToObject(parameters, item->string, cJSON_Duplicate(item, 1));
                }
                item = item->next;
            }
            cJSON_Delete(stored_parameters);
        } else {
            cJSON_AddStringToObject(parameters, "accessToken", config->token);
        }
    } else {
        cJSON_AddStringToObject(parameters, "accessToken", config->token);
    }

    // if (config && config->voice_processing_config) {
    //     cJSON *voice_processing_json = cJSON_Parse(config->voice_processing_config);
    //     if (voice_processing_json) {
    //         // 读取声纹识别开关
    //         cJSON *enable_pdns = cJSON_GetObjectItem(voice_processing_json, "enable_pdns");
    //         if (enable_pdns && cJSON_IsBool(enable_pdns)) {
    //             cJSON_AddBoolToObject(voice_processing_config, "enable_pdns", cJSON_IsTrue(enable_pdns));
    //         } else {
    //             cJSON_AddBoolToObject(voice_processing_config, "enable_pdns", false);
    //         }
    //         cJSON_Delete(voice_processing_json);
    //     } else {
    //         quec_ai_log(LOG_AI_INFO, "Failed to parse voice_processing_config JSON configuration");
    //         cJSON_AddBoolToObject(voice_processing_config, "enable_pdns", false);
    //     }
    // } else {
    //     quec_ai_log(LOG_AI_INFO, "No voice_processing_config found");
    //     cJSON_AddBoolToObject(voice_processing_config, "enable_pdns", false);
    // }
    cJSON_AddBoolToObject(voice_processing_config, "enable_ans", true);
    if (config && config->voice_print_config) {
        cJSON *voice_print_json = cJSON_Parse(config->voice_print_config);
        if (voice_print_json) {
            // 先获取 identityConfig 对象
            cJSON *identityConfig = cJSON_GetObjectItem(voice_print_json, "identityConfig");
            if (identityConfig && cJSON_IsObject(identityConfig)) {
                // 从 identityConfig 中读取配置参数
                cJSON *groupId = cJSON_GetObjectItem(identityConfig, "groupId");
                if (groupId && cJSON_IsString(groupId)) {
                    cJSON_AddStringToObject(voice_print_config, "group_id", groupId->valuestring);
                } else {
                    quec_ai_log(LOG_AI_INFO, "Missing groupId in identityConfig");
                    cJSON_AddStringToObject(voice_print_config, "group_id", "");
                }
                
                cJSON *score = cJSON_GetObjectItem(identityConfig, "score");
                if (score && cJSON_IsNumber(score)) {
                    cJSON_AddNumberToObject(voice_print_config, "score", score->valueint);
                } else {
                    cJSON_AddNumberToObject(voice_print_config, "score", 40);
                }
                
                cJSON *reuseVoiceInfo = cJSON_GetObjectItem(identityConfig, "reuseVoiceInfo");
                if (reuseVoiceInfo && cJSON_IsBool(reuseVoiceInfo)) {
                    cJSON_AddBoolToObject(voice_print_config, "reuse_voice_info", cJSON_IsTrue(reuseVoiceInfo));
                } else {
                    cJSON_AddBoolToObject(voice_print_config, "reuse_voice_info", false);
                }
            } else {
                quec_ai_log(LOG_AI_INFO, "identityConfig not found in voice_print_config");
                // 设置默认值
                cJSON_AddStringToObject(voice_print_config, "group_id", "");
                cJSON_AddNumberToObject(voice_print_config, "score", 40);
                cJSON_AddBoolToObject(voice_print_config, "reuse_voice_info", false);
            }
            
            cJSON_Delete(voice_print_json);
        } else {
            quec_ai_log(LOG_AI_INFO, "Failed to parse voice_print_config JSON configuration");
            // 设置默认值
            cJSON_AddStringToObject(voice_print_config, "group_id", "");
            cJSON_AddNumberToObject(voice_print_config, "score", 40);
            cJSON_AddBoolToObject(voice_print_config, "reuse_voice_info", false);
        }
    } else {
        quec_ai_log(LOG_AI_INFO, "No voice_print_config found");
        // 设置默认值
        cJSON_AddStringToObject(voice_print_config, "group_id", "");
        cJSON_AddNumberToObject(voice_print_config, "score", 40);
        cJSON_AddBoolToObject(voice_print_config, "reuse_voice_info", false);
    }

    cJSON_AddStringToObject(input_audio, "format", "pcm");
    cJSON_AddStringToObject(input_audio, "codec", "pcm");
    cJSON_AddNumberToObject(input_audio, "sample_rate", 16000);
    cJSON_AddNumberToObject(input_audio, "channel", 1);
    cJSON_AddNumberToObject(input_audio, "bit_depth", 16);

    cJSON_AddStringToObject(output_audio, "codec", "pcm");

    cJSON_AddItemToObject(output_audio, "pcm_config", pcm_config);
    cJSON_AddNumberToObject(pcm_config, "sample_rate", 16000);
    cJSON_AddNumberToObject(pcm_config, "frame_size_ms", 50);

    cJSON_AddItemToObject(pcm_config, "limit_config", limit_config);
    cJSON_AddNumberToObject(limit_config, "period", 1);
    cJSON_AddNumberToObject(limit_config, "max_frame_num", 22);

    cJSON_AddNumberToObject(output_audio, "speech_rate", 0);
    cJSON_AddStringToObject(output_audio, "voice_id", "7426720361733046281");

    //subcription the event
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("error"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.message.delta"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.message.completed"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.message.create"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.audio.sentence_start"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.audio.delta"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.audio.completed"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.chat.completed")); 
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.chat.canceled")); // 事件后，表示扣子服务器打断了智能体说话
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("chat.updated"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("chat.created"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("input_audio_buffer.speech_started")); //表示扣子服务端识别到用户正在说话
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("input_audio_buffer.speech_stopped"));
    cJSON_AddItemToArray(subscriptions, cJSON_CreateString("conversation.chat.failed"));

    cJSON_AddStringToObject(turn_detection, "type", "server_vad");
    cJSON_AddNumberToObject(turn_detection,"silence_duration_ms",300);

    cJSON_AddItemToObject(root, "data", config_data);
    cJSON_AddBoolToObject(config_data,"need_play_prologue",true);
    cJSON_AddItemToObject(config_data, "chat_config", chat_config);
    cJSON_AddItemToObject(config_data, "event_subscriptions", subscriptions);
    cJSON_AddItemToObject(config_data, "input_audio", input_audio);
    cJSON_AddItemToObject(config_data, "output_audio", output_audio);
    cJSON_AddItemToObject(config_data, "turn_detection", turn_detection);
    cJSON_AddItemToObject(config_data, "voice_processing_config", voice_processing_config);  //添加主动噪声抑制
    cJSON_AddItemToObject(config_data, "voice_print_config", voice_print_config); // 添加声纹识别配置

    int ret = quec_ai_websocket_send_json_data(wsi,root,false);
    cJSON_Delete(root);

    return ret;
}

// 发送语音接口，将语音数据放入data 和len，然后调用这个接口发送,可以将
static int quec_ai_handler_upstream_input_audio_buffer_append(struct lws *wsi,char *event_type,char *data,int data_len){
    UNUSED(wsi);
    UNUSED(event_type);
    UNUSED(data);
    UNUSED(data_len);
    

    cJSON *root_data = cJSON_CreateObject();
    char event_id[64];
    generate_event_id(event_id, sizeof(event_id));
    cJSON_AddStringToObject(root_data, "id", event_id);
    cJSON_AddStringToObject(root_data, "event_type", event_type);

    cJSON *audio_cjson_data = cJSON_CreateObject();
    cJSON_AddItemToObject(root_data, "data", audio_cjson_data);

    size_t rem = data_len % 3;
    size_t base64_audio_len = ((data_len / 3) * 4) + (rem > 0 ? 4 : 0) + 1;

    unsigned char *base64_audio = (unsigned char *)malloc(base64_audio_len);
    if (!base64_audio) {
        quec_ai_log(LOG_AI_ERR, "base64 audio malloc failed\n");
        cJSON_Delete(root_data);
        return -1;
    }
    base64_encode_openssl(base64_audio, base64_audio_len, &base64_audio_len, (const char unsigned *)data, data_len);

    cJSON_AddStringToObject(audio_cjson_data, "delta", (const char * const)base64_audio);
    quec_ai_websocket_send_json_data(wsi,root_data,false);
    free(base64_audio); 
    cJSON_Delete(root_data);
    
    return 0;
}
// 当需要打断时，调用此接口告诉口子服务器，打断下发的语音。
static int quec_ai_handler_upstream_input_audio_buffer_complete(struct lws *wsi,char *event_type,char *data,int data_len){
    UNUSED(wsi);
    UNUSED(event_type);
    UNUSED(data);
    UNUSED(data_len);
    cJSON *root_data = cJSON_CreateObject();
    char event_id[64];
    generate_event_id(event_id, sizeof(event_id));
	cJSON_AddStringToObject(root_data, "id", event_id);
	cJSON_AddStringToObject(root_data, "event_type", event_type);
	quec_ai_websocket_send_json_data(wsi,root_data,false);
    cJSON_Delete(root_data);
    return 0;
}

static int quec_ai_handler_upstream_input_audio_buffer_clear(struct lws *wsi,char *event_type,char *data,int data_len){
    UNUSED(wsi);
    UNUSED(event_type);
    UNUSED(data);
    UNUSED(data_len);
    return 0;
}

typedef struct {
    char *data;
    size_t len;
} upload_response_t;

static size_t upload_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    upload_response_t *response = (upload_response_t *)userdata;
    size_t chunk_len = size * nmemb;
    if (size != 0 && chunk_len / size != nmemb) {
        return 0;
    }
    if (chunk_len > ((size_t)-1) - response->len - 1) {
        return 0;
    }

    char *new_data = realloc(response->data, response->len + chunk_len + 1);
    if (!new_data) {
        return 0;
    }

    response->data = new_data;
    memcpy(response->data + response->len, ptr, chunk_len);
    response->len += chunk_len;
    response->data[response->len] = '\0';
    return chunk_len;
}

static const char *get_coze_auth_token() {
    quec_ai_config *config = quec_ai_get_config();
    if (config == NULL) {
        quec_ai_log(LOG_AI_ERR, "Failed to get config");
        return NULL;
    }
    return config->token;
}

// 上传文件到 coze
static char *upload_file_to_coze(const char *file_path) {
    const char *token = get_coze_auth_token();
    CURL *curl = NULL;
    curl_mime *mime = NULL;
    curl_mimepart *part = NULL;
    struct curl_slist *headers = NULL;
    cJSON *response_json = NULL;
    upload_response_t response = {0};
    char *auth_header = NULL;
    char *id = NULL;
    long http_code = 0;

    if (file_path == NULL || file_path[0] == '\0' || token == NULL || token[0] == '\0') {
        quec_ai_log(LOG_AI_ERR, "Invalid file upload parameters\n");
        return NULL;
    }

    response.data = malloc(1);
    if (!response.data) {
        return NULL;
    }
    response.data[0] = '\0';

    curl = curl_easy_init();
    if (!curl) {
        quec_ai_log(LOG_AI_ERR, "Failed to initialize curl\n");
        goto cleanup;
    }

    size_t auth_header_len = strlen("Authorization: Bearer ") + strlen(token) + 1;
    auth_header = malloc(auth_header_len);
    if (!auth_header) {
        goto cleanup;
    }
    snprintf(auth_header, auth_header_len, "Authorization: Bearer %s", token);
    headers = curl_slist_append(headers, auth_header);
    free(auth_header);
    auth_header = NULL;
    if (!headers) {
        quec_ai_log(LOG_AI_ERR, "Failed to append upload authorization header\n");
        goto cleanup;
    }

    mime = curl_mime_init(curl);
    if (!mime) {
        quec_ai_log(LOG_AI_ERR, "Failed to initialize upload mime\n");
        goto cleanup;
    }
    part = curl_mime_addpart(mime);
    if (!part || curl_mime_name(part, "file") != CURLE_OK || curl_mime_filedata(part, file_path) != CURLE_OK) {
        quec_ai_log(LOG_AI_ERR, "Failed to prepare upload file part\n");
        goto cleanup;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.coze.cn/v1/files/upload");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, upload_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        quec_ai_log(LOG_AI_ERR, "Upload request failed: %s\n", curl_easy_strerror(ret));
        goto cleanup;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code < 200 || http_code >= 300) {
        quec_ai_log(LOG_AI_ERR, "Upload request failed with HTTP code: %ld\n", http_code);
        goto cleanup;
    }

    response_json = cJSON_Parse(response.data);
    if (!response_json) {
        quec_ai_log(LOG_AI_ERR, "Failed to parse upload response\n");
        goto cleanup;
    }
    cJSON *code = cJSON_GetObjectItem(response_json, "code");
    if (!code || !cJSON_IsNumber(code) || code->valueint != 0) {
        quec_ai_log(LOG_AI_ERR, "Upload failed with code: %d\n", code && cJSON_IsNumber(code) ? code->valueint : -1);
        goto cleanup;
    }

    cJSON *data = cJSON_GetObjectItem(response_json, "data");
    if (!data || !cJSON_IsObject(data)) {
        quec_ai_log(LOG_AI_ERR, "No data in upload response\n");
        goto cleanup;
    }

    cJSON *file_id = cJSON_GetObjectItem(data, "id");
    if (!file_id || !cJSON_IsString(file_id) || file_id->valuestring == NULL) {
        quec_ai_log(LOG_AI_ERR, "No file ID in upload response\n");
        goto cleanup;
    }

    id = strdup(file_id->valuestring);

cleanup:
    cJSON_Delete(response_json);
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    free(auth_header);
    free(response.data);
    return id;
}

static char *read_complete_content(void) {
    FILE *fp = fopen("/tmp/complete_content.txt", "r");
    if (!fp) {
        quec_ai_log(LOG_AI_ERR, "Failed to open complete content file\n");
        return NULL;
    }
    
    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size <= 0) {
        quec_ai_log(LOG_AI_ERR, "Complete content file is empty\n");
        fclose(fp);
        return NULL;
    }
    
    // 分配内存并读取内容
    char *complete_content = malloc(file_size + 1);
    if (!complete_content) {
        quec_ai_log(LOG_AI_ERR, "Memory allocation failed for complete content\n");
        fclose(fp);
        return NULL;
    }
    
    size_t bytes_read = fread(complete_content, 1, file_size, fp);
    complete_content[bytes_read] = '\0';
    
    fclose(fp);
    
    quec_ai_log(LOG_AI_INFO, "Read complete content: %s\n", complete_content);
    return complete_content;
}

// 根据文件路径识别类型的函数
static const char* get_file_type_from_path(const char *file_path) {
    if (!file_path) return "file";
    
    const char *extension = strrchr(file_path, '.');
    if (!extension) return "file";
    
    extension++; // 跳过点号
    
    if (strcasecmp(extension, "jpg") == 0 || strcasecmp(extension, "jpeg") == 0 ||
        strcasecmp(extension, "png") == 0 || strcasecmp(extension, "gif") == 0 ||
        strcasecmp(extension, "bmp") == 0 || strcasecmp(extension, "webp") == 0) {
        return "image";
    }
    else if (strcasecmp(extension, "mp3") == 0 || strcasecmp(extension, "wav") == 0 ||
             strcasecmp(extension, "aac") == 0 || strcasecmp(extension, "flac") == 0 ||
             strcasecmp(extension, "m4a") == 0 || strcasecmp(extension, "ogg") == 0) {
        return "audio";
    }
    else if (strcasecmp(extension, "txt") == 0 || strcasecmp(extension, "pdf") == 0 ||
             strcasecmp(extension, "doc") == 0 || strcasecmp(extension, "docx") == 0) {
        return "file";
    }
    else {
        return "file";
    }
}

static int quec_ai_handler_upstream_conversation_message_create(struct lws *wsi,char *event_type,char *data,int data_len){
    // UNUSED(wsi);
    UNUSED(data);
    UNUSED(data_len);

    int ret = -1;
    char *text_content = NULL;
    char *file_id = NULL;
    char *content_str = NULL;
    cJSON *content_array = NULL;
    cJSON *text_item = NULL;
    cJSON *file_item = NULL;
    cJSON *submit_message = NULL;
    cJSON *data_obj = NULL;

    // 从文件中读取完整content内容
    text_content = read_complete_content();
    if (!text_content) {
        quec_ai_log(LOG_AI_ERR, "Failed to read complete content from file\n");
        goto cleanup;
    }


    char *screenshot_path = "/tmp/1.jpg";
    quec_ai_log(LOG_AI_INFO, "finish take screenshot\n");

    const char *file_type = get_file_type_from_path(screenshot_path);
    file_id = upload_file_to_coze(screenshot_path);
    if (!file_id) {
        quec_ai_log(LOG_AI_ERR, "Failed to upload file to coze\n");
        goto cleanup;
    }

    quec_ai_log(LOG_AI_INFO, "success upload file to coze\n");

    //free(screenshot_path);
    // 构建新的消息内容
    content_array = cJSON_CreateArray();
    if (!content_array) {
        quec_ai_log(LOG_AI_ERR, "Failed to create content array\n");
        goto cleanup;
    }

    text_item = cJSON_CreateObject();
    if (!text_item) {
        quec_ai_log(LOG_AI_ERR, "Failed to create text item\n");
        goto cleanup;
    }
    cJSON_AddStringToObject(text_item, "type", "text");

    char response_text[512];
    snprintf(response_text, sizeof(response_text),
             "%s", text_content);

    cJSON_AddStringToObject(text_item, "text", response_text);
    cJSON_AddItemToArray(content_array, text_item);
    text_item = NULL;

    file_item = cJSON_CreateObject();
    if (!file_item) {
        quec_ai_log(LOG_AI_ERR, "Failed to create file item\n");
        goto cleanup;
    }
    cJSON_AddStringToObject(file_item, "type", file_type);
    cJSON_AddStringToObject(file_item, "file_id", file_id);
    cJSON_AddItemToArray(content_array, file_item);
    file_item = NULL;

    content_str = cJSON_PrintUnformatted(content_array);
    if (!content_str) {
        quec_ai_log(LOG_AI_ERR, "Failed to create content string\n");
        goto cleanup;
    }

    quec_ai_log(LOG_AI_INFO, "Submission content: %s\n", content_str);

    // 构建完整的提交消息
    submit_message = cJSON_CreateObject();
    if (!submit_message) {
        quec_ai_log(LOG_AI_ERR, "Failed to create submit message\n");
        goto cleanup;
    }

    char event_id[64];
    generate_event_id(event_id, sizeof(event_id));
    cJSON_AddStringToObject(submit_message, "id", event_id);

    // 设置事件类型
    cJSON_AddStringToObject(submit_message, "event_type", event_type);

    // 构建 data 对象
    data_obj = cJSON_CreateObject();
    if (!data_obj) {
        quec_ai_log(LOG_AI_ERR, "Failed to create data object\n");
        goto cleanup;
    }
    cJSON_AddStringToObject(data_obj, "role", "user");  // 用户上传截图
    cJSON_AddStringToObject(data_obj, "content_type", "object_string");
    cJSON_AddStringToObject(data_obj, "content", content_str);

    cJSON_AddItemToObject(submit_message, "data", data_obj);
    data_obj = NULL;

    // 发送消息
    ret = quec_ai_websocket_send_json_data(wsi, submit_message, true);
    if (ret >= 0) {
        ret = 0;
    }

cleanup:
    if (data_obj) {
        cJSON_Delete(data_obj);
    }
    if (submit_message) {
        cJSON_Delete(submit_message);
    }
    if (file_item) {
        cJSON_Delete(file_item);
    }
    if (text_item) {
        cJSON_Delete(text_item);
    }
    if (content_array) {
        cJSON_Delete(content_array);
    }
    free(content_str);
    free(file_id);
    free(text_content);
    return ret;
}
static int quec_ai_handler_upstream_conversation_clear(struct lws *wsi,char *event_type,char *data,int data_len){
    UNUSED(wsi);
    UNUSED(event_type);
    UNUSED(data);
    UNUSED(data_len);
    return 0;
}
static int quec_ai_handler_upstream_conversation_chat_submit_tool_outputs(struct lws *wsi,char *event_type,char *data,int data_len){
    UNUSED(wsi);
    UNUSED(event_type);
    UNUSED(data);
    UNUSED(data_len);
    return 0;
}
static int quec_ai_handler_upstream_conversation_chat_cancel(struct lws *wsi,char *event_type,char *data,int data_len){
    UNUSED(wsi);
    UNUSED(event_type);
    UNUSED(data);
    UNUSED(data_len);

    // cJSON *root_data = cJSON_CreateObject();
    // cJSON_AddStringToObject(root_data, "id", event_id);
    // cJSON_AddStringToObject(root_data, "event_type", event_type);
    
    // // conversation.chat.cancel 通常不需要额外的 data 字段
    // // 如果需要空对象，可以添加
    // cJSON *empty_data = cJSON_CreateObject();
    // cJSON_AddItemToObject(root_data, "data", empty_data);
    
    // quec_ai_websocket_send_json_data(wsi, root_data, false);
    
    // cJSON_Delete(root_data);
    
    // quec_ai_log(LOG_AI_INFO,"Sent conversation.chat.cancel event to interrupt agent output");

    return 0;
}
static int quec_ai_handler_upstream_input_text_generate_audio(struct lws *wsi,char *event_type,char *data,int data_len){
    UNUSED(wsi);
    UNUSED(event_type);
    UNUSED(data);
    UNUSED(data_len);
    return 0;
}

quec_ai_coze_handle_upstream_map quec_ai_upstream_map[UPSTREAM_NUM]={
    {"chat.update",quec_ai_handler_upstream_chat_update},
    {"input_audio_buffer.append",quec_ai_handler_upstream_input_audio_buffer_append},
    {"input_audio_buffer.complete",quec_ai_handler_upstream_input_audio_buffer_complete},
    {"input_audio_buffer.clear",quec_ai_handler_upstream_input_audio_buffer_clear},
    {"conversation.message.create",quec_ai_handler_upstream_conversation_message_create},
    {"conversation.clear",quec_ai_handler_upstream_conversation_clear},
    {"conversation.chat.submit_tool_outputs",quec_ai_handler_upstream_conversation_chat_submit_tool_outputs},
    {"conversation.chat.cancel",quec_ai_handler_upstream_conversation_chat_cancel},
    {"input_text.generate_audio",quec_ai_handler_upstream_input_text_generate_audio},
    
};



int quec_ai_coze_handler_upstream_event(struct lws *wsi,char *event_type,char *data,int data_len){
    if (event_type == NULL) {
        quec_ai_log(LOG_AI_ERR, "upstream event_type is NULL\n");
        return -1;
    }

    unsigned long i = 0 ;
    for( i = 0; i < sizeof(quec_ai_upstream_map)/sizeof(quec_ai_upstream_map[0]);i++){
        if ( quec_ai_upstream_map[i].event_type !=NULL && strlen(quec_ai_upstream_map[i].event_type) > 0 && quec_ai_upstream_map[i].func != NULL) {
            if ( strcmp(quec_ai_upstream_map[i].event_type,event_type) == 0){
                return  quec_ai_upstream_map[i].func(wsi,event_type,data,data_len);
            }
        }
    }
    return 0;

}