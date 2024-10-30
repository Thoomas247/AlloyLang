// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

using namespace System::Runtime::InteropServices;

#if defined _M_IX86
#pragma comment(linker, "/export:NativeEntryPoint_CallManaged=_NativeEntryPoint_CallManaged@4")
#endif
extern "C" ALLOYLANGCLR_API void __stdcall NativeEntryPoint_CallManaged(const wchar_t* msg)
{
}

#if defined _M_IX86 
#pragma comment(linker, "/export:NativeEntryPoint_CallNative=_NativeEntryPoint_CallNative@4")
#endif
extern "C" ALLOYLANGCLR_API void __stdcall NativeEntryPoint_CallNative(const wchar_t* msg)
{
}

#pragma unmanaged

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

