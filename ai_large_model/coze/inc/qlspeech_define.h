/**
 * *****************************************************************************
 * @file       : qlspeech_define.h
 * @brief      : 语音前端处理库类型定义头文件
 * @author     : lay.liu@quectel.com
 * @date       : 2024-05-06
 * @version    : v1.0
 * @copyright  : Copyright (c) 2024 Smart AI Robot
 * @license    : MIT License
 * @details    : 该头文件定义了语音前端处理库的所有基础数据类型、
 *              宏定义和类型别名。包括基本数据类型（8/16/24/32位整数）、
 *              平台相关的配置选项（字节序、内存对齐等）以及
 *              函数调用约定和资源管理相关的类型定义。
 * *****************************************************************************
 */

#ifndef QUECTEL_SPEECH_FRONTEND_DEFINE_H
#define QUECTEL_SPEECH_FRONTEND_DEFINE_H

#include "qlspeech_platform.h"

/*----------------------------------------------+
 *  Decorators
 +----------------------------------------------*/
#define qlConst		QL_CONST
#define qlStatic	QL_STATIC

#ifdef QL_INLINE
#define qlInline	QL_INLINE
#else
#define qlInline	
#endif

#define qlExtern	QL_EXTERN

#define qlPtr		QL_PTR_PREFIX*
#define qlCPtr		qlConst qlPtr
#define qlPtrC		qlPtr qlConst
#define qlCPtrC		qlConst qlPtr qlConst

#define qlCall		QL_CALL_STANDARD	        // unrecursive, not reentrant
#define qlReentrant	QL_CALL_REENTRANT	        // recursive, reentrant
#define qlVACall	QL_CALL_VAR_ARG		        // supporting variable argument

#define qlProc		qlCall qlPtr

/*----------------------------------------------+
 *  Definitions
 +----------------------------------------------*/

typedef int             qlComp;                 // for comparing
typedef	int             qlBool;                 // for boolean

#define	qlNull	        (0)
#define	qlTrue	        (~0)
#define	qlFalse	        (0)
#define qlGreater	    (1)
#define qlEqual		    (0)
#define qlLesser	    (-1)
#define qlIsGreater(v)	((v)>0)
#define qlIsEqual(v)	(0==(v))
#define qlIsLesser(v)	((v)<0)

/*----------------------------------------------+
 *  Datatypes
 +----------------------------------------------*/

/* values */
typedef	signed QL_TYPE_INT8		qlInt8;		    // 8-bit
typedef	unsigned QL_TYPE_INT8	qlUInt8;	    // 8-bit

typedef	signed QL_TYPE_INT16	qlInt16;	    // 16-bit
typedef	unsigned QL_TYPE_INT16	qlUInt16;	    // 16-bit

typedef	signed QL_TYPE_INT24	qlInt24;	    // 24-bit
typedef	unsigned QL_TYPE_INT24	qlUInt24;	    // 24-bit

typedef	signed QL_TYPE_INT32	qlInt32;	    // 32-bit
typedef	unsigned QL_TYPE_INT32	qlUInt32;	    // 32-bit

#ifdef QL_TYPE_INT48
typedef	signed QL_TYPE_INT48	qlInt48;	    // 48-bit
typedef	unsigned QL_TYPE_INT48	qlUInt48;	    // 48-bit
#endif

#ifdef QL_TYPE_INT64
typedef	signed QL_TYPE_INT64	qlInt64;	    // 64-bit
typedef	unsigned QL_TYPE_INT64	qlUInt64;	    // 64-bit
#endif

/* pointers of value */
typedef	qlInt8		      qlPtr qlPInt8;	    // 8-bit
typedef	qlUInt8		      qlPtr qlPUInt8;	    // 8-bit

typedef	qlInt16		      qlPtr qlPInt16;	    // 16-bit
typedef	qlUInt16	      qlPtr qlPUInt16;	    // 16-bit

typedef	qlInt24		      qlPtr qlPInt24;	    // 24-bit
typedef	qlUInt24	      qlPtr qlPUInt24;	    // 24-bit

typedef	qlInt32		      qlPtr qlPInt32;	    // 32-bit
typedef	qlUInt32	      qlPtr qlPUInt32;	    // 32-bit

#ifdef QL_TYPE_INT48
typedef	qlInt48		      qlPtr qlPInt48;       // 48-bit
typedef	qlUInt48	      qlPtr qlPUInt48;      // 48-bit
#endif

#ifdef QL_TYPE_INT64
typedef	qlInt64		      qlPtr qlPInt64;       // 64-bit
typedef	qlUInt64	      qlPtr qlPUInt64;      // 64-bit
#endif

/* constant pointers of value */
typedef	qlInt8		      qlCPtr qlPCInt8;	    // 8-bit
typedef	qlUInt8		      qlCPtr qlPCUInt8;	    // 8-bit

typedef	qlInt16		      qlCPtr qlPCInt16;	    // 16-bit
typedef	qlUInt16	      qlCPtr qlPCUInt16;    // 16-bit

typedef	qlInt24		      qlCPtr qlPCInt24;	    // 24-bit
typedef	qlUInt24	      qlCPtr qlPCUInt24;    // 24-bit

typedef	qlInt32		      qlCPtr qlPCInt32;     // 32-bit
typedef	qlUInt32	      qlCPtr qlPCUInt32;    // 32-bit

#ifdef QL_TYPE_INT48
typedef	qlInt48		      qlCPtr qlPCInt48;	    // 48-bit
typedef	qlUInt48	      qlCPtr qlPCUInt48;    // 48-bit
#endif

#ifdef QL_TYPE_INT64
typedef	qlInt64		      qlCPtr qlPCInt64;	    // 64-bit
typedef	qlUInt64	      qlCPtr qlPCUInt64;    // 64-bit
#endif

/*----------------------------------------------+
 *  Boundary Values
 +----------------------------------------------*/

#define QL_SBYTE_MAX    (+127)
#define QL_MAX_INT16    (+32767)
#define QL_INT_MAX      (+8388607L)
#define QL_MAX_INT32    (+2147483647L)

#define QL_SBYTE_MIN    (-QL_SBYTE_MAX - 1)
#define QL_MIN_INT16	(-QL_MAX_INT16 - 1)
#define QL_INT_MIN		(-QL_INT_MAX - 1)
#define QL_MIN_INT32	(-QL_MAX_INT32 - 1)

#define QL_BYTE_MAX		(0xffU)
#define QL_USHORT_MAX	(0xffffU)
#define QL_UINT_MAX		(0xffffffUL)
#define QL_ULONG_MAX	(0xffffffffUL)

/* memory cell */
typedef	qlUInt8		    qlByte;
typedef	qlPUInt8	    qlPByte;
typedef	qlPCUInt8	    qlPCByte;

typedef	signed			qlInt;
typedef	signed qlPtr	qlPInt;
typedef	signed qlCPtr	qlPCInt;

typedef	unsigned		qlUInt;
typedef	unsigned qlPtr	qlPUInt;
typedef	unsigned qlCPtr	qlPCUInt;

/* pointer of void */
typedef	void qlPtr	    qlPointer;
typedef	void qlCPtr	    qlCPointer;

/* adress and size */
typedef	QL_TYPE_ADDRESS	qlAddress;
typedef	QL_TYPE_SIZE	qlSize;

/*----------------------------------------------+
 *  Characters
 +----------------------------------------------*/

/* character types */
typedef qlInt8		    qlCharA;
typedef qlUInt16	    qlCharW;

#if QL_UNICODE
typedef qlCharW		    qlChar;
#else
typedef qlCharA		    qlChar;
#endif

/* string types */
typedef qlCharA qlPtr	qlStrA;
typedef qlCharA qlCPtr	qlCStrA;

typedef qlCharW qlPtr	qlStrW;
typedef qlCharW qlCPtr	qlCStrW;

typedef qlChar qlPtr	qlStr;
typedef qlChar qlCPtr	qlCStr;

/* constant text */
#define qlTextA(s)	    ((qlCStrA)s)
#define qlTextW(s)	    ((qlCStrW)L##s)

#if QL_UNICODE
#define qlText(s)	    qlTextW(s)
#else
#define qlText(s)	    qlTextA(s)
#endif

/*----------------------------------------------+
 *  Resource Adress, Size, and Bytes of Data
 +----------------------------------------------*/

typedef qlUInt32 qlResAddress;
typedef qlUInt32 qlResSize;

#define qlResSize_Int8		1
#define qlResSize_Int16		2
#define qlResSize_Int32		4

/* read resource callback type */
typedef void (qlProc qlCBReadRes)(
	qlPointer		pParameter,			        // [in] user callback parameter
	qlPointer		pBuffer,			        // [out] read resource buffer
	qlResAddress	iPos,				        // [in] read start position
	qlResSize		nSize );			        // [in] read size

/* map resource callback type */
typedef qlCPointer (qlProc qlCBMapRes)(
	qlPointer		pParameter,			        // [in] user callback parameter
	qlResAddress	iPos,				        // [in] map start position
	qlResSize		nSize );			        // [in] map size

/* resource pack description */
typedef struct tagResPackDesc qlTResPackDesc, qlPtr qlPResPackDesc;

struct tagResPackDesc {
	qlPointer		pCBParam;			        // resource callback parameter
	qlCBReadRes		pfnRead;			        // read resource callback entry
	qlCBMapRes		pfnMap;				        // map resource callback entry (optional)
	qlResSize		nSize;				        // resource pack size (optional, 0 for null)

	qlPUInt8		pCacheBlockIndex;	        // cache block index (optional, size = dwCacheBlockCount)
	qlPointer		pCacheBuffer;		        // cache buffer (optional, size = dwCacheBlockSize * dwCacheBlockCount)
	qlSize			nCacheBlockSize;	        // cache block size (optional, must be 2^N)
	qlSize			nCacheBlockCount;	        // cache block count (optional, must be 2^N)
	qlSize			nCacheBlockExt;		        // cache block ext (optional)
};

#endif // QUECTEL_SPEECH_FRONTEND_DEFINE_H
