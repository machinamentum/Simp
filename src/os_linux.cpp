// @Note most of this noise is glX and X11 stuff so it can be assumed that with little modification
// that this would work fine on OSX, maybe.
#include "os_api.h"
#include <X11/Xlib.h>
static Display *global_display = nullptr;
static Atom global_wm_delete_window = 0;
static XVisualInfo *vi;
static Colormap cmap;

static void init_display() {
	if (global_display) return;

	// XInitThreads();
	global_display = XOpenDisplay(nullptr);
	global_wm_delete_window = XInternAtom(global_display, "WM_DELETE_WINDOW", False);
	    s32 attr[] ={
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        None
    };
    vi = glXChooseVisual(global_display, 0, attr);
    Window root = DefaultRootWindow(global_display);
    cmap = XCreateColormap(global_display, root, vi->visual, AllocNone);
}

static s32 num_open_windows = 0;

OS_Window os_create_window(s32 width, s32 height, const char *title) {
	if (!global_display) init_display();
	XLockDisplay(global_display);
    Window root = DefaultRootWindow(global_display);

    XSetWindowAttributes swa = {0};
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;
    Window win = XCreateWindow(global_display, root, 0, 0, width, height, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);

    XMapWindow(global_display, win);
    XStoreName(global_display, win, title);

    XSetWMProtocols(global_display, win, &global_wm_delete_window, 1);

    num_open_windows++;
    XUnlockDisplay(global_display);
    return win;
}

OS_GL_Context os_create_gl_context(OS_Window win) {
	return glXCreateContext(global_display, vi, nullptr, GL_TRUE);
}

void os_get_window_dimensions(OS_Window win, s32 *w, s32 *h) {
	XWindowAttributes xwa;
	XGetWindowAttributes(global_display, win, &xwa);
	*w = xwa.width;
	*h = xwa.height;
}

void os_make_current(OS_Window win, OS_GL_Context ctx) {
	glXMakeCurrent(global_display, win, ctx);
}

void os_close_window(OS_Window win) {
	XLockDisplay(global_display);
	assert(num_open_windows); // this should never trigger, but just in case...
	num_open_windows--;
	glXMakeCurrent(global_display, None, nullptr);
	XDestroyWindow(global_display, win);
	XUnlockDisplay(global_display);
}

s32  os_number_open_windows() {
	return num_open_windows;
}

void os_swap_buffers(OS_Window win) {
	glXSwapBuffers(global_display, win);
}

Array<Input_Event> input_events;

void os_pump_input() {
	input_events.clear();

	XLockDisplay(global_display);
	while (XPending(global_display)) {
		XEvent event;
		XNextEvent(global_display, &event);

		if (event.type == ClientMessage) {
			if (global_wm_delete_window == (Atom) event.xclient.data.l[0]) {
        		Input_Event ev;
	            ev.type = Event_Type::QUIT;
	            ev.window = event.xclient.window;
	            input_events.add(ev);
        	}
		}
	}

	XUnlockDisplay(global_display);
}