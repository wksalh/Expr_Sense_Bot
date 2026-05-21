/**
 * @file	sputils.i
 * @brief	sputils��ʵ��
 *
 *  ����ļ���������Щ����Ҫ����̫��ͷ�ļ��Ĳ���
 *  ���磺�ַ���������ͨ�ò���
 */
#ifndef __SPUTILS_I__
#define __SPUTILS_I__

//#include "stringutil.h"
#include <time.h>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <math.h>

#ifdef WIN32
# include <Windows.h>
# include <io.h>
#else  // Linux
# include <sys/time.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <dirent.h>
# include <unistd.h>
# include <string.h>
# include <stdlib.h>
# include <dlfcn.h>
# include <stdio.h>
# include <wchar.h>
#endif // OS

/*=============================================================================
 * �ַ�������
 *=============================================================================*/
inline int last_error(void)
{
#if defined(WIN32)
		return ::GetLastError();
#else	// nonwin
		return errno;
#endif	// nonwin
}

inline bool is_quanjiao(const char* pstr)
{
	if ( pstr == 0 )
		return false;
	if ( *pstr == 0 || *(pstr+1) == 0 )
		return false;
	return ((unsigned char)(*pstr) & 0x80) && ((unsigned char)(*pstr) != 0xFF);
}

inline int trim_str(std::string & str, const char * strim)
{
	// Get start and end position
	int start = 0, end = 0;
	const char* pstr = str.c_str();
	const char* p = pstr;
	while ( *p )
	{
		// ȫ��
		if ( is_quanjiao(p) )
		{
			if ( *p == 0xa1 && *(p+1) == 0xa1 )
			{
				if ( end == 0 )
					start += 2;
			}
			else
			{
				end = (int) (p - str.c_str() + 2);
			}
			p += 2;
			continue;
		}
		// ���
		if ( ( (unsigned char)*p < 0x20 || strchr(strim, *p) ) )
		{
			if ( end == 0 )
				start ++;
		}
		else
			end = (int) (p - str.c_str() + 1);
		p++;
	}

	// trim it
	if(end == 0)
		end = str.size();

	str = str.substr(start, end-start);
	return end - start;
}

inline int split_full_str(const std::string & str, std::vector<std::string> & subs_array, const char spliter[] /* = ",;:" */, int len /* = -1*/)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int strSize = str.length();
	if (-1 != len)
	{
		strSize = len;
	}
	while ( i < strSize )
	{
		if ( is_quanjiao(str.c_str()+i) )
		{
			i += 2;
			continue;
		}
		else
		{
			if( strchr(spliter, str[i] ) )
			{
				if ( j != i )
				{
					subs_array.push_back(str.substr(j, i - j ));
				}
				j = i + 1;
			}
			i++;
		}
	}
	if (j != i)
	{
		subs_array.push_back(str.substr(j, i - j ));
	}
	return 0;
}

inline bool is_file_exist(const char* file)
{
	if(NULL == file)
	{
		return false;
	}
#ifdef WIN32
	DWORD ret = ::GetFileAttributesA(file);
	if ( ret != 0xFFFFFFFF && ret != FILE_ATTRIBUTE_DIRECTORY )
		return true;
#else
	FILE * pTest = fopen(file,"r");
	if(pTest != NULL)
	{
		fclose(pTest);
		return true;
	}
#endif /*WIN32*/
	return false;
}

inline bool is_dir_exist(const char* dir)
{
#ifdef WIN32
	DWORD ret = ::GetFileAttributesA(dir);
	if ( ret != 0xFFFFFFFF && ( ret & FILE_ATTRIBUTE_DIRECTORY ) )
		return true;
#else
	struct stat fs ;
	memset(&fs, 0, sizeof(fs));
	if ( stat(dir, &fs) == 0 && ( fs.st_mode & S_IFDIR ) != 0 )
		return true;
#endif /*WIN32*/
	return false;
}

inline size_t get_file_size(const char* file)
{
	size_t size = 0;
#ifdef _WIN64
	struct __stat64 si;
#endif
	if ( file != 0 )
	{
#ifdef _WIN64
		int ret = ::_stat64(file, &si);
		if ( ret == 0 )
			size = si.st_size;
#else
		FILE * pTest = fopen(file,"r");
		if(pTest != NULL)
		{
			fseek(pTest, 0, SEEK_END);
			size = ftell(pTest);
			fclose(pTest);
		}
		else
		{	
			size = 0;
		}
#endif
		
	}
	return size;
}

inline
int read_bin_file (const char * file, void * data, size_t bytes, size_t * readed /* = 0 */)
{
	FILE * fp = fopen(file, "rb");
	if ( fp != 0 )
	{
		size_t rs = fread(data, 1, bytes, fp);
		if ( readed )
			*readed = rs;
		fclose(fp);
		return 0;
	}

	return last_error();
}

inline int write_bin_file(const char* file, const void * data, size_t bytes, size_t * written /* = 0 */, bool fail_if_exist /* = false */, bool append /* = false */)
{
	const char * mode = "wb";
	if ( fail_if_exist )
		mode = "r+b";
	if ( append )
		mode = "ab";

	FILE * fp = fopen(file, mode);
	if ( fp != 0 )
	{
		size_t ws = fwrite(data, 1, bytes, fp);
		if ( written )
			*written = ws;
		fclose(fp);
		return 0;
	}

	return last_error();
}

inline bool read_line(FILE* pFile, std::string &szLine)
{
	if (pFile == NULL || feof(pFile))
	{
		return false;
	}

	char buf[1024] = {0};
	if ( fgets(buf, 1024, pFile) )
	{
		szLine = buf;
		trim_str(szLine, " \n");
		return true;
	}

	return false;
}

#endif /* __SPUTILS_I__ */
