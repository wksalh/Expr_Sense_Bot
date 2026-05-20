/**
 * @file        ai_common_base64.h
 * @brief       Base64编解码模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了Base64编解码的接口函数，
 *              基于OpenSSL库实现。提供了音频数据、文本数据等的
 *              Base64编码和解密功能，常用于网络传输和数据存储场景。
 */

#ifndef __AI_COMMON_BASE64_H__
#define __AI_COMMON_BASE64_H__

#include <stdlib.h>

/**
 * @brief   Base64编码函数
 * 
 * @details 该函数使用OpenSSL库对输入数据进行Base64编码，
 *          将二进制数据转换为可打印的ASCII字符串。
 * 
 * @param   dst         [out] 编码输出缓冲区
 * @param   dlen        [in]  输出缓冲区最大长度
 * @param   olen        [out] 实际编码输出长度
 * @param   src         [in]  待编码的输入数据
 * @param   slen        [in]  输入数据长度
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 编码成功
 *          - -1: 编码失败
 */
int base64_encode_openssl( unsigned char *dst, size_t dlen, size_t *olen,const unsigned char *src, size_t slen );

/**
 * @brief   Base64解码函数
 * 
 * @details 该函数使用OpenSSL库对Base64编码数据进行解码，
 *          将ASCII字符串还原为原始二进制数据。
 * 
 * @param   dst         [out] 解码输出缓冲区
 * @param   dlen        [in]  输出缓冲区最大长度
 * @param   olen        [out] 实际解码输出长度
 * @param   src         [in]  待解码的Base64字符串
 * @param   slen        [in]  输入字符串长度
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 解码成功
 *          - -1: 解码失败
 */
int base64_decode_openssl( unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen );

#endif
