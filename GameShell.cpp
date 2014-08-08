// Game.cpp
//
// https://github.com/tylerneylon/gameshell-win
//

extern "C" {
#include "game.h"
}

// OpenGL header, via glew.
#define GLEW_STATIC
#include "glew/glew.h"

// Windows and standard headers.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include "Resource.h"

// strsafe must be included after tchar.h because required header
// ordering makes complete sense thank you microsoft.
#include <strsafe.h>

// Sadly, this must be included last because it defines a strdup
// macro that causes a compiler error if defined before the
// strdup function itself.
extern "C" {
#include "oswrap/oswrap.h"
}

#pragma warning(disable : 4244)  // Disable number conversion warnings.


// Constants for raw mouse input.
#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif


#define INIT_X_SIZE 640
#define INIT_Y_SIZE 480

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
HWND      hMainWnd;
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

HDC			hDC = NULL;    // GDI device context.
HGLRC		hRC = NULL;    // Permanent rendering context.

bool	keys[256];			// Array Used For The Keyboard Routine
bool	active = TRUE;		// Window Active Flag Set To TRUE By Default
bool  skip_next_mouse_delta = false;
POINT win_center;

int xViewSize = INIT_X_SIZE;
int yViewSize = INIT_Y_SIZE;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
HWND				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

// Returns the UTF-8 string corresponding to a key down event.
char *chars_from_keypress(UINT virtual_key, UINT scan_code) {

  // Get the up/down state of the keyboard.
  BYTE keyboard_state[256];
  GetKeyboardState(keyboard_state);

  wchar_t wchars[32];
  int num_wchars = ToUnicode(
    virtual_key,
    scan_code,
    keyboard_state,
    wchars,
    32,
    0);  // wFlags

  //dbg__printf("num_wchars=%d.\n", num_wchars);

  static char chars[32];
  int num_chars = WideCharToMultiByte(
    CP_UTF8,
    0,  // dwFlags
    wchars,
    num_wchars,
    chars,
    32,
    NULL,  // Default char to use for non-representable chars; doesn't apply to UTF-8.
    NULL   // Indicates if the above default char was used;    doesn't apply to UTF-8.
    );

  //dbg__printf("num_chars=%d.\n", num_chars);

  // WideCharToMultiByte may not null-terminate the output when the input
  // length is specified without a terminating null-char, as we are doing.
  chars[num_chars] = '\0';

  return chars;
}

void ResizeGLScene(GLsizei width, GLsizei height) {
  //OutputDebugString("ResizeGLScene\n");

  xViewSize = width;
  yViewSize = height;

  glViewport(0, 0, xViewSize, yViewSize);

  game__resize(xViewSize, yViewSize);
}

void take_mouse(HWND hWnd) {
  POINT win_origin = { 0, 0 };
  ClientToScreen(hWnd, &win_origin);

  RECT client_rect;
  GetClientRect(hWnd, &client_rect);
  POINT win_size = { client_rect.right, client_rect.bottom };

  win_center = { win_origin.x + win_size.x / 2, win_origin.y + win_size.y / 2 };
  SetCursorPos(win_center.x, win_center.y);

  RECT mouse_clip_rect = {
    win_origin.x,                  // left
    win_origin.y,                  // top
    win_origin.x + win_size.x,     // right
    win_origin.y + win_size.y };   // bottom
  ClipCursor(&mouse_clip_rect);

  ShowCursor(FALSE);
}

void keep_mouse() {
  take_mouse(hMainWnd);
}

void free_mouse() {
  ShowCursor(TRUE);
  ClipCursor(NULL);
}

void InitGL() {
  dbg__printf("InitGL\n");
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    OutputDebugString("Error from glewInit!\n");
  }
  game__init();
}

void DrawGLScene(GLvoid) {
  game__main_loop();
}

void KillGLWindow(HWND hWnd, HINSTANCE hInstance) {

  OutputDebugString("KillGLWindow\n");

  if (hRC)
  {
    if (!wglMakeCurrent(NULL, NULL))
    {
      OutputDebugString("Release Of DC And RC Failed.\n");
    }

    if (!wglDeleteContext(hRC))
    {
      OutputDebugString("Release Rendering Context Failed.\n");
    }
    hRC = NULL;
  }

  if (hDC && !ReleaseDC(hWnd, hDC))
  {
    OutputDebugString("Release Device Context Failed.\n");
    hDC = NULL;
  }

  if (hWnd && !DestroyWindow(hWnd))
  {
    OutputDebugString("Could Not Release hWnd.\n");
  }

  if (!UnregisterClass(szWindowClass, hInstance))
  {
    OutputDebugString("Could Not Unregister Class.\n");
    hInstance = NULL;
  }
}


int APIENTRY _tWinMain(_In_     HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_     LPTSTR    lpCmdLine,
  _In_     int       nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  OutputDebugString("_tWinMain\n");

  // Turn off stderr buffering for easier debugging.
  setvbuf(stderr, NULL, _IONBF, 0);

  MSG msg;

  // Initialize global strings
  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance, IDC_GAME, szWindowClass, MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  // Perform application initialization.
  HWND hWnd = InitInstance(hInstance, nCmdShow);
  if (hWnd == NULL) return FALSE;

  hMainWnd = hWnd;

  RAWINPUTDEVICE Rid[1];
  Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
  Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
  Rid[0].dwFlags = RIDEV_INPUTSINK;
  Rid[0].hwndTarget = hWnd;
  RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

  bool done = false;
  while (!done) {

    if (PeekMessage(&msg, NULL /* hWnd */, 0 /* wMsgFilterMin */, 0 /* wMsgFilterMax */, PM_REMOVE)) {
      done = (msg.message == WM_QUIT);
      if (!done) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
    else {
      DrawGLScene();
      SwapBuffers(hDC);
    }
  }

  KillGLWindow(hWnd, hInstance);

  return msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GAME));
  wcex.hCursor = NULL;  // LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = NULL;
  wcex.lpszMenuName = MAKEINTRESOURCE(IDC_GAME);
  wcex.lpszClassName = szWindowClass;
  wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

  return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow) {
  dbg__printf("InitInstance\n");
  HWND hWnd;

  hInst = hInstance;  // Store instance handle in our global variable

  RECT  windowRect = { 0         /* left */, 0         /* top */,
    xViewSize /* right */, yViewSize /* bottom */ };
  DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
  DWORD dwStyle = WS_OVERLAPPEDWINDOW;

  AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

  char dbgMsg[1024];
  LONG w = windowRect.right - windowRect.left;
  LONG h = windowRect.bottom - windowRect.top;
  StringCbPrintf(dbgMsg, 1024, "size=%dx%d\n", w, h);
  OutputDebugString(dbgMsg);

  hWnd = CreateWindowEx(dwExStyle,
    szWindowClass,
    "MyGame",  // window title
    dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
    0, 0,  // window position
    windowRect.right - windowRect.left,
    windowRect.bottom - windowRect.top,
    NULL,  // no parent window
    NULL,  // no menu
    hInstance,
    NULL   // parameter for WM_CREATE
    );

  if (!hWnd) {
    OutputDebugString("CreateWindowEx failed.\n");
    return NULL;
  }

  hDC = GetDC(hWnd);
  if (hDC == NULL) {
    OutputDebugString("GetDC failed.\n");
    return NULL;
  }

  static	PIXELFORMATDESCRIPTOR pfd =				// pfd Tells Windows How We Want Things To Be
  {
    sizeof(PIXELFORMATDESCRIPTOR),				// Size Of This Pixel Format Descriptor
    1,											// Version Number
    PFD_DRAW_TO_WINDOW |					    	// Format Must Support Window
    PFD_SUPPORT_OPENGL |                         // Format Must Support OpenGL
    PFD_DOUBLEBUFFER,							// Must Support Double Buffering
    PFD_TYPE_RGBA,								// Request An RGBA Format
    24,  										// Select Our Color Depth
    0, 0, 0, 0, 0, 0,							// Color Bits Ignored
    0,											// No Alpha Buffer
    0,											// Shift Bit Ignored
    0,											// No Accumulation Buffer
    0, 0, 0, 0,									// Accumulation Bits Ignored
    16,											// 16Bit Z-Buffer (Depth Buffer)
    0,											// No Stencil Buffer
    0,											// No Auxiliary Buffer
    PFD_MAIN_PLANE,								// Main Drawing Layer
    0,											// Reserved
    0, 0, 0										// Layer Masks Ignored
  };
  GLuint pixelFormat;
  pixelFormat = ChoosePixelFormat(hDC, &pfd);
  if (pixelFormat == 0) {
    OutputDebugString("ChoosePixelFormat failed.\n");
    return NULL;
  }

  if (!SetPixelFormat(hDC, pixelFormat, &pfd)) {
    OutputDebugString("SetPixelFormat failed.\n");
    return NULL;
  }

  hRC = wglCreateContext(hDC);
  if (hRC == NULL) {
    OutputDebugString("wglCreateContext failed.\n");
    return NULL;
  }

  if (!wglMakeCurrent(hDC, hRC)) {
    OutputDebugString("wglMakeCurrent failed.\n");
    return NULL;
  }

  ShowWindow(hWnd, nCmdShow);
  SetForegroundWindow(hWnd);
  SetFocus(hWnd);

  InitGL();

  // This call gives now-initialized rendering code access
  // to the current window resolution.
  ResizeGLScene(xViewSize, yViewSize);

  // No need to call UpdateWindow as we're constantly drawing.

  return hWnd;
}

void mouse_moved_to(int x, int y) {

  game__mouse_at(x, y);

  return;

  static int last_x = -1;
  static int last_y = -1;

  if (last_x >= 0 && !skip_next_mouse_delta) {
    //draw_cube_mouse_moved_by(x - last_x, y - last_y);
    SetCursorPos(win_center.x, win_center.y);
  }

  last_x = x;
  last_y = y;
  skip_next_mouse_delta = !skip_next_mouse_delta;

  //dbg__printf("moved to (%d, %d)\n", x, y);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

  int wmId, wmEvent;

  switch (message) {
  case WM_ACTIVATE:
    active = LOWORD(wParam) && !HIWORD(wParam);  // Active & not minimized.
    dbg__printf("active=%d\n", active);
    break;

  case WM_SYSCOMMAND:
    OutputDebugString("WM_SYSCOMMAND\n");
    switch (wParam) {
    case SC_SCREENSAVE:
    case SC_MONITORPOWER:
      return 0;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
    break;

  case WM_COMMAND:
    OutputDebugString("WM_COMMAND\n");
    wmId = LOWORD(wParam);
    wmEvent = HIWORD(wParam);
    // Parse the menu selections:
    switch (wmId) {
    case IDM_ABOUT:
      DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
      break;
    case IDM_EXIT:
      DestroyWindow(hWnd);
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
    break;

  case WM_CLOSE:
    OutputDebugString("WM_CLOSE\n");
  case WM_DESTROY:
    OutputDebugString("WM_DESTROY (or WM_CLOSE if it's right above this)\n");
    PostQuitMessage(0);
    break;

  case WM_KEYDOWN:
    keys[wParam] = true;
    {
      UINT scan_code = (lParam >> 16) & 0xFF;
      char *chars = chars_from_keypress(wParam, scan_code);
      game__key_down(io__convert_system_code(wParam), chars);
    }
    break;

  case WM_KEYUP:
    keys[wParam] = false;
    game__key_up(io__convert_system_code(wParam));
    break;

  case WM_MOUSEMOVE:
    // We send in coordinates where (0, 0) is the lower-left corner.
    mouse_moved_to(LOWORD(lParam), (yViewSize - 1) - HIWORD(lParam));
    break;

  case WM_LBUTTONDOWN:
    // We send in coordinates where (0, 0) is the lower-left corner.
    game__mouse_down(LOWORD(lParam), (yViewSize - 1) - HIWORD(lParam));
    break;

  case WM_INPUT:
  {
                 UINT dwSize = 40;
                 static BYTE lpb[40];

                 GetRawInputData((HRAWINPUT)lParam, RID_INPUT,
                   lpb, &dwSize, sizeof(RAWINPUTHEADER));

                 RAWINPUT* raw = (RAWINPUT*)lpb;

                 if (raw->header.dwType == RIM_TYPEMOUSE) {
                   int dx = raw->data.mouse.lLastX;
                   int dy = raw->data.mouse.lLastY;
                   game__mouse_moved(dx, dy);
                   return 0;
                 }
                 break;
  }

  case WM_SIZE:
    ResizeGLScene(LOWORD(lParam) /* width */, HIWORD(lParam) /* height */);
    break;

  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}
