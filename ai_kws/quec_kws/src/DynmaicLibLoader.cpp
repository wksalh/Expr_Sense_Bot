#include "DynmaicLibLoader.h"
#if WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <string.h>
#include <string>
#include "ai_log.h"

DynmaicLibLoader::DynmaicLibLoader(void)
{
}

DynmaicLibLoader::~DynmaicLibLoader(void)
{
}

DLL_HANDLE DynmaicLibLoader::loadLibrary(const char *pLibFilePath)
{
	if (pLibFilePath == NULL || strlen(pLibFilePath) == 0)
	{
		quec_ai_log(LOG_AI_INFO,"lib path is null or empty, can not load\n");
		return NULL;
	}

	std::string fullLibFilePath = pLibFilePath;
#ifdef WIN32

#ifdef _DEBUG
	fullLibFilePath += "d.dll";
#else
	fullLibFilePath += ".dll";
#endif /* _DEBUG */

	return LoadLibraryA(fullLibFilePath.c_str());
#else
	fullLibFilePath = "lib" + fullLibFilePath + ".so";
	quec_ai_log(LOG_AI_INFO,"fullLibFilePath = %s\n", fullLibFilePath.c_str());
	dlerror();
	DLL_HANDLE handle = dlopen(fullLibFilePath.c_str(), RTLD_LAZY);
	char* err = dlerror();
	if(err)
	{
		quec_ai_log(LOG_AI_INFO,"load dynamic lib %s fail, err: %s\n", pLibFilePath, err);
		if (handle) {
			dlclose(handle);
		}
		return NULL;
	}
	
	return handle;
#endif
}

void* DynmaicLibLoader::getProcAddress(DLL_HANDLE hDll, const char *pProcName)
{
	if (hDll == NULL)
	{
		quec_ai_log(LOG_AI_INFO,"lib handle is null, can not get address\n");
		return NULL;
	}

	if (pProcName == NULL || strlen(pProcName) == 0)
	{
		quec_ai_log(LOG_AI_INFO,"pProcName is null or empty, can not get address\n");
		return NULL;
	}

#ifdef WIN32
	return GetProcAddress((HMODULE)hDll, pProcName);
#else
	dlerror();
	void* p = dlsym(hDll, pProcName);
	char *err = dlerror();
	if(err)
	{
		quec_ai_log(LOG_AI_INFO,"get process %s fail, err: %s\n", pProcName, err);
		return NULL;
	}
	return p;
#endif
}

void DynmaicLibLoader::freeLibrary(DLL_HANDLE hDll)
{
	if (hDll == NULL)
	{
		return;
	}

#ifdef WIN32
	FreeLibrary((HMODULE)hDll);
#else
	dlclose(hDll);
#endif
}
