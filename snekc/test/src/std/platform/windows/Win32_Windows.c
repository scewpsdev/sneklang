

const uint MB_OK = 0;
const uint MB_ICONINFORMATION = 0x00000040;

const uint CS_VREDRAW = 0x0001;
const uint CS_HREDRAW = 0x0002;
const uint CS_OWNDC = 0x0020;

const uint CW_USEDEFAULT = 0x80000000;

const uint WM_SIZE = 0x0005;
const uint WM_DESTROY = 0x0002;
const uint WM_CLOSE = 0x0010;
const uint WM_ACTIVATEAPP = 0x001C;

const uint WS_OVERLAPPED = 0x00000000;
const uint WS_CAPTION = 0x00C00000;
const uint WS_SYSMENU = 0x00080000;
const uint WS_THICKFRAME = 0x00040000;
const uint WS_MINIMIZEBOX = 0x00020000;
const uint WS_MAXIMIZEBOX = 0x00010000;
const uint WS_VISIBLE = 0x10000000;
const uint WS_OVERLAPPEDWINDOW = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);


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

struct LARGE_INTEGER
{
	int64 quadPart;
}

typedef WNDPROC : int64(HWND hwnd, uint uMsg, WPARAM wParam, LPARAM lParam);

struct POINT
{
	int x;
	int y;
}

struct MSG
{
	HWND hwnd;
	uint message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
	DWORD lPrivate;
}

struct WNDCLASS
{
	uint style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	char* lpszMenuName;
	char* lpszClassName;
}


HMODULE GetModuleHandleA(char* lpModuleName);

LRESULT DefWindowProcA(HWND window, uint message, WPARAM wParam, LPARAM lParam);

ATOM RegisterClassA(WNDCLASS* lpWndClass);

HWND CreateWindowExA(
  DWORD     dwExStyle,
  char*     lpClassName,
  char*     lpWindowName,
  DWORD     dwStyle,
  int       X,
  int       Y,
  int       nWidth,
  int       nHeight,
  HWND      hWndParent,
  HMENU     hMenu,
  HINSTANCE hInstance,
  void*     lpParam
);

int GetMessageA(
	MSG* lpMsg,
	HWND hWnd,
	uint wMsgFilterMin,
	uint wMsgFilterMax
);

int TranslateMessage(
	MSG* lpMsg
);

LRESULT DispatchMessageA(
	MSG* lpMsg
);

int MessageBoxA(HWND hWnd, char* lpText, char* lpCaption, uint uType);

void OutputDebugStringA(char* str);
