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

union Color {
	u32 color = 0xFF00FF00;
	struct {
		u8 r;
		u8 g;
		u8 b;
		u8 a;
	};
};

struct Editor_Window {
	OS_Window os_window;
	OS_GL_Context os_gl_context;

	bool is_dirty = true;
	bool mouse_button_left = false;
	bool drag_image = false;
	int  drag_image_x, drag_image_y; // start position of the drag in pixels so we know how much to translate
	bool lcontrol = false;

	int tile_spacing = 16;

	Color color;

	// starting x,y of the image editing area
	int image_x = 400;
	int image_y = 200;
	float image_scale = 8.0;
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

Editor_Window *get_editor_for_window(Array<Editor_Window *> &wins, OS_Window id) {
	for (int index = 0; index < wins.count; ++index) {
		Editor_Window *win = wins[index];
		if (win->os_window == id) return win;
	}

	return nullptr;
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


static void draw_quad_lines(float x, float y, float width, float height, float scale) {
	float w = width * scale;
	float h = height * scale;
	glBegin(GL_LINES);
	glVertex2f(x, y);
	glVertex2f(x+w, y);

	glVertex2f(x, y);
	glVertex2f(x, y+h);


	glVertex2f(x+w, y);
	glVertex2f(x+w, y+h);

	glVertex2f(x, y+h);
	glVertex2f(x+w, y+h);
	
	glEnd();
}

static void draw_grid(float x, float y, int width, int height, float scale, int spacing) {
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_LINES);
	float h = height*scale;
	float w = width*scale;
	for (int i = 0; i <= (width / spacing); ++i) {
		glVertex2f(x + (i*spacing*scale), y);
		glVertex2f(x + (i*spacing*scale), y + h);
	}

	for (int i = 0; i <= (height / spacing); ++i) {
		glVertex2f(x, y + (i*spacing*scale));
		glVertex2f(x+w, y + (i*spacing*scale));
	}

	glEnd();
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

static void draw(Editor_Window *ed) {
	if (!ed->is_dirty) return;
	// os_make_current(ed->os_window, ed->os_gl_context);
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
		if (ed->image_scale > 2.0) {
			glColor4f(0.2, 0.2, 0.2, 0.5);
			glLineWidth(1.0);
			draw_grid(ed->image_x, ed->image_y, im->width, im->height, ed->image_scale, 1);
		}

		glColor4f(0.0, 0.0, 0.0, 1.0);
		// glLineWidth(1.5);
		draw_grid(ed->image_x, ed->image_y, im->width, im->height, ed->image_scale, ed->tile_spacing);
		glColor4f(1, 1, 1, 1);
	}

	s32 color_bar_y = h - 40;
	glDisable(GL_TEXTURE_2D);
	Color col = ed->color;
	glColor4ub(col.r, col.g, col.b, col.a);
	draw_quad(10, color_bar_y, 150, 30, 1.0);
	glColor4f(0, 0, 0, 1);
	glLineWidth(2.0);
	draw_quad_lines(10, color_bar_y, 150, 30, 1.0);
	glLineWidth(1.0);
	glColor4f(1, 1, 1, 1);

	glFinish();
	os_swap_buffers(ed->os_window);
}

bool get_pixel_pointed_at(Editor_Window *ed, s32 cursor_x, s32 cursor_y, s32 *px, s32 *py) {
	float scale = ed->image_scale;
	if (cursor_x < ed->image_x) return false;
	if (cursor_y < ed->image_y) return false;

	cursor_x = (cursor_x-ed->image_x);
	cursor_y = (cursor_y-ed->image_y);

	float inv_scale = 1.0 / scale;
	s32 ox = (s32)(cursor_x * inv_scale);
	s32 oy = (s32)(cursor_y * inv_scale);

	if (ox >= ed->image->width) return false;
	if (oy >= ed->image->height) return false;

	*px = ox;
	*py = oy;
	return true;
}

inline void write_color_to(char *data, Color color) {
	u32 *udata = (u32 *)data;
	*udata = color.color;
}

inline Color get_color_from(char *data) {
	u32 *udata = (u32 *)data;
	Color c;
	c.color = *udata;
	return c;
}

static void update(Editor_Window *ed) {
	ed->is_dirty = false;

	s32 cx, cy;
	if (os_get_mouse_position(ed->os_window, &cx, &cy)) {
		if (ed->drag_image) {
			if (cx != ed->drag_image_x) {
				ed->image_x += (cx - ed->drag_image_x);
				ed->drag_image_x = cx;
				ed->is_dirty = true;
			}
			if (cy != ed->drag_image_y) {
				ed->image_y += (cy - ed->drag_image_y);
				ed->drag_image_y = cy;
				ed->is_dirty = true;
			}
			return;
		}
		s32 px = 0, py = 0;
		bool success = get_pixel_pointed_at(ed, cx, cy, &px, &py);
		if (success && !ed->lcontrol && ed->mouse_button_left) {
			char *data = ed->image->data;
			data = data + ((px + py*ed->image->width) * 4);
			write_color_to(data, ed->color);
			ed->is_dirty = true;
		} else if (success && ed->lcontrol && ed->mouse_button_left) {
			char *data = ed->image->data;
			data = data + ((px + py*ed->image->width) * 4);
			ed->color = get_color_from(data);
			ed->is_dirty = true;
		}
	}
}

static void zoom_editor_one_tick(Editor_Window *ed, bool down) {
	if (down) {
		ed->image_scale *= 0.5;
	} else {
		ed->image_scale *= 2.0;
	}
	if (ed->image_scale < 0.5) ed->image_scale = 0.5;
	ed->is_dirty = true;
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
			} else if (ev.type == Event_Type::MOUSE_BUTTON) {
				Editor_Window *ed = get_editor_for_window(windows, ev.window);
				if (ed) {
					if (ev.button == Button_Type::MOUSE_LEFT) {
						ed->mouse_button_left = ev.down;
					} else if (ev.button == Button_Type::MOUSE_SCROLL) {
						zoom_editor_one_tick(ed, ev.down);
					} else if (ev.button == Button_Type::MOUSE_MIDDLE) {
						ed->drag_image = ev.down;
						ed->drag_image_x = ev.x;
						ed->drag_image_y = ev.y;
					}
				}
			} else if (ev.type == Event_Type::KEYBOARD) {
				Editor_Window *ed = get_editor_for_window(windows, ev.window);
				if (ed) {
					if (ev.key == Key_Type::LCONTROL) {
						ed->lcontrol = ev.down;
					}
				}
			}
		}

		if (os_number_open_windows() == 0) break;

		for (int i = 0; i < windows.count; ++i) {
			Editor_Window *win = windows[i];
			draw(win);
			update(win);
		}

		os_pump_input();
	}

	return 0;
}