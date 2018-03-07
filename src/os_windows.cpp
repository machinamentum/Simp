
#include "os_api.h"
#include <Windowsx.h>

static bool initialized = false;
static WNDCLASS wndclass {0};
static s32 num_open_windows = 0;
static s32 accum_wheel_delta = 0;

Array<Input_Event> input_events;

#include <cstdio>
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
		case WM_SYSKEYUP:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			if (lParam & (1 << 30) && ((lParam & (1 << 31)) == 0)) break; // skip repeats
			u32 vkcode = wParam;
        	Input_Event ev;
            ev.type = Event_Type::KEYBOARD;
            ev.down = (lParam & (1 << 31)) == 0;
            if (vkcode == VK_CONTROL) {
                ev.key = Key_Type::LCONTROL;
            } else if (vkcode == 'S') {
                ev.key = Key_Type::KEY_S;
            } else if (vkcode == 'T') {
                ev.key = Key_Type::KEY_T;
            } else {
                break;
            }
            ev.mod = (GetKeyState(VK_CONTROL) & (1 << 15)) ? Key_Type::LCONTROL : (Key_Type)-1;
            ev.window = hWnd;
            input_events.add(ev);
			break;
		}

		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP: {
			Input_Event ev;
            ev.type = Event_Type::MOUSE_BUTTON;
            ev.window = hWnd;
            ev.down = (wParam & MK_LBUTTON) != 0;
            ev.button = Button_Type::MOUSE_LEFT;
            ev.x = GET_X_LPARAM(lParam);
            ev.y = GET_Y_LPARAM(lParam);

           	input_events.add(ev);
			break;
		}
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP: {
			Input_Event ev;
            ev.type = Event_Type::MOUSE_BUTTON;
            ev.window = hWnd;
            ev.down = (wParam & MK_MBUTTON) != 0;
            ev.button = Button_Type::MOUSE_MIDDLE;
            ev.x = GET_X_LPARAM(lParam);
            ev.y = GET_Y_LPARAM(lParam);

           	input_events.add(ev);
			break;
		}
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP: {
			Input_Event ev;
            ev.type = Event_Type::MOUSE_BUTTON;
            ev.window = hWnd;
            ev.down = (wParam & MK_RBUTTON) != 0;
            ev.button = Button_Type::MOUSE_RIGHT;
            ev.x = GET_X_LPARAM(lParam);
            ev.y = GET_Y_LPARAM(lParam);

           	input_events.add(ev);
			break;
		}

		case WM_MOUSEWHEEL: {
			Input_Event ev;
            ev.type = Event_Type::MOUSE_BUTTON;
            ev.window = hWnd;
            ev.button = Button_Type::MOUSE_SCROLL;
            ev.x = GET_X_LPARAM(lParam);
            ev.y = GET_Y_LPARAM(lParam);

            auto zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (zDelta < 0) {
            	ev.down = true;
            } else {
            	ev.down = false;
            }

            accum_wheel_delta += zDelta;

            while (accum_wheel_delta < -WHEEL_DELTA || accum_wheel_delta > WHEEL_DELTA) {
            	accum_wheel_delta /= WHEEL_DELTA;
            	input_events.add(ev);
            }

			break;
		}

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

OS_Window os_create_window(s32 width, s32 height, const char *title) {
	if (!initialized) {
		initialized = true;
		wndclass.lpfnWndProc = wnd_proc;
		wndclass.hInstance = GetModuleHandle(nullptr);
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
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
	input_events.clear();

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
	GetClientRect(win, &rect);
	*width = (rect.right - rect.left);
	*height = (rect.bottom - rect.top);
}

bool os_get_mouse_position(OS_Window win, s32 *x, s32 *y) {
	RECT rect;
	GetClientRect(win, &rect);
	POINT pt;
	GetCursorPos(&pt);
    ScreenToClient(win, &pt);
	*x = (pt.x);
	*y = (pt.y);
	return true;
}


char *os_open_file_dialog(OS_Window win, bool is_for_save) {
    char path[MAX_PATH];
    ZeroMemory(&path, sizeof(path));

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = win;
    ofn.lpstrFilter = "PNG Files\0*.png\0Any File\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = nullptr;
    ofn.lpstrDefExt = "png";
    ofn.Flags = 0;
    if (is_for_save) {
        ofn.Flags = OFN_OVERWRITEPROMPT;
    }

    BOOL result;
    if (is_for_save) result = GetSaveFileNameA(&ofn);
    else result = GetOpenFileNameA(&ofn);

    if (!result) return nullptr;
    return copy_string(path);
}