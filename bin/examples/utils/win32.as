#pragma cimport("kernel32.dll", "CreateToolhelp32Snapshot", "uptr@ __CreateToolhelp32Snapshot(uint32, uint32)")
#pragma cimport("kernel32.dll", "Process32First", "int32 __Process32First(uptr@, uptr@)")
#pragma cimport("kernel32.dll", "Process32Next", "int32 __Process32Next(uptr@, uptr@)")
#pragma cimport("kernel32.dll", "GetCurrentProcessId", "uint32 __GetCurrentProcessId()")
#pragma cimport("kernel32.dll", "CloseHandle", "int32 __CloseHandle(uptr@)")
#define MAX_PATH 260
#define TH32CS_SNAPPROCESS 0x00000002
#define TH32CS_SNAPMODULE 0x00000008
#define offsetof_PROCESSENTRY32_dwSize 0
#define offsetof_PROCESSENTRY32_th32ProcessID 8
#define offsetof_PROCESSENTRY32_szExeFile 44
#ifdef SOF_CreateToolhelp32Snapshot
#define CreateToolhelp32Snapshot(flags, pid) __CreateToolhelp32Snapshot(flags, pid)
#else
#define CreateToolhelp32Snapshot(_, _) null
#endif
#ifdef SOF_Process32First
#define Process32First(snapshot, process) __Process32First(snapshot, process)
#else
#define Process32First(_, _) 0
#endif
#ifdef SOF_Process32Next
#define Process32Next(snapshot, process) __Process32Next(snapshot, process)
#else
#define Process32Next(_, _) 0
#endif
#ifdef SOF_CloseHandle
#define GetCurrentProcessId() __GetCurrentProcessId()
#else
#define GetCurrentProcessId() 0
#endif
#ifdef SOF_CloseHandle
#define CloseHandle(handle) __CloseHandle(handle)
#else
#define CloseHandle(_) 0
#endif
