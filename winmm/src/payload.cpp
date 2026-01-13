#include "pch.h"
#include "payload.h"

#include <cstdio>

#include <shobjidl_core.h>
#pragma comment(lib, "Shell32.lib")

#define TARGET_PROC_NAME "mintty.exe"

typedef FARPROC (WINAPI* fpGetProcAddress)(
	HMODULE hModule,
	LPCSTR lpProcName
	);

char* path_find_file_name(char* path)
{
	size_t pos = 0;

	for(size_t i = 0; path[i]; ++i){
		if(path[i] == '/' || path[i] == '\\'){
			pos = i;
		}
	}

	if(pos == 0){
		return path;
	}

	return path + pos + 1;
}

bool is_current_process(const char* target)
{
	char	path[MAX_PATH];
	DWORD	len;
	char*	filename;
	
	len	= GetModuleFileNameA(NULL, path, MAX_PATH);
	
	if(len == 0 || len >= MAX_PATH){
		return false;
	}

	filename	= path_find_file_name(path);

	return (_strcmpi(filename, target) == 0);
}

BYTE*	get_first_iat_entry(HMODULE hModule, const char* name)
{
	BYTE*						pBase		= reinterpret_cast<BYTE*>(hModule);
	IMAGE_DOS_HEADER*			pDos		= reinterpret_cast<IMAGE_DOS_HEADER*>(pBase);
	IMAGE_NT_HEADERS64*			pNT			= reinterpret_cast<IMAGE_NT_HEADERS64*>(pBase + pDos->e_lfanew);
	IMAGE_DATA_DIRECTORY*		pDir		= nullptr;
	IMAGE_IMPORT_DESCRIPTOR*	pDescriptor	= nullptr;

	if(pDos->e_magic != IMAGE_DOS_SIGNATURE){
		return nullptr;
	}

	if(pNT->Signature != IMAGE_NT_SIGNATURE){
		return nullptr;
	}

	pDir	= &pNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	if(pDir->VirtualAddress == 0 || pDir->Size == 0){
		return nullptr;
	}

	pDescriptor	= reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pBase + pDir->VirtualAddress);

	for(; pDescriptor->Name != 0; ++pDescriptor){
		IMAGE_THUNK_DATA64*		pINT;	// Import Name Table
		IMAGE_THUNK_DATA64*		pIAT;	// Import Address Table

		pINT	= reinterpret_cast<IMAGE_THUNK_DATA64*>(pBase + pDescriptor->OriginalFirstThunk);
		pIAT	= reinterpret_cast<IMAGE_THUNK_DATA64*>(pBase + pDescriptor->FirstThunk);

		if(pDescriptor->OriginalFirstThunk == 0){
			pINT	= reinterpret_cast<IMAGE_THUNK_DATA64*>(pBase + pDescriptor->FirstThunk);
		}

		for(; pINT->u1.AddressOfData != 0; pINT++, pIAT++){
			IMAGE_IMPORT_BY_NAME*	pName;

			if(pINT->u1.Ordinal & IMAGE_ORDINAL_FLAG64){
				continue;
			}
			
			pName	= reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + pINT->u1.AddressOfData);

			if(_strcmpi(pName->Name, name) == 0){
				return reinterpret_cast<BYTE*>(pIAT);
			}
		}
	}

	return nullptr;
}

static wchar_t INSTANCE_UUID[64] = {
	0
};

void generate_random_uuid()
{
	BYTE bytes[16];

	srand((unsigned int) time(0));

	for(size_t i = 0; i < sizeof(bytes); ++i){
		bytes[i] = rand();
	}

	bytes[6] = (bytes[6] & 0x0F) | 0x40;
	bytes[8] = (bytes[8] & 0x3F) | 0x80;

	swprintf_s(INSTANCE_UUID, 64, 
		L"bash.%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		bytes[0], bytes[1], bytes[2], bytes[3],
		bytes[4], bytes[5],
		bytes[6], bytes[7],
		bytes[8], bytes[9],
		bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
	);
}

HRESULT HK_SET_APP_ID(PCWSTR AppID)
{
	return SetCurrentProcessExplicitAppUserModelID(INSTANCE_UUID);
}

fpGetProcAddress	OG_GET_PROC_ADDRESS	= nullptr;
FARPROC				HK_GET_PROC_ADDRESS(HMODULE hModule, LPCSTR lpProcName)
{
	if(((uint64_t) lpProcName & 0xFFFF'FFFF'FFFF'0000) && _strcmpi(lpProcName, "SetCurrentProcessExplicitAppUserModelID") == 0){
		return (FARPROC) HK_SET_APP_ID;
	}

	return OG_GET_PROC_ADDRESS(hModule, lpProcName);
}

void		exec_payload()
{
	BYTE**	pIAT_GetProcAddress;

	if(!is_current_process(TARGET_PROC_NAME)){
		return;
	}
	
	pIAT_GetProcAddress	= (BYTE**) get_first_iat_entry(GetModuleHandleA(nullptr), "GetProcAddress");

	if(pIAT_GetProcAddress == nullptr){
		return;
	}

	if(OG_GET_PROC_ADDRESS == nullptr){
		generate_random_uuid();
		if(!GetModuleHandleA("shell32.dll")){
			LoadLibraryA("shell32.dll");
		}

		DWORD prot;
		VirtualProtect(pIAT_GetProcAddress, 0x8, PAGE_READWRITE, &prot);
		OG_GET_PROC_ADDRESS		= *reinterpret_cast<fpGetProcAddress*>(pIAT_GetProcAddress);
		*pIAT_GetProcAddress	= reinterpret_cast<BYTE*>(HK_GET_PROC_ADDRESS);
		VirtualProtect(pIAT_GetProcAddress, 0x8, prot, &prot);
	}

	SetCurrentProcessExplicitAppUserModelID(INSTANCE_UUID);
}





////////////////////////////////
//// Entrypoint Hijacking
////////////////////////////////

BYTE*		g_pMainEntrypoint	= nullptr;
BYTE		g_ogEntrypointByte	= 0xCC;
PVOID		g_hVEH				= nullptr;

LONG __stdcall	EntrypointExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo);

void			hijack_entrypoint()
{
	BYTE*				pBase;
	IMAGE_DOS_HEADER*	pDos;
	IMAGE_NT_HEADERS64*	pNT;
	DWORD				ulProt;

	g_hVEH				= AddVectoredExceptionHandler(1, EntrypointExceptionHandler);
	pBase				= reinterpret_cast<BYTE*>(GetModuleHandleA(nullptr));
	pDos				= reinterpret_cast<IMAGE_DOS_HEADER*>(pBase);
	pNT					= reinterpret_cast<IMAGE_NT_HEADERS64*>(pBase + pDos->e_lfanew);
	g_pMainEntrypoint	= pBase + pNT->OptionalHeader.AddressOfEntryPoint;
	g_ogEntrypointByte	= *g_pMainEntrypoint;

	VirtualProtect(g_pMainEntrypoint, 1, PAGE_EXECUTE_READWRITE, &ulProt);
	*g_pMainEntrypoint	= 0xCC;
	VirtualProtect(g_pMainEntrypoint, 1, ulProt, &ulProt);
}

// Logs work in this function, as it's executed after exec_payload().
void			restore_entrypoint()
{
	DWORD		ulProt;

	VirtualProtect(g_pMainEntrypoint, 1, PAGE_EXECUTE_READWRITE, &ulProt);
	*g_pMainEntrypoint	= g_ogEntrypointByte;
	VirtualProtect(g_pMainEntrypoint, 1, ulProt, &ulProt);

	if(g_hVEH){
		RemoveVectoredExceptionHandler(g_hVEH);
		g_hVEH	= 0;
	}
}

LONG __stdcall	EntrypointExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
	PCONTEXT				pWinContext;
	PEXCEPTION_RECORD		pWinRecord;

	pWinContext				= ExceptionInfo->ContextRecord;
	pWinRecord				= ExceptionInfo->ExceptionRecord;

	if(pWinRecord->ExceptionCode != EXCEPTION_BREAKPOINT){
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if(pWinContext->Rip != reinterpret_cast<DWORD64>(g_pMainEntrypoint)){
		return EXCEPTION_CONTINUE_SEARCH;
	}

	exec_payload();
	restore_entrypoint();
	return EXCEPTION_CONTINUE_EXECUTION;
}

BOOL __stdcall	DllMain_Payload
(
	HMODULE	hModule,
	DWORD	fdwReason,
	LPVOID	lpReserved
)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if(!is_current_process(TARGET_PROC_NAME)){
			return TRUE;
		}
		hijack_entrypoint();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}
