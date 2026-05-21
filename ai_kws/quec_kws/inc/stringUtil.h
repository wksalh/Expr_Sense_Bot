#ifndef __SPUTILS_H__
#define __SPUTILS_H__

#include <string>
#include <vector>
#include <list>
#include <map>
#include <ctype.h>

//get last error
int last_error(void);

//Full-Half Funtion
bool is_quanjiao(const char* pstr);

bool read_line(FILE* pFile, std::string &szLine);
//trim string
int trim_str(std::string& str, const char* strim = " ");
	
//split string
int split_full_str(const std::string& str, std::vector<std::string>& subs_array, const char spliter[] = ",;:", int len = -1);
int split_full_strutf8(const std::string & str, std::vector<std::string> & subs_array, const char spliter[]  = ",;:");

//File System Function
bool is_file_exist(const char* file);
bool is_dir_exist(const char* dir);

size_t get_file_size(const char* file);

int read_bin_file (const char* file, void* data, size_t bytes, size_t* readed = 0);
int write_bin_file(const char* file, const void* data, size_t bytes, size_t* written = 0, bool fail_if_exist = false, bool append = false);
// 字符串操作的实现放在sputils.i文件，不需要cpp文件
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable : 4996)
#include "stringutil.inl"
#	pragma warning(pop)
#else
#include "stringutil.inl"
#endif
#endif /* __SPUTILS_H__ */
