
#include "os_api.h"

static bool initialized = false;
static WNDCLASS wndclass {0};
static s32 num_open_windows = 0;

Array<Input_Event> input_events;

LRESULT CALLBACK wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		// case WM_CREATE:
		// 	break;
		case WM_CLOSE:
			Input_Event ev;
            ev.type = Event_Type::QUIT;
            ev.window = hWnd;
            input_events.add(ev);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

#include <cstdio>
OS_Window os_create_window(s32 width, s32 height, const char *title) {
	if (!initialized) {
		initialized = true;
		wndclass.lpfnWndProc = wnd_proc;
		wndclass.hInstance = GetModuleHandle(nullptr);
		wndclass.hbrBackground = (HBRUSH) COLOR_BACKGROUND;
		wndclass.lpszClassName = "SimpWndClass";
		wndclass.style = CS_OWNDC;
		if (!RegisterClass(&wndclass)) {
			// printf("ERROR: count not register WNDCLASS\n"); // @TODO proper error system
			assert(0);
			return 0;
		}
	}

	HINSTANCE hInstance = GetModuleHandle(nullptr);
	num_open_windows++;
	return CreateWindowA(wndclass.lpszClassName, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, width, height, 0, 0, hInstance, 0);
}

OS_GL_Context os_create_gl_context(OS_Window win) {
	PIXELFORMATDESCRIPTOR pfd = {0};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	HDC hdc = GetDC(win);

	int cpf = ChoosePixelFormat(hdc, &pfd);
	SetPixelFormat(hdc, cpf, &pfd);

	return wglCreateContext(hdc);
}

void os_close_window(OS_Window win) {
	num_open_windows--;
	DestroyWindow(win);
}

void os_swap_buffers(OS_Window win) {
	SwapBuffers(GetDC(win));
}

s32  os_number_open_windows() {
	return num_open_windows;
}

void os_pump_input() {
	MSG msg = {0};
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void os_make_current(OS_Window win, OS_GL_Context ctx) {
	wglMakeCurrent(GetDC(win), ctx);
}

void os_get_window_dimensions(OS_Window win, s32 *width, s32 *height) {
	RECT rect;
	GetWindowRect(win, &rect);
	*width = (rect.right - rect.left);
	*height = (rect.bottom - rect.top);
}

bool os_get_mouse_position(OS_Window win, s32 *x, s32 *y) {
	RECT rect;
	GetWindowRect(win, &rect);
	POINT pt;
	GetCursorPos(&pt);
	*x = (pt.x - rect.left);
	*y = (pt.y - rect.top);
	return true;
}
