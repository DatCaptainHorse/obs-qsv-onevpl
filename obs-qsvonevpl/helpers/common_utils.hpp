#pragma once
#include <stdio.h>

#include <vpl/mfxvideo++.h>


// =================================================================
// OS-specific definitions of types, macro, etc...
// The following should be defined:
//  - mfxTime
//  - MSDK_FOPEN
//  - MSDK_SLEEP
#if defined(_WIN32) || defined(_WIN64)
#include "../bits/windows_defs.h"
#elif defined(__linux__)
#include "../bits/linux_defs.h"
#endif

// =================================================================
// Helper macro definitions...
#define MSDK_PRINT_RET_MSG(ERR)                          \
	{                                                \
		PrintErrString(ERR, __FILE__, __LINE__); \
	}
#define MSDK_CHECK_RESULT(P, X, ERR)             \
	{                                        \
		if ((X) > (P)) {                 \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_CHECK_POINTER(P, ERR)               \
	{                                        \
		if (!(P)) {                      \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_CHECK_ERROR(P, X, ERR)              \
	{                                        \
		if ((X) == (P)) {                \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_IGNORE_MFX_STS(P, X)         \
	{                                 \
		if ((X) == (P)) {         \
			P = MFX_ERR_NONE; \
		}                         \
	}
#define MSDK_BREAK_ON_ERROR(P)           \
	{                                \
		if (MFX_ERR_NONE != (P)) \
			break;           \
	}
#define MSDK_SAFE_DELETE_ARRAY(P)   \
	{                           \
		if (P) {            \
			delete[] P; \
			P = NULL;   \
		}                   \
	}
#define MSDK_ALIGN32(X) (((mfxU32)((X) + 31)) & (~(mfxU32)31))
#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#define MSDK_SAFE_RELEASE(X)          \
	{                             \
		if (X) {              \
			X->Release(); \
			X = NULL;     \
		}                     \
	}
#define MSDK_MAX(A, B) (((A) > (B)) ? (A) : (B))

#define INIT_MFX_EXT_BUFFER(x, id)               \
	{                                        \
		(x).Header.BufferId = (id);      \
		(x).Header.BufferSz = sizeof(x); \
	}

#define INFINITE 0xFFFFFFFF // Infinite timeout

void PrintErrString(int err, const char *filestr, int line);



void Release();

void mfxGetTime(mfxTime *timestamp);

//void mfxInitTime();  might need this for Windows
double TimeDiffMsec(mfxTime tfinish, mfxTime tstart);
extern "C" void util_cpuid(int cpuinfo[4], int flags);

void check_adapters(struct adapter_info *adapters, size_t *adapter_count);

struct adapter_info {
	bool is_intel;
	bool is_dgpu;
	bool supports_av1;
	bool supports_hevc;
	bool supports_vp9;
};

#define MAX_ADAPTERS 10
extern struct adapter_info adapters[MAX_ADAPTERS];
extern size_t adapter_count;
