#ifndef __IVPLATFORM_H__
#define __IVPLATFORM_H__
/*----------------------------------------------+
 |												|
 |	ivPlatform.h - InterSound 4 Platform Config |
 |												|
 |		Platform: ADS (ARM)					|
 |												|
 |		Copyright (c) 1999-2012, iFLYTEK Ltd.	|
 |		All rights reserved.					|
 |												|
 +----------------------------------------------*/

/*
 *	TODO: ���������Ŀ��ƽ̨������Ҫ�Ĺ���ͷ�ļ�
 */
// #include <stdio.h>
// #include <crtdbg.h>
/*
 *	TODO: ����Ŀ��ƽ̨�����޸����������ѡ��
 */

#define BIT64 1

#define IV_UNIT_BITS 8	/* �ڴ������Ԫλ�� */
#define IV_BIG_ENDIAN 0 /* �Ƿ��� Big-Endian �ֽ��� */
#if BIT64
#define IV_PTR_GRID 8 /* ���ָ�����ֵ */
#else
#define IV_PTR_GRID 4 /* ���ָ�����ֵ */
#endif
#define IV_PTR_PREFIX	   /* ָ�����ιؼ���(����ȡֵ�� near | far, ����Ϊ��) */
#define IV_CONST const	   /* �����ؼ���(����Ϊ��) */
#define IV_EXTERN extern   /* �ⲿ�ؼ��� */
#define IV_STATIC static   /* ��̬�����ؼ���(����Ϊ��) */
#define IV_INLINE __inline /* �����ؼ���(����ȡֵ�� inline, ����Ϊ��) */
#define IV_CALL_STANDARD   /* ��ͨ�������ιؼ���(����ȡֵ�� stdcall | fastcall | pascal, ����Ϊ��) */
#define IV_CALL_REENTRANT  /* �ݹ麯�����ιؼ���(����ȡֵ�� stdcall | reentrant, ����Ϊ��) */
#define IV_CALL_VAR_ARG	   /* ��κ������ιؼ���(����ȡֵ�� cdecl, ����Ϊ��) */

#define IV_TYPE_INT8 char	/* 8λ�������� */
#define IV_TYPE_INT16 short /* 16λ�������� */
#define IV_TYPE_INT24 int	/* 24λ�������� */
#define IV_TYPE_INT32 int	/* 32λ�������� */

#if BIT64
#define IV_TYPE_ADDRESS unsigned long long /* ��ַ�������� */
#define IV_TYPE_SIZE unsigned long long	   /* ��С�������� */
#else
#define IV_TYPE_ADDRESS unsigned int /* ��ַ�������� */
#define IV_TYPE_SIZE unsigned int	 /* ��С�������� */
#endif
#define IV_VOLATILE volatile

#define IV_ANSI_MEMORY 0 /* �Ƿ�ʹ�� ANSI �ڴ������ */
#define IV_ANSI_STRING 0 /* �Ƿ�ʹ�� ANSI �ַ��������� */

#if defined __GNUC__
#define IV_GNUC_COMPILER 1
#endif

#define IV_ARM_COMPILER 1

#if defined _MSC_VER
#define IV_WIN32_COMPILER 1
#endif

#define IV_ASSERT(exp) //_ASSERT(exp) /* ���Բ��� */
#define IV_YIELD	   /* ���в���(��Э��ʽ����ϵͳ��Ӧ����Ϊ�����л�����, ����Ϊ��) */

#if defined(DEBUG) || defined(_DEBUG)
#define IV_DEBUG 1 /* �Ƿ�֧�ֵ��� */
#else
#define IV_DEBUG 0 /* �Ƿ�֧�ֵ��� */
#endif
#if BIT64
// #define __aarch64__
#else
#define __arm__
#endif
// #define __AITALK_IOS__
// #undef __AITALK_IOS__
// #define __ARM_NEON__
// #undef __ARM_NEON__

#if defined __GNUC__ /*���gcc�������*/
// #pragma message("GCC compiler")
#define OS_LINUX 1
#define IV_GNUC_COMPILER 1
#if defined __i386__ /*��������ʹ��x86ָ�*/
#pragma message("GCC x86")
#define IV_GNUC_X86 1
#elif defined __x86_64__ /*��������ʹ��x86_64ָ�*/
#pragma message("GCC x86_64")
#define IV_GNUC_X86_64 1
#elif defined __aarch64__ /*��������ʹ��armָ�������armָ���Android*/
//#pragma message("GCC arm")
#define IV_GNUC_ARM 1
// #define __AITALK_IOS__
#undef __AITALK_IOS__
#if defined __ARM_NEON__
#pragma message("GCC arm neon")
#define IV_GNUC_ARM_NEON 1 /*�趨ʹ��arm neonָ���Ż�*/
#if defined __ANDROID__
#define __ANDROID_NEON__ 1
#endif
#endif
#elif defined __arm__ /*��������ʹ��armָ�������armָ���Android*/
//#pragma message("GCC arm")
#define IV_GNUC_ARM 1
// #define __AITALK_IOS__
#undef __AITALK_IOS__
#if defined __ARM_NEON__
#pragma message("GCC arm neon")
#define IV_GNUC_ARM_NEON 1 /*�趨ʹ��arm neonָ���Ż�*/
#if defined __ANDROID__
#define __ANDROID_NEON__ 1
#endif
#endif
#endif
#elif defined _MSC_VER /*���΢��������*/
#pragma message("Now ,using Microsoft Visual C++")
#define OS_LINUX 0
#if defined _WIN32_WCE
#pragma message("WinCE")
#if defined _M_ARM
#pragma message("WinCE ARM")
#define IV_WINCE_ARM 1
#if _M_ARM == 7
#pragma message("WinCE ARMv7")
#define IV_WINCE_ARM_NEON
#elif _M_ARM == 6
#pragma message "WinCE ARMv6"
#elif _M_ARM == 5
#pragma message("WinCE ARMv5")
#elif _M_ARM == 4
#pragma message("WinCE ARMv4")
#endif
#elif defined _M_MRX000
#pragma message("WinCE MIPS")
#elif defined _M_X86
#pragma message("WinCE x86")
#endif
#else
#pragma message("Windows Desktop")
#if defined _M_X64
#pragma message("Windows Desktop x64")
#elif defined _M_X86
#pragma message("Windows Desktop x86")
#endif
#endif
#else
#pragma message("Other need add more")
#define IV_OTHER_COMPILER
#endif

#if defined __ANDROID__ /*���Androidƽ̨��ע������Androidƽ̨�꣬����CPU���ͣ����ܰ���arm��mips��x86�ȶ���*/
#define IV_ANDROID 1
#endif

/* ����ƽ̨����ѡ������Ƿ��� Unicode ��ʽ���� */
#if defined(UNICODE) || defined(_UNICODE)
#define IV_UNICODE 1 /* �Ƿ��� Unicode ��ʽ���� */
#else
#define IV_UNICODE 0 /* �Ƿ��� Unicode ��ʽ���� */
#endif

#ifdef _MSC_VER
// ������Ϣ
#define STRING2(x) #x
#define STRING(x) STRING2(x)
#define To_msg(msg) __FILE__ "(" STRING(__LINE__) ") : "##msg
#define CompileMsg(msg) //__pragma(message(To_msg(msg)))
#else
#define CompileMsg(msg)
#endif

#endif /* __IVPLATFORM_H__ */
