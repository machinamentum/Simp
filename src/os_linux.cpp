// @Note most of this noise is glX and X11 stuff so it can be assumed that with little modification
// that this would work fine on OSX, maybe.
#include "os_api.h"
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
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

bool os_get_mouse_position(OS_Window win, s32 *x, s32 *y) {
	Window root;
	Window child;
	int rx, ry;
	u32 mask;
	Bool relative = XQueryPointer(global_display, win, &root, &child, &rx, &ry, x, y, &mask);
	return relative == True;
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
#include <cstdio>
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
		} else if (event.type == ButtonPress) {
        	Input_Event ev;
            ev.type = Event_Type::MOUSE_BUTTON;
            ev.window = event.xbutton.window;
            auto button = event.xbutton.button;
            ev.down = true;
            ev.x = event.xbutton.x;
            ev.y = event.xbutton.y;
            if (button == Button1) {
            	ev.button = Button_Type::MOUSE_LEFT;
            } else if (button == Button2) {
            	ev.button = Button_Type::MOUSE_MIDDLE;
            } else if (button == Button3) {
            	ev.button = Button_Type::MOUSE_RIGHT;
            } else if (button == Button4) {
            	ev.button = Button_Type::MOUSE_SCROLL;
            	ev.down = false;
            } else if (button == Button5) {
            	ev.button = Button_Type::MOUSE_SCROLL;
            	ev.down = true;
            } else {
            	continue;
            }

           	input_events.add(ev);
        } else if (event.type == ButtonRelease) {
        	Input_Event ev;
            ev.type = Event_Type::MOUSE_BUTTON;
            ev.window = event.xbutton.window;
            ev.x = event.xbutton.x;
            ev.y = event.xbutton.y;

            auto button = event.xbutton.button;
            if (button == Button1) {
            	ev.button = Button_Type::MOUSE_LEFT;
            } else if (button == Button2) {
            	ev.button = Button_Type::MOUSE_MIDDLE;
            } else if (button == Button3) {
            	ev.button = Button_Type::MOUSE_RIGHT;
            } else {
            	continue;
            }

           	ev.down = false;
           	input_events.add(ev);
        }  else if (event.type == KeyPress) {
        	// u32 shift = (event.xkey.state & ShiftMask) != 0;
        	auto keysym = XkbKeycodeToKeysym(global_display, event.xkey.keycode, 0, 1);
        	Input_Event ev;
            ev.type = Event_Type::KEYBOARD;
            ev.down = true;
            if (keysym == XK_Control_L) {
                ev.key = Key_Type::LCONTROL;
            } else if (keysym == XK_S) {
                ev.key = Key_Type::KEY_S;
            } else if (keysym == XK_T) {
                ev.key = Key_Type::KEY_T;
            } else {
                continue;
            }
            ev.mod = (event.xkey.state & ControlMask) ? Key_Type::LCONTROL : (Key_Type)-1;
            ev.window = event.xkey.window;
            input_events.add(ev);
        } else if (event.type == KeyRelease) {
        	if (XEventsQueued(global_display, QueuedAfterReading)) {
        		XEvent nev;
        		XPeekEvent(global_display, &nev);

        		if (nev.type == KeyPress && nev.xkey.time == event.xkey.time
        			&& nev.xkey.keycode == event.xkey.keycode) {
        			XNextEvent(global_display, &nev);
        			continue;
        		}
        	}

        	// u32 shift = (event.xkey.state & ShiftMask) != 0;
        	auto keysym = XkbKeycodeToKeysym(global_display, event.xkey.keycode, 0, 1);
        	Input_Event ev;
            ev.type = Event_Type::KEYBOARD;
            ev.down = false;
            if (keysym == XK_Control_L) {
            	ev.key = Key_Type::LCONTROL;
            } else if (keysym == XK_S) {
            	ev.key = Key_Type::KEY_S;
            } else if (keysym == XK_T) {
                ev.key = Key_Type::KEY_T;
            } else {
            	continue;
            }
            ev.window = event.xkey.window;
            input_events.add(ev);
        }
	}

	XUnlockDisplay(global_display);
}