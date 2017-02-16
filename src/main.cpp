#include "os_api.h"
#include <GL/gl.h>
#include "stb_image.h"

struct Image {
	// in pixels
	int width;
	int height;
	// we internally will use RGBA8 (32 bpp) color format

	char *data; // length is width*height*4, no stride
	GLuint texID;
};

static Image *load_image(const char *path) {
	int w, h, comp;
	char *data = (char *)stbi_load(path, &w, &h, &comp, 4);
	if (!data) return nullptr;
	Image *im = new Image();
	im->data = data;
	im->width = w;
	im->height = h;
	glGenTextures(1, &im->texID);
	glBindTexture(GL_TEXTURE_2D, im->texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return im;
}

struct Editor_Window {
	OS_Window os_window;
	OS_GL_Context os_gl_context;

	// starting x,y of the image editing area
	int image_x = 600;
	int image_y = 200;
	float image_scale = 16.0;
	Image *image = nullptr;
};

void remove_window_by_id(Array<Editor_Window *> &wins, OS_Window id) {
	int index;
	for (index = 0; index < wins.count; ++index) {
		Editor_Window *win = wins[index];
		if (win->os_window == id) break;
	}

	wins.remove(index);
}

Editor_Window *create_editor_window(Array<Editor_Window *> &wins) {
	Editor_Window *editor = new Editor_Window();
	editor->os_window = os_create_window(1280, 720, "Simp");
	editor->os_gl_context = os_create_gl_context(editor->os_window);
	wins.add(editor);
	return editor;
}

static void draw_quad(float x, float y, float width, float height, float scale) {
	float w = width * scale;
	float h = height * scale;
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(x, y);
	glTexCoord2f(1, 0);
	glVertex2f(x+w, y);
	glTexCoord2f(1, 1);
	glVertex2f(x+w, y+h);
	glTexCoord2f(0, 1);
	glVertex2f(x, y+h);
	glEnd();
}

static void draw_grid(float x, float y, int width, int height, float scale) {

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.0, 0, 0, 0.5);
	glBegin(GL_LINES);
	float h = height*scale;
	float w = width*scale;
	for (int i = 0; i <= width; ++i) {
		glVertex2f(x + (i*scale), y);
		glVertex2f(x + (i*scale), y + h);
	}

	for (int i = 0; i <= height; ++i) {
		glVertex2f(x, y + (i*scale));
		glVertex2f(x+w, y + (i*scale));
	}

	glEnd();

	glColor4f(1, 1, 1, 1);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

static void draw(Editor_Window *ed) {
	os_make_current(ed->os_window, ed->os_gl_context);
	s32 w, h;
	os_get_window_dimensions(ed->os_window, &w, &h);
	glViewport(0, 0, w, h);
	glScissor(0, 0, w, h);
	glClearColor(0.9, 0.9, 0.9, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (ed->image) {
		Image *im = ed->image;
		glBindTexture(GL_TEXTURE_2D, im->texID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im->width, im->height, GL_RGBA, GL_UNSIGNED_BYTE, im->data);
		draw_quad(ed->image_x, ed->image_y, im->width, im->height, ed->image_scale);
		if (ed->image_scale > 1.0) draw_grid(ed->image_x, ed->image_y, im->width, im->height, ed->image_scale);
	}

	os_swap_buffers(ed->os_window);
}

static Array<Editor_Window *> windows;

int main(int argc, char **argv) {

	if (argc < 2) {
		printf("Please specify an image file to open.\n");
		return -1;
	}
	
	Editor_Window *ed = create_editor_window(windows);
	os_make_current(ed->os_window, ed->os_gl_context);
	ed->image = load_image(argv[1]);
	if (!ed->image) {
		printf("Couldn't load image: %s\n", argv[1]);
		return -1;
	}

	while (true) {
		for (int i = 0; i < input_events.count; ++i) {
			Input_Event &ev = input_events[i];
			if (ev.type == Event_Type::QUIT) {
				os_close_window(ev.window);
				remove_window_by_id(windows, ev.window);
			}
		}

		if (os_number_open_windows() == 0) break;

		os_pump_input();
		for (int i = 0; i < windows.count; ++i) {
			Editor_Window *win = windows[i];
			draw(win);
		}
	}

	return 0;
}