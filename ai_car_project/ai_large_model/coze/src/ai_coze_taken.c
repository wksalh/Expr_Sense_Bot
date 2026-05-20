#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <jansson.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "ai_websocket_coze.h"
#include "ai_log.h"

#define QIOT_MQTT_PRODUCT_KEY_ENV "QIOT_MQTT_PRODUCT_KEY"
#define QIOT_MQTT_ACCESS_SECRET_ENV "QIOT_MQTT_ACCESS_SECRET"
#define QIOT_MQTT_AUTHORIZATION_ENV "QIOT_MQTT_AUTHORIZATION"
#define COZE_UID_ENV "COZE_UID"

// 用于存储curl响应数据
struct string {
    char *ptr;
    size_t len;
};

// 保存token到config.json文件
int save_coze_info_to_config(coze_connect_data_t *coze_data, const char *config_path)
{
    if (coze_data == NULL || config_path == NULL) return -1;
    
    // 创建基本JSON结构
    json_t *root = json_object();
    if (coze_data->token) 
        json_object_set_new(root, "coze_access_token", json_string(coze_data->token));
    if (coze_data->botId) 
        json_object_set_new(root, "coze_bot_id", json_string(coze_data->botId));
    if (coze_data->voiceId) 
        json_object_set_new(root, "coze_voice_id", json_string(coze_data->voiceId));
    if (coze_data->conversationId) 
        json_object_set_new(root, "coze_conversation_id", json_string(coze_data->conversationId));
    
    const char *coze_uid = getenv(COZE_UID_ENV);
    if (coze_uid && coze_uid[0] != '\0') {
        json_object_set_new(root, "coze_uid", json_string(coze_uid));
        quec_ai_log(LOG_AI_INFO,"Using configured Coze uid\n");
    } else if (coze_data->uid && strlen(coze_data->uid) > 0) {
        json_object_set_new(root, "coze_uid", json_string(coze_data->uid));
        quec_ai_log(LOG_AI_INFO,"Using API returned uid\n");
    }

    json_object_set_new(root, "coze_speech_rate", json_integer(coze_data->speechRate));
    json_object_set_new(root, "coze_role_id", json_integer(coze_data->roleId));
    
    char coze_path[512];
    const char *default_bot_id = "7553834038902112256";
    const char *bot_id_to_use = (coze_data->botId && strlen(coze_data->botId) > 0) ? 
                                coze_data->botId : default_bot_id;
    
    snprintf(coze_path, sizeof(coze_path), 
             "/v1/chat?bot_id=%s", bot_id_to_use);

     // 固定写入地址、路径、端口
    json_object_set_new(root, "coze_address", json_string("ws.coze.cn"));
    json_object_set_new(root, "coze_path", json_string(coze_path));
    json_object_set_new(root, "coze_port", json_integer(443));

    // 提取并保存 parameters 字段
    if (coze_data->parameters && strlen(coze_data->parameters) > 0) {
        json_error_t error;
        json_t *parameters_json = json_loads(coze_data->parameters, 0, &error);
        if (parameters_json) {
            json_object_set_new(root, "coze_parameters", parameters_json);
        } else {
            fprintf(stderr, "Failed to parse parameters JSON: %s\n", error.text);
            json_object_set_new(root, "coze_parameters", json_string(coze_data->parameters));
        }
    }
    // 保存 voicePrint 字段（声纹配置）
    if (coze_data->voicePrint && strlen(coze_data->voicePrint) > 0) {
        json_error_t error;
        json_t *voice_print_json = json_loads(coze_data->voicePrint, 0, &error);
        if (voice_print_json) {
            json_object_set_new(root, "coze_voice_print", voice_print_json);
            quec_ai_log(LOG_AI_INFO,"Voice Print config saved to file\n");
        } else {
            fprintf(stderr, "Failed to parse voicePrint JSON: %s\n", error.text);
        }
    } else {
        quec_ai_log(LOG_AI_INFO,"Voice Print: No data provided, skipping save\n");
    }
    
    // 保存 voiceProcessingConfig 字段
    if (coze_data->voiceProcessingConfig && strlen(coze_data->voiceProcessingConfig) > 0) {
        json_error_t error;
        json_t *voice_processing_json = json_loads(coze_data->voiceProcessingConfig, 0, &error);
        if (voice_processing_json) {
            json_object_set_new(root, "coze_voice_processing_config", voice_processing_json);
        } else {
            fprintf(stderr, "Failed to parse voiceProcessingConfig JSON: %s\n", error.text);
            json_t *default_voice_processing = json_object();
            json_object_set_new(default_voice_processing, "enable_pdns", json_false());
            json_object_set_new(root, "coze_voice_processing_config", default_voice_processing);
        }
    } else {
        json_t *default_voice_processing = json_object();
        json_object_set_new(default_voice_processing, "enable_pdns", json_false());
        json_object_set_new(root, "coze_voice_processing_config", default_voice_processing);
    }

    // 直接写入文件
    FILE *file = fopen(config_path, "w");
    if (!file) {
        fprintf(stderr, "Cannot open file: %s\n", config_path);
        json_decref(root);
        return -1;
    }
    
    char *json_str = json_dumps(root, JSON_INDENT(4));
    if (fprintf(file, "%s", json_str) < 0) {
        fprintf(stderr, "Write failed\n");
    }
    
    fclose(file);
    free(json_str);
    json_decref(root);
    
    quec_ai_log(LOG_AI_INFO,"Config saved successfully\n");
    return 0;
}

// 初始化字符串结构
static int init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(s->len + 1);
    if (s->ptr == NULL) {
        fprintf(stderr, "malloc() failed\n");
        return -1;
    }
    s->ptr[0] = '\0';
    return 0;
}

// curl写回调函数
static size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
    size_t add_len = size * nmemb;
    size_t new_len = s->len + add_len;
    char *new_ptr = realloc(s->ptr, new_len + 1);
    if (new_ptr == NULL) {
        fprintf(stderr, "realloc() failed\n");
        return 0;
    }
    s->ptr = new_ptr;
    memcpy(s->ptr + s->len, ptr, add_len);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return add_len;
}

// 生成时间戳
char* generate_timestamp(void) {
    time_t now = time(NULL);
    char *timestamp = malloc(20);
    if (timestamp == NULL) return NULL;
    
    snprintf(timestamp, 20, "%ld", now);
    return timestamp;
}


// SHA256签名生成
/**
 * @brief 生成Coze API签名
 * @param productKey 产品密钥
 * @param deviceKey 设备密钥  
 * @param timestamp 时间戳（毫秒）
 * @param accessSecret 访问密钥
 * @return SHA256签名（64位十六进制字符串），需要调用者释放内存
 */
char* generate_coze_signature(const char *productKey, const char *deviceKey, 
                             const char *timestamp, const char *accessSecret) {
    if (productKey == NULL || deviceKey == NULL || 
        timestamp == NULL || accessSecret == NULL) {
        fprintf(stderr, "Error: NULL parameter in generate_coze_signature\n");
        return NULL;
    }
    
    // 计算拼接后字符串的总长度
    size_t total_len = strlen(productKey) + strlen(deviceKey) + 
                       strlen(timestamp) + strlen(accessSecret) + 1;
    
    // 使用动态分配确保足够空间
    char *message = malloc(total_len);
    if (message == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for message\n");
        return NULL;
    }
    
    // 拼接字符串：productKey + deviceKey + timestamp + accessSecret
    snprintf(message, total_len, "%s%s%s%s", productKey, deviceKey, timestamp, accessSecret);
    
    quec_ai_log(LOG_AI_INFO,"Signature input prepared, message length: %zu\n", strlen(message));
    
    // 计算SHA256哈希
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)message, strlen(message), digest);
    
    // 释放拼接的字符串内存
    free(message);
    
    // 转换为十六进制字符串
    char *signature = malloc(65); // 64字符 + 1结束符
    if (signature == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for signature\n");
        return NULL;
    }
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(signature + (i * 2), "%02x", digest[i]);
    }
    signature[64] = '\0';
    
    quec_ai_log(LOG_AI_INFO,"Generated signature\n");

    return signature;
}

static int json_object_set_string_checked(json_t *root, const char *key, const char *value)
{
    json_t *json_value = json_string(value);
    if (json_value == NULL) {
        return -1;
    }
    if (json_object_set(root, key, json_value) != 0) {
        json_decref(json_value);
        return -1;
    }
    json_decref(json_value);
    return 0;
}

static int json_object_set_integer_checked(json_t *root, const char *key, json_int_t value)
{
    json_t *json_value = json_integer(value);
    if (json_value == NULL) {
        return -1;
    }
    if (json_object_set(root, key, json_value) != 0) {
        json_decref(json_value);
        return -1;
    }
    json_decref(json_value);
    return 0;
}

static int copy_json_string_checked(char **dst, json_t *value)
{
    if (!json_is_string(value)) {
        return 0;
    }
    *dst = strdup(json_string_value(value));
    return *dst ? 0 : -1;
}

static int dump_json_checked(char **dst, json_t *value)
{
    if (value == NULL) {
        return 0;
    }
    *dst = json_dumps(value, JSON_COMPACT);
    return *dst ? 0 : -1;
}

void coze_response_free(coze_response_t *response);

// 获取Coze Token
coze_response_t* coze_get_connect_info(const char *productKey, const char *deviceKey, const char *accessSecret, const char *authorization, int roleId) {
    CURL *curl = NULL;
    CURLcode res;
    struct string s = {0};
    coze_response_t *response = NULL;
    json_t *root = NULL;
    json_t *root_resp = NULL;
    char *json_data = NULL;
    char *signature = NULL;
    char *auth_header = NULL;
    struct curl_slist *headers = NULL;

    if (productKey == NULL || deviceKey == NULL || accessSecret == NULL || authorization == NULL ||
        productKey[0] == '\0' || deviceKey[0] == '\0' || accessSecret[0] == '\0' || authorization[0] == '\0') {
        fprintf(stderr, "Missing Coze credential configuration\n");
        return NULL;
    }

    // 生成时间戳（毫秒级）
    char timestamp[20];
    long long ms = (long long)time(NULL) * 1000;
    snprintf(timestamp, sizeof(timestamp), "%lld", ms);

    // 生成签名
    signature = generate_coze_signature(productKey, deviceKey, timestamp, accessSecret);
    if (signature == NULL) {
        fprintf(stderr, "Failed to generate signature\n");
        return NULL;
    }

    quec_ai_log(LOG_AI_INFO,"Generating Coze connect info request, roleId=%d\n", roleId);

    // 构建JSON请求体
    root = json_object();
    if (root == NULL) {
        fprintf(stderr, "Failed to create request JSON object\n");
        goto cleanup;
    }
    if (json_object_set_string_checked(root, "productKey", productKey) != 0 ||
        json_object_set_string_checked(root, "deviceKey", deviceKey) != 0 ||
        json_object_set_string_checked(root, "timestamp", timestamp) != 0 ||
        json_object_set_string_checked(root, "sign", signature) != 0) {
        fprintf(stderr, "Failed to populate request JSON object\n");
        goto cleanup;
    }

    if (roleId > 0 && json_object_set_integer_checked(root, "roleId", roleId) != 0) {
        fprintf(stderr, "Failed to set roleId in request JSON object\n");
        goto cleanup;
    }

    json_data = json_dumps(root, JSON_COMPACT);
    if (json_data == NULL) {
        fprintf(stderr, "Failed to dump request JSON\n");
        goto cleanup;
    }

    quec_ai_log(LOG_AI_INFO,"Coze connect info request JSON prepared\n");

    curl = curl_easy_init();
    if (curl) {
        if (init_string(&s) != 0) {
            goto cleanup;
        }

        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (headers == NULL) {
            fprintf(stderr, "Failed to append Content-Type header\n");
            goto cleanup;
        }
        size_t auth_header_len = strlen("Authorization: ") + strlen(authorization) + 1;
        auth_header = malloc(auth_header_len);
        if (auth_header == NULL) {
            fprintf(stderr, "Failed to allocate Authorization header\n");
            goto cleanup;
        }
        snprintf(auth_header, auth_header_len, "Authorization: %s", authorization);
        struct curl_slist *new_headers = curl_slist_append(headers, auth_header);
        free(auth_header);
        auth_header = NULL;
        if (new_headers == NULL) {
            fprintf(stderr, "Failed to append Authorization header\n");
            goto cleanup;
        }
        headers = new_headers;

        curl_easy_setopt(curl, CURLOPT_URL, "https://aigc-api.iotomp.com/v2/aibiz/openapi/v1/coze/websocketConnectInfo/v1");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(json_data));
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Coze-Client/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        quec_ai_log(LOG_AI_INFO,"Sending request to Coze API...\n");
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            quec_ai_log(LOG_AI_INFO,"Response received from Coze API\n");

            // 解析JSON响应
            json_error_t error;
            root_resp = json_loads(s.ptr, 0, &error);

            if (root_resp) {
                response = malloc(sizeof(*response));
                if (response == NULL) {
                    fprintf(stderr, "Failed to allocate response\n");
                    goto cleanup;
                }
                memset(response, 0, sizeof(*response));

                // 解析基础字段
                json_t *code_obj = json_object_get(root_resp, "code");
                json_t *msg_obj = json_object_get(root_resp, "msg");
                json_t *data_obj = json_object_get(root_resp, "data");

                if (json_is_integer(code_obj)) {
                    response->code = json_integer_value(code_obj);
                }

                if (copy_json_string_checked(&response->msg, msg_obj) != 0) {
                    goto parse_alloc_fail;
                }

                // 解析data对象
                if (data_obj && json_is_object(data_obj)) {
                    response->data = malloc(sizeof(*response->data));
                    if (response->data == NULL) {
                        goto parse_alloc_fail;
                    }
                    memset(response->data, 0, sizeof(*response->data));

                    // 解析data中的各个字段
                    json_t *token = json_object_get(data_obj, "accessToken");
                    json_t *botId = json_object_get(data_obj, "botId");
                    json_t *voiceId = json_object_get(data_obj, "voiceId");
                    json_t *conversationId = json_object_get(data_obj, "conversationId");
                    json_t *uid = json_object_get(data_obj, "uid");
                    json_t *speechRate = json_object_get(data_obj, "speechRate");
                    json_t *parameters = json_object_get(data_obj, "parameters");
                    json_t *hotWords = json_object_get(data_obj, "hotWords");
                    json_t *interruptConfig = json_object_get(data_obj, "interruptConfig");
                    json_t *preAnswerList = json_object_get(data_obj, "preAnswerList");
                    json_t *roleId_obj = json_object_get(data_obj, "roleId");
                    json_t *voiceProcessingConfig = json_object_get(data_obj, "voiceProcessingConfig");
                    json_t *voicePrint = json_object_get(data_obj, "voicePrint");

                    if (copy_json_string_checked(&response->data->token, token) != 0 ||
                        copy_json_string_checked(&response->data->botId, botId) != 0 ||
                        copy_json_string_checked(&response->data->voiceId, voiceId) != 0 ||
                        copy_json_string_checked(&response->data->conversationId, conversationId) != 0 ||
                        copy_json_string_checked(&response->data->uid, uid) != 0) {
                        goto parse_alloc_fail;
                    }
                    if (json_is_integer(speechRate)) {
                        response->data->speechRate = json_integer_value(speechRate);
                    }
                    if (json_is_string(parameters)) {
                        if (copy_json_string_checked(&response->data->parameters, parameters) != 0) {
                            goto parse_alloc_fail;
                        }
                    } else if (dump_json_checked(&response->data->parameters, parameters) != 0) {
                        goto parse_alloc_fail;
                    }
                    if (json_is_string(hotWords)) {
                        if (copy_json_string_checked(&response->data->hotWords, hotWords) != 0) {
                            goto parse_alloc_fail;
                        }
                    } else if (dump_json_checked(&response->data->hotWords, hotWords) != 0) {
                        goto parse_alloc_fail;
                    }
                    if (json_is_string(interruptConfig)) {
                        if (copy_json_string_checked(&response->data->interruptConfig, interruptConfig) != 0) {
                            goto parse_alloc_fail;
                        }
                    } else if (dump_json_checked(&response->data->interruptConfig, interruptConfig) != 0) {
                        goto parse_alloc_fail;
                    }
                    if (json_is_string(preAnswerList)) {
                        if (copy_json_string_checked(&response->data->preAnswerList, preAnswerList) != 0) {
                            goto parse_alloc_fail;
                        }
                    } else if (dump_json_checked(&response->data->preAnswerList, preAnswerList) != 0) {
                        goto parse_alloc_fail;
                    }
                    if (json_is_integer(roleId_obj)) {
                        response->data->roleId = json_integer_value(roleId_obj);
                    }
                    if (json_is_string(voiceProcessingConfig)) {
                        if (copy_json_string_checked(&response->data->voiceProcessingConfig, voiceProcessingConfig) != 0) {
                            goto parse_alloc_fail;
                        }
                    } else if (dump_json_checked(&response->data->voiceProcessingConfig, voiceProcessingConfig) != 0) {
                        goto parse_alloc_fail;
                    }
                    if (json_is_string(voicePrint)) {
                        if (copy_json_string_checked(&response->data->voicePrint, voicePrint) != 0) {
                            goto parse_alloc_fail;
                        }
                    } else if (dump_json_checked(&response->data->voicePrint, voicePrint) != 0) {
                        goto parse_alloc_fail;
                    }
                }
            } else {
                fprintf(stderr, "JSON parse error: %s\n", error.text);
            }
        }
    }

cleanup:
    if (root_resp) {
        json_decref(root_resp);
    }
    if (root) {
        json_decref(root);
    }
    if (headers) {
        curl_slist_free_all(headers);
    }
    if (curl) {
        curl_easy_cleanup(curl);
    }
    free(s.ptr);
    free(json_data);
    free(signature);
    free(auth_header);

    return response;

parse_alloc_fail:
    fprintf(stderr, "Failed to allocate response field\n");
    coze_response_free(response);
    response = NULL;
    goto cleanup;
}

// 释放响应内存
void coze_response_free(coze_response_t *response) {
    if (response) {
        free(response->msg);
        
        if (response->data) {
            free(response->data->token);
            free(response->data->botId);
            free(response->data->voiceId);
            free(response->data->conversationId);
            free(response->data->uid);
            free(response->data->parameters);
            free(response->data->hotWords);
            free(response->data->interruptConfig);
            free(response->data->preAnswerList);
            free(response->data->voiceProcessingConfig);
            free(response->data->voicePrint);
            free(response->data);
        }
        
        free(response);
    }
}

static int get_mac_address(const char *ifname, char *mac_str, size_t mac_str_len) {
    int sockfd;
    struct ifreq ifr;
    unsigned char mac[6];

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    // 设置网卡名称
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    // 获取 MAC 地址
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl");
        close(sockfd);
        return -1;
    }

    // 复制 MAC 地址
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

    // 格式化为字符串（如 "00:55:33:22:55:13"）
    snprintf(mac_str, mac_str_len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    close(sockfd);
    return 0;
}

int quec_ai_coze_token_init()
{
    // 初始化curl全局库
    char dk[18];
    curl_global_init(CURL_GLOBAL_DEFAULT);
    if (get_mac_address("wlan0", dk, sizeof(dk)) == 0) {
        quec_ai_log(LOG_AI_INFO,"wlan0 MAC address: %s\n", dk);
    } else {
        quec_ai_log(LOG_AI_INFO,"Failed to get MAC address for wlan0\n");
        return -1;
    }

    const char *pk = getenv(QIOT_MQTT_PRODUCT_KEY_ENV);
    const char *access_secret = getenv(QIOT_MQTT_ACCESS_SECRET_ENV);
    const char *authorization = getenv(QIOT_MQTT_AUTHORIZATION_ENV);
    int roleId = 0;

    if (pk == NULL || pk[0] == '\0' || access_secret == NULL || access_secret[0] == '\0' ||
        authorization == NULL || authorization[0] == '\0') {
        quec_ai_log(LOG_AI_ERR,"Missing QIOT_MQTT_PRODUCT_KEY, QIOT_MQTT_ACCESS_SECRET or QIOT_MQTT_AUTHORIZATION\n");
        curl_global_cleanup();
        return -1;
    }

    quec_ai_log(LOG_AI_INFO,"Getting Coze token...\n");
    coze_response_t *response = coze_get_connect_info(pk, dk, access_secret, authorization, roleId);
    
    if (response == NULL) {
        fprintf(stderr, "Failed to get token response\n");
        curl_global_cleanup();
        return -1;
    }
    
    if (response->code != 200) {
        fprintf(stderr, "Token request failed: code=%d, msg=%s\n", 
                response->code, 
                response->msg ? response->msg : "Unknown error");
        coze_response_free(response);
        curl_global_cleanup();
        return -1;
    }
	quec_ai_log(LOG_AI_INFO,"Token obtained successfully!\n");
    // quec_ai_log(LOG_AI_INFO,"Token data: %s\n", response->data ? response->data : "NULL");

    // 保存token到config.json文件
   if (response->data) {
        quec_ai_log(LOG_AI_INFO,"AccessToken received\n");
        quec_ai_log(LOG_AI_INFO,"BotId: %s\n", response->data->botId);
        quec_ai_log(LOG_AI_INFO,"VoiceId: %s\n", response->data->voiceId);
        quec_ai_log(LOG_AI_INFO,"ConversationId received\n");
        quec_ai_log(LOG_AI_INFO,"SpeechRate: %d\n", response->data->speechRate);
        quec_ai_log(LOG_AI_INFO,"RoleId: %d\n", response->data->roleId);
        
        // 检查并打印声纹状态
        if (response->data->voicePrint && strlen(response->data->voicePrint) > 0) {
            json_error_t error;
            json_t *voice_print_json = json_loads(response->data->voicePrint, 0, &error);
            if (voice_print_json) {
                json_t *identityStatus = json_object_get(voice_print_json, "identityStatus");
                json_t *identityConfig = json_object_get(voice_print_json, "identityConfig");
                json_t *wakeupStatus = json_object_get(voice_print_json, "wakeupStatus");
                
                if (identityStatus && json_is_boolean(identityStatus)) {
                    quec_ai_log(LOG_AI_INFO,"Voice Print Identity Status: %s\n", 
                           json_is_true(identityStatus) ? "ENABLED" : "DISABLED");
                } else {
                    quec_ai_log(LOG_AI_INFO,"Voice Print Identity Status: NOT SET\n");
                }
                
                if (identityConfig) {
                    if (json_is_null(identityConfig)) {
                        quec_ai_log(LOG_AI_INFO,"Voice Print Identity Config: NULL (not configured)\n");
                    } else if (json_is_object(identityConfig)) {
                        quec_ai_log(LOG_AI_INFO,"Voice Print Identity Config: CONFIGURED\n");
                        json_t *groupId = json_object_get(identityConfig, "groupId");
                        json_t *score = json_object_get(identityConfig, "score");
                        json_t *reuseVoiceInfo = json_object_get(identityConfig, "reuseVoiceInfo");
                        
                        if (groupId && json_is_string(groupId)) {
                            quec_ai_log(LOG_AI_INFO,"  - GroupId: %s\n", json_string_value(groupId));
                        }
                        if (score && json_is_integer(score)) {
                            quec_ai_log(LOG_AI_INFO,"  - Score: %lld\n", json_integer_value(score));
                        }
                        if (reuseVoiceInfo && json_is_boolean(reuseVoiceInfo)) {
                            quec_ai_log(LOG_AI_INFO,"  - ReuseVoiceInfo: %s\n", 
                                   json_is_true(reuseVoiceInfo) ? "true" : "false");
                        }
                    }
                } else {
                    quec_ai_log(LOG_AI_INFO,"Voice Print Identity Config: NOT FOUND\n");
                }
                
                if (wakeupStatus && json_is_boolean(wakeupStatus)) {
                    quec_ai_log(LOG_AI_INFO,"Voice Print Wakeup Status: %s\n", 
                           json_is_true(wakeupStatus) ? "ENABLED" : "DISABLED");
                }
                
                json_decref(voice_print_json);
            } else {
                quec_ai_log(LOG_AI_INFO,"Voice Print: Failed to parse JSON: %s\n", error.text);
            }
        } else {
            quec_ai_log(LOG_AI_INFO,"Voice Print: NOT PROVIDED\n");
        }
        
        // 保存到配置文件
        if (save_coze_info_to_config(response->data, "./../config/websocket_config.json") != 0) {
            fprintf(stderr, "Failed to save Coze info to config file\n");
        } else {
            quec_ai_log(LOG_AI_INFO,"Coze connection info successfully saved to config file\n");
        }
    }
    // 清理响应
    coze_response_free(response);
    curl_global_cleanup();
    return 0;
}
