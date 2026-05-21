/** 
 * @file	cpconv.h
 * @brief	编码转换封装头文件,需要"gbk_unicdoe.h"和"gbk_unicode.cpp"的编码库及基本函数
 * 
 *      +---------+              +---------+            +---------+
 *     |   GBK    |  <========> | Unicode | <========> |  UTF-8   |
 *     +---------+              +---------+            +---------+
 * 
 * @author	mhlu
 * @version	1.0
 * @date	2018/02/01
 * 
 * @see		
 * 
 * @par 版本记录：
 * <table border=1>
 *  <tr> <th>版本	<th>日期			<th>作者		<th>备注 </tr>
 *  <tr> <td>1.0	<td>2018/02/01	<td>mhlu	<td>创建 </tr>
 * </table>
 */
 
#pragma once
 
#include <string>
 
/** 
* @name		gbk2ucs
* @brief	gbk->unicode
* @author	mhlu
* @date		2018/02/01
* @param	const char* pgbk		-[in]-gbk编码
* @param	ivInt32 len				-[in]-gbk编码长度
* @param	usigned short* pucs		-[out]-存储unicode编码指针
* @param	ivInt32 wlen			-[in]-存储unicode编码长度	
* @return	ivInt32-return 0-failed,return not 0-success
*/
int gbk2ucs(const char* pgbk, int len, unsigned short* pucs, int wlen);

/** 
* @name		ucs2gbk
* @brief	unicode->gbk
* @author	mhlu
* @date		2018/02/01
* @param	usigned short*pucs	-[in]-unicode编码指针
* @param	ivInt32 wlen			-[in]-存储unicode编码长度	
* @param	char*pgbk			-[out]-存储gbk编码指针
* @param	ivInt32 len				-[in]-gbk编码长度
* @return	ivInt32-return 0-failed,return not 0-success
*/
int ucs2gbk(unsigned short* pucs, int wlen, char* pgbk, int len);

/** 
* @name		ucs2utf8
* @brief	unicode->utf8
* @author	mhlu
* @date		2018/02/01
* @param	usigned short*pucs	-[in]-unicode编码指针
* @param	ivInt32 wlen			-[in]-存储unicode编码长度	
* @param	char*putf8			-[out]-存储utf8编码指针
* @param	ivInt32 len				-[in]-utf8编码长度
* @return	ivInt32-return 0-failed,return not 0-success
*/
int ucs2utf8(unsigned short* pucs, int wlen, char* putf8, int len);

/** 
* @name		utf8ucs2
* @brief	utf8->unicode
* @author	mhlu
* @date		2018/02/01
* @param	const char*putf8	-[in]-utf8编码
* @param	ivInt32 len				-[in]-utf8编码长度
* @param	usigned short*pucs	-[out]-存储unicode编码指针
* @param	ivInt32 wlen			-[in]-存储unicode编码长度	
* @return	ivInt32-return 0-failed,return not 0-success
*/
int utf8ucs2(const char* putf8, int len, unsigned short* pucs, int wlen);

/** 
* @name		gbk2utf8
* @brief	gkb->utf8
* @author	mhlu
* @date		2018/02/01
* @param	const char*pgbk	-[in]-gbk编码
* @return	std::string-转换后的utf8
*/
std::string gbk2utf8(const char* pgbk);

/** 
* @name		utf8gbk
* @brief	utf8->gbk
* @author	mhlu
* @date		2018/02/01
* @param	const char*putf8	-[in]-utf8编码
* @return	std::string-转换后的gbk
*/
std::string utf8gbk(const char* putf8);