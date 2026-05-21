#pragma once
#ifndef __GBK_UNICODE_H__
#define __GBK_UNICODE_H__

int __wcslen__(const unsigned short* wcstr);
int __wcscpy__(unsigned short* s1, const unsigned short* s2);
int __wcsncpy__(unsigned short* s1, const unsigned short* s2, int n);
int __wcscmp__(const unsigned short* s1, const unsigned short* s2);

int ucs2utf8(unsigned short ucs2, char* utf8);
int utf8ucs2(const char* utf8, unsigned short* ucs2);

int ucs2utf8s(const unsigned short* ucs2s, char* utf8s);
int utf8ucs2s(const char* utf8s, unsigned short* ucs2s, int len);

int wchar2gbk(unsigned short* wcstr, int len, char *str, int size);
int gbk2wchar(const char *str, int len, unsigned short* wcstr, int size);

#endif
