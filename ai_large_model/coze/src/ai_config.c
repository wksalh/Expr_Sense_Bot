#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "ai_log.h"
#include "ai_websocket_config.h"


quec_ai_config quec_ai_web_config;

// 确保配置文件存在，如果不存在则创建
int quec_ai_ensure_config_file(const char *config_path) {
    if (config_path == NULL) {
        quec_ai_log(LOG_AI_ERR, "Config path is NULL");
        return -1;
    }

    FILE *file = fopen(config_path, "r");
    if (file != NULL) {
        fclose(file);
        quec_ai_log(LOG_AI_ERR, "Config file already exists: %s", config_path);
        return 0;
    }

    quec_ai_log(LOG_AI_INFO, "Config file not found, creating default: %s", config_path);
    
    char dir_path[256];
    const char *last_slash = strrchr(config_path, '/');
    if (last_slash != NULL) {
        size_t dir_len = last_slash - config_path;
        if (dir_len >= sizeof(dir_path)) {
            quec_ai_log(LOG_AI_ERR, "Config directory path too long");
            return -1;
        }
        snprintf(dir_path, sizeof(dir_path), "%.*s", (int)dir_len, config_path);
        
        if (mkdir(dir_path, 0755) == 0) {
            quec_ai_log(LOG_AI_INFO, "Created config directory: %s", dir_path);
        } else if (errno != EEXIST) {
            quec_ai_log(LOG_AI_ERR, "Failed to create config directory: %s", dir_path);
            return -1;
        }
    }

    // 创建默认配置文件
    file = fopen(config_path, "w");
    if (file == NULL) {
        quec_ai_log(LOG_AI_ERR, "Failed to create config file: %s", config_path);
        return -1;
    }

    fprintf(file, "{\n");
    fprintf(file, "    \"coze_address\": \"ws.coze.cn\",\n");
    fprintf(file, "    \"coze_port\": 443,\n");
    fprintf(file, "    \"coze_path\": \"/v1/chat?bot_id=7553834038902112256\",\n");
    fprintf(file, "    \"coze_access_token\": \"\",\n");
    fprintf(file, "    \"coze_bot_id\": \"7553834038902112256\",\n");
    fprintf(file, "    \"coze_voice_id\": \"7426720361733046281\",\n");
    fprintf(file, "    \"coze_conversation_id\": \"\",\n");
    fprintf(file, "    \"coze_uid\": \"\",\n");
    fprintf(file, "    \"coze_speech_rate\": 0,\n");
    fprintf(file, "    \"coze_role_id\": 0,\n");
    fprintf(file, "    \"coze_parameters\": {},\n");
    fprintf(file, "    \"coze_voice_print\": {\n");
    fprintf(file, "        \"identity_status\": false,\n");
    fprintf(file, "        \"identity_config\": {\n");
    fprintf(file, "            \"groupId\": \"\",\n");
    fprintf(file, "            \"score\": 40,\n");
    fprintf(file, "            \"reuse_voice_info\": false\n");
    fprintf(file, "        }\n");
    fprintf(file, "    }\n");
    fprintf(file, "}\n");

    fclose(file);
    quec_ai_log(LOG_AI_INFO, "Created default config file: %s", config_path);
    return 0;
}

static char * quec_ai_read_config(const char *filename){

    if (filename == NULL) {
        return NULL;
    }

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        quec_ai_log(LOG_AI_ERR,"无法打开文件");
        quec_ai_log(LOG_AI_INFO,"file open error,%s\n",filename);
        return NULL;
    }

    struct stat buf;
    memset((void*)&buf,0x00,sizeof(buf));
    if (fstat(fd, &buf) != 0) {
        quec_ai_log(LOG_AI_ERR,"无法获取文件状态");
        close(fd);
        return NULL;
    }
    if (buf.st_size <= 0) {
        quec_ai_log(LOG_AI_ERR,"配置文件为空: %s", filename);
        close(fd);
        return NULL;
    }

    char *cjson_config =  malloc((size_t)buf.st_size+1);
    if ( cjson_config == NULL ){
        close(fd);
        return NULL;
    }
    memset(cjson_config,0x00,(size_t)buf.st_size+1);
    // 映射文件到内存
    char* buffer = mmap(NULL, (size_t)buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("内存映射失败");
        free(cjson_config);
        close(fd);
        return NULL;
    }
    memcpy(cjson_config, buffer, (size_t)buf.st_size);
    munmap(buffer, (size_t)buf.st_size);
    close(fd);
    return cjson_config;
}

static char * quec_ai_set_value(const char *data,int data_len){
    if ( data == NULL || data_len <=0 ){
        return NULL;
    }
    char *buf = malloc( data_len + 1);
    if ( buf == NULL ){
        return NULL;
    }
    memset(buf,0x00,data_len + 1);
    memcpy(buf,data,data_len);
    return buf;
}

int quec_ai_set_config(char *filename){
    if ( filename == NULL ){
        return -1;
    }
    char  *cjson_config = quec_ai_read_config(filename);
    if( cjson_config == NULL ){
        quec_ai_log(LOG_AI_ERR,"%d",quec_ai_web_config.port);
        return -1;
    }
    cJSON *root = cJSON_Parse(cjson_config);
    if ( root == NULL ){
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL){
            quec_ai_log(LOG_AI_ERR,"%s",error_ptr);
            
        }
        free(cjson_config);
        return -1;
    }

    cJSON *adress   =  cJSON_GetObjectItem(root, "coze_address");
    // cJSON *path     =  cJSON_GetObjectItem(root, "coze_path");
    cJSON *port     =  cJSON_GetObjectItem(root, "coze_port");

    // 读取Coze特定的配置字段
    cJSON *token = cJSON_GetObjectItem(root, "coze_access_token");
    cJSON *botId = cJSON_GetObjectItem(root, "coze_bot_id");
    cJSON *voiceId = cJSON_GetObjectItem(root, "coze_voice_id");
    cJSON *conversationId = cJSON_GetObjectItem(root, "coze_conversation_id");
    cJSON *uid = cJSON_GetObjectItem(root, "coze_uid");
    cJSON *speechRate = cJSON_GetObjectItem(root, "coze_speech_rate");
    cJSON *roleId = cJSON_GetObjectItem(root, "coze_role_id");
    cJSON *parameters = cJSON_GetObjectItem(root, "coze_parameters");  // 新增 parameters 读取
    cJSON *voice_processing_config = cJSON_GetObjectItem(root, "coze_voice_processing_config");  // 新增声纹处理配置读取
    cJSON *voice_print_config = cJSON_GetObjectItem(root, "coze_voice_print");  // 新增声纹识别配置读取

    // 检查必需字段
    if (!cJSON_IsString(token) || token->valuestring == NULL || !cJSON_IsString(botId) || botId->valuestring == NULL) {
        quec_ai_log(LOG_AI_ERR, "Missing or invalid required Coze fields: coze_access_token=%p, coze_bot_id=%p",
                   token, botId);
        
        cJSON *item = root->child;
        quec_ai_log(LOG_AI_INFO, "Available fields in websocket_config.json:");
        while (item) {
            if (cJSON_IsString(item)) {
                if (item->string && strstr(item->string, "token") != NULL) {
                    quec_ai_log(LOG_AI_INFO, "  %s: <redacted>", item->string);
                } else {
                    quec_ai_log(LOG_AI_INFO, "  %s: %s", item->string, item->valuestring);
                }
            } else if (cJSON_IsNumber(item)) {
                quec_ai_log(LOG_AI_INFO, "  %s: %d", item->string, item->valueint);
            } else {
                quec_ai_log(LOG_AI_INFO, "  %s: (other type)", item->string);
            }
            item = item->next;
        }
        
        cJSON_Delete(root);
        free(cjson_config);
        return -1;
    }
    // 设置Coze配置值
    quec_ai_web_config.botId = quec_ai_set_value(botId->valuestring, strlen(botId->valuestring));
    quec_ai_web_config.token = quec_ai_set_value(token->valuestring, strlen(token->valuestring));
    
    if (parameters) {
        char *parameters_str = cJSON_PrintUnformatted(parameters);
        if (parameters_str) {
            quec_ai_web_config.parameters = quec_ai_set_value(parameters_str, strlen(parameters_str));
            free(parameters_str);
            quec_ai_log(LOG_AI_INFO, "Loaded parameters from config");
        }
    }

    char dynamic_path[512];
    snprintf(dynamic_path, sizeof(dynamic_path), 
             "/v1/chat?bot_id=%s", botId->valuestring);

    quec_ai_web_config.path = quec_ai_set_value(dynamic_path, strlen(dynamic_path));
    
    // 设置Coze地址、端口（使用Coze专用字段或默认值）
    if (adress && cJSON_IsString(adress) && adress->valuestring) {
        quec_ai_web_config.adress = quec_ai_set_value(adress->valuestring, strlen(adress->valuestring));
    } else {
        quec_ai_web_config.adress = quec_ai_set_value("ws.coze.cn", strlen("ws.coze.cn")); // Coze API默认地址
    }
    
    if (port && cJSON_IsNumber(port)) {
        quec_ai_web_config.port = port->valueint;
    } else {
        quec_ai_web_config.port = 443; // HTTPS/WebSocket默认端口
    }

    // 设置其他Coze特定配置
    if (voiceId && cJSON_IsString(voiceId) && voiceId->valuestring) {
        quec_ai_web_config.voiceId = quec_ai_set_value(voiceId->valuestring, strlen(voiceId->valuestring));
    }
    
    if (conversationId && cJSON_IsString(conversationId) && conversationId->valuestring) {
        quec_ai_web_config.conversationId = quec_ai_set_value(conversationId->valuestring, strlen(conversationId->valuestring));
    }
    
    if (speechRate && cJSON_IsNumber(speechRate)) {
        quec_ai_web_config.speechRate = speechRate->valueint;
    } else {
        quec_ai_web_config.speechRate = 0; // 默认语速
    }
    
    if (roleId && cJSON_IsNumber(roleId)) {
        quec_ai_web_config.roleId = roleId->valueint;
    }
    
    if (uid && cJSON_IsString(uid) && uid->valuestring) {
        quec_ai_web_config.uid = quec_ai_set_value(uid->valuestring, strlen(uid->valuestring));
    }
    
    if (voice_processing_config && cJSON_IsObject(voice_processing_config)) {
        char *voice_processing_str = cJSON_PrintUnformatted(voice_processing_config);
        if (voice_processing_str) {
            quec_ai_web_config.voice_processing_config = quec_ai_set_value(voice_processing_str, strlen(voice_processing_str));
            quec_ai_log(LOG_AI_INFO, "Loaded voice_processing_config: %s", voice_processing_str);
            free(voice_processing_str);
            
            // 解析声纹处理配置的具体字段
            cJSON *enable_pdns = cJSON_GetObjectItem(voice_processing_config, "enable_pdns");
            if (enable_pdns && cJSON_IsBool(enable_pdns)) {
                quec_ai_log(LOG_AI_INFO, "Voice processing enable_pdns: %s", 
                        cJSON_IsTrue(enable_pdns) ? "true" : "false");
            } else {
                quec_ai_log(LOG_AI_INFO, "enable_pdns not found or not boolean in voice_processing_config");
            }
        } else {
            quec_ai_log(LOG_AI_INFO, "Failed to convert voice_processing_config to string");
        }
    } else {
        quec_ai_log(LOG_AI_INFO, "No coze_voice_processing_config found in config file, using default settings");
    }

    if (voice_print_config && cJSON_IsObject(voice_print_config)) {
        char *voice_print_str = cJSON_PrintUnformatted(voice_print_config);
        if (voice_print_str) {
            quec_ai_web_config.voice_print_config = quec_ai_set_value(voice_print_str, strlen(voice_print_str));
            quec_ai_log(LOG_AI_INFO, "Loaded voice_print_config: %s", voice_print_str);
            free(voice_print_str);
            
            cJSON *identityConfig = cJSON_GetObjectItem(voice_print_config, "identityConfig");
            if (identityConfig && cJSON_IsObject(identityConfig)) {
                cJSON *groupId = cJSON_GetObjectItem(identityConfig, "groupId");
                cJSON *score = cJSON_GetObjectItem(identityConfig, "score");
                cJSON *reuseVoiceInfo = cJSON_GetObjectItem(identityConfig, "reuseVoiceInfo");
                
                if (groupId && cJSON_IsString(groupId)) {
                    quec_ai_log(LOG_AI_INFO, "Voice print groupId: %s", groupId->valuestring);
                } else {
                    quec_ai_log(LOG_AI_INFO,"groupId not found or not string in identityConfig");
                }
                if (score && cJSON_IsNumber(score)) {
                    quec_ai_log(LOG_AI_INFO, "Voice print score: %d", score->valueint);
                } else {
                    quec_ai_log(LOG_AI_INFO, "score not found or not number in identityConfig");
                }
                if (reuseVoiceInfo && cJSON_IsBool(reuseVoiceInfo)) {
                    quec_ai_log(LOG_AI_INFO, "Voice print reuseVoiceInfo: %s", 
                            cJSON_IsTrue(reuseVoiceInfo) ? "true" : "false");
                } else {
                    quec_ai_log(LOG_AI_INFO, "reuseVoiceInfo not found or not boolean in identityConfig");
                }
            } else {
                quec_ai_log(LOG_AI_INFO, "identityConfig not found or not object in voice_print_config");
            }
        } else {
            quec_ai_log(LOG_AI_INFO, "Failed to convert voice_print_config to string");
        }
    }
    // 日志输出所有配置
    quec_ai_log(LOG_AI_INFO, "=== Coze WebSocket Configuration ===");
    quec_ai_log(LOG_AI_INFO, "botId          = %s", quec_ai_web_config.botId);
    quec_ai_log(LOG_AI_INFO, "token          = <redacted>");
    quec_ai_log(LOG_AI_INFO, "adress         = %s", quec_ai_web_config.adress);
    quec_ai_log(LOG_AI_INFO, "path            = %s", quec_ai_web_config.path);
    quec_ai_log(LOG_AI_INFO, "port            = %d", quec_ai_web_config.port);
    quec_ai_log(LOG_AI_INFO, "voiceId        = %s", quec_ai_web_config.voiceId ? quec_ai_web_config.voiceId : "NULL");
    quec_ai_log(LOG_AI_INFO, "conversationId = %s", quec_ai_web_config.conversationId ? quec_ai_web_config.conversationId : "NULL");
    quec_ai_log(LOG_AI_INFO, "speechRate     = %d", quec_ai_web_config.speechRate);
    quec_ai_log(LOG_AI_INFO, "roleId         = %d", quec_ai_web_config.roleId);
    quec_ai_log(LOG_AI_INFO, "uid             = %s", quec_ai_web_config.uid ? quec_ai_web_config.uid : "NULL");
    
    cJSON_Delete(root);
    free(cjson_config);

    quec_ai_log(LOG_AI_INFO, "Coze configuration loaded successfully");
    return 0;
}

quec_ai_config * quec_ai_get_config(void){
    return &quec_ai_web_config;
}
