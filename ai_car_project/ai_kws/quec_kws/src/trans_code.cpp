/** 
 * @file	trans_code.cpp
 * @brief	����ת����װʵ��,��Ҫ"gbk_unicdoe.h"��"gbk_unicode.cpp"�ı���⼰��������
 * 
 * @author	mhlu
 * @version	1.0
 * @date	2018/02/01
 * 
 * @see		
 * 
 * @par �汾��¼��
 * <table border=1>
 *  <tr> <th>�汾	<th>����			<th>����		<th>��ע </tr>
 *  <tr> <td>1.0	<td>2018/02/01	<td>mhlu	<td>���� </tr>
 * </table>
 */

#include "gbk_unicode.h"
#include "trans_code.h"

#include <string.h>

int gbk2ucs(const char* pgbk, int len, unsigned short* pucs, int wlen)
{
	return gbk2wchar(pgbk, len, pucs, wlen);
}

int ucs2gbk(unsigned short* pucs, int wlen, char* pgbk, int len)
{
	return wchar2gbk(pucs, wlen, pgbk, len);
}

int ucs2utf8(unsigned short* pucs, int wlen,  char* putf8, int len)
{
	(void)wlen;
	(void)len;
	return ucs2utf8s(pucs, putf8);
}

int utf8ucs2(const char *putf8, int len, unsigned short* pucs, int wlen)
{
	(void)len;
	return utf8ucs2s(putf8, pucs, wlen);
}

std::string gbk2utf8(const char* pgbk)
{
	int wlen = (int)strlen(pgbk) + 1;
	unsigned short* pucs = new unsigned short[wlen];
	gbk2ucs(pgbk, wlen -1, pucs, wlen);
	char *putf8 = new char[2*wlen+1];
	ucs2utf8s(pucs, putf8);
	std::string str = putf8;
	delete []putf8;
	delete []pucs;
	return str;
}

std::string utf8gbk(const char *putf8)
{
	int wlen = (int)strlen(putf8)+1;
	unsigned short* pucs = new unsigned short[wlen];
	utf8ucs2s(putf8, pucs, wlen);
	char* pgbk = new char[wlen];
	ucs2gbk(pucs, wlen, pgbk, wlen);
	std::string str = pgbk;
	delete []pgbk;
	delete []pucs;
	return str;
}