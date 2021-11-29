module win32;


typedef HANDLE : void*;
typedef HINSTANCE : HANDLE;
typedef HMODULE : HINSTANCE;
typedef HWND : HANDLE;
typedef HICON : HANDLE;
typedef HCURSOR : HANDLE;
typedef HBRUSH : HANDLE;
typedef HMENU : HANDLE;

typedef DWORD : uint32;
typedef WORD : uint16;
typedef ATOM : WORD;
typedef WPARAM : ulong;
typedef LPARAM : long;
typedef LRESULT : long;

public struct LARGE_INTEGER
{
	int64 quadPart;
}


extern void OutputDebugStringA(char* str);
