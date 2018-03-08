#include "os_api.h"
#include <GL/gl.h>
#include "stb_image.h"
#include "stb_image_write.h"

struct Image {
	// in pixels
	int width;
	int height;
	// we internally will use RGBA8 (32 bpp) color format

	char *data; // length is width*height*4, no stride
	GLuint texID;

	char *path;
};

static Image *load_image(const char *path) {
	int w, h, comp;
	char *data = (char *)stbi_load(path, &w, &h, &comp, 4);
	if (!data) return nullptr;
	Image *im = new Image();
	im->data = data;
	im->width = w;
	im->height = h;
	im->path = (char *)path; // maybe copy here ?

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

struct Region_Selection {
	// in image pixel coords
	int x0, y0;
	int x1, y1;
};

const int END_MOVE_SELECTED_PIXELS = 4;
const int BEGIN_MOVE_SELECTED_PIXELS = 3;
const int BEGIN_SELECTION = 2;
const int END_SELECTION = 1;
const int NOT_SELECTING = 0;

struct Palette {
	const static int num_colors = 32;
	const static int tile_size = 16;
	Color colors[num_colors];

	// position
	int x = 0;
	int y = 0;

	int line_break = 8;
};

struct Editor_Window {
	OS_Window os_window;
	OS_GL_Context os_gl_context;
 
	bool is_dirty = true;
	bool mouse_button_left = false;
	bool drag_image = false;
	int  drag_image_x, drag_image_y; // start position of the drag in pixels so we know how much to translate
	bool lcontrol = false;

	int select_mode = 0; // 4: end move selected pixels; 3: moving selected pixels; 2: starting selection; 1: ending selection; 0: not selecting
	Region_Selection selection;

	int move_pixels_start_x = 0;
	int move_pixels_start_y = 0;
	int move_offset_x = 0;
	int move_offset_y = 0;
	Image *move_data = nullptr;

	int tile_spacing = 16;

	// UI toggles
	bool show_mini_map = true;

	Color color;
	Palette palette;

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

static Color make_color(u8 r, u8 g, u8 b, u8 a) {
	Color c;
	c.r = r;
	c.g = g;
	c.b = b;
	c.a = a;
	return c;
}

static void init_default_palette(Palette *p) {
	// DB32 palette
	p->colors[0] = make_color(0, 0, 0, 255);
	p->colors[1] = make_color(34, 32, 52, 255);
	p->colors[2] = make_color(69, 40, 60, 255);
	p->colors[3] = make_color(102, 57, 49, 255);
	p->colors[4] = make_color(143, 86, 59, 255);
	p->colors[5] = make_color(223, 113, 38, 255);
	p->colors[6] = make_color(217, 160, 102, 255);
	p->colors[7] = make_color(238, 195, 154, 255);
	p->colors[8] = make_color(251, 242, 54, 255);
	p->colors[9] = make_color(153, 229, 80, 255);
	p->colors[10] = make_color(106, 190, 48, 255);
	p->colors[11] = make_color(55, 148, 110, 255);
	p->colors[12] = make_color(75, 105, 47, 255);
	p->colors[13] = make_color(82, 75, 36, 255);
	p->colors[14] = make_color(50, 60, 57, 255);
	p->colors[15] = make_color(63, 63, 116, 255);
	p->colors[16] = make_color(48, 96, 130, 255);
	p->colors[17] = make_color(91, 110, 225, 255);
	p->colors[18] = make_color(99, 155, 255, 255);
	p->colors[19] = make_color(95, 205, 228, 255);
	p->colors[20] = make_color(203, 219, 252, 255);
	p->colors[21] = make_color(255, 255, 255, 255);
	p->colors[22] = make_color(155, 173, 183, 255);
	p->colors[23] = make_color(132, 126, 135, 255);
	p->colors[24] = make_color(105, 106, 106, 255);
	p->colors[25] = make_color(89, 86, 82, 255);
	p->colors[26] = make_color(118, 66, 138, 255);
	p->colors[27] = make_color(172, 50, 50, 255);
	p->colors[28] = make_color(217, 87, 99, 255);
	p->colors[29] = make_color(215, 123, 186, 255);
	p->colors[30] = make_color(143, 151, 74, 255);
	p->colors[31] = make_color(138, 111, 48, 255);
}

Editor_Window *create_editor_window(Array<Editor_Window *> &wins) {
	Editor_Window *editor = new Editor_Window();
	editor->os_window = os_create_window(1280, 720, "Simp");
	editor->os_gl_context = os_create_gl_context(editor->os_window);
	init_default_palette(&editor->palette);
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
	// if (!ed->is_dirty) return;
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

	Image *im = ed->image;

	if (ed->image) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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
		glDisable(GL_BLEND);
	}

	glDisable(GL_TEXTURE_2D);
	Region_Selection &sel = ed->selection;
	glColor4f(1.0, 0.0, 0.0, 0.5);
	glLineWidth(3.0);
	{
		int w = sel.x1-sel.x0;
		int h = sel.y1-sel.y0;
		draw_quad_lines(ed->image_x + sel.x0*ed->image_scale, ed->image_y + sel.y0*ed->image_scale, w, h, ed->image_scale);
	}

	s32 color_bar_y = h - 40;
	Color col = ed->color;
	glColor4ub(col.r, col.g, col.b, col.a);
	draw_quad(10, color_bar_y, 40, 30, 1.0);
	glColor4f(0, 0, 0, 1);
	glLineWidth(2.0);
	draw_quad_lines(10, color_bar_y, 40, 30, 1.0);
	glColor4f(1, 1, 1, 1);

	if (im && ed->show_mini_map) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, im->texID);
		float aspect = (float) im->width / (float) im->height;
		const float default_minimap_height = 200.0f;
		float mmwidth = default_minimap_height * aspect;
		draw_quad((w - mmwidth) - 10, 10, mmwidth, default_minimap_height, 1);

		glColor4f(0, 0, 0, 1);
		glDisable(GL_TEXTURE_2D);
		draw_quad_lines((w - mmwidth) - 10, 10, mmwidth, default_minimap_height, 1);
		glColor4f(1, 1, 1, 1);
	}

	{
		Palette *p = &ed->palette;
		int tile_size = p->tile_size;
		for (int i = 0; i < p->num_colors; i++) {
			int y = p->y + ((i / p->line_break) * tile_size);
			int x = p->x + ((i % p->line_break) * tile_size);

			Color c = p->colors[i];
			glColor4ub(c.r, c.g, c.b, c.a);
			draw_quad(x, y, tile_size, tile_size, 1);
		}

		glColor4f(1, 1, 1, 1);
	}

	glLineWidth(1.0);

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

	if (ox > ed->image->width) return false;
	if (oy > ed->image->height) return false;

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

inline bool point_in_region(Region_Selection *sel, int px, int py) {
	int x0 = sel->x0;
	int y0 = sel->y0;
	int x1 = sel->x1;
	int y1 = sel->y1;

	return !((px < x0 || px >= x1) || (py < y0 || py >= y1));
}

inline bool is_in_space_occupied_by_palette(int px, int py, Palette *p) {
	int x = p->x;
	int y = p->y;

	int w = p->line_break * p->tile_size;
	int h = (p->num_colors / p->line_break) * p->tile_size;
	if (p->num_colors % p->line_break) h += p->tile_size;
	// printf("%d:%d\n", w, h);

	return px >= x && px < x+w && py >= y && py < y+h;
}

static Image *generate_default_image(int width = 128, int height = 128, char *storage = nullptr) {
	Image *im = new Image();
	im->width = width;
	im->height = height;
	if (storage) {
		im->data = storage;
	} else {
		im->data = static_cast<char *>(alloc(im->width * im->height * 4));
		memset(im->data, 0xFFFFFFFF, im->width * im->height * 4);
	}
	im->path = nullptr;

	glGenTextures(1, &im->texID);
	glBindTexture(GL_TEXTURE_2D, im->texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im->width, im->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, im->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return im;
}

static void update(Editor_Window *ed) {
	ed->is_dirty = false;

	s32 cx, cy;
	if (os_get_mouse_position(ed->os_window, &cx, &cy)) {
		s32 px = 0, py = 0;
		if (ed->select_mode == BEGIN_SELECTION && ed->mouse_button_left) {
			if (cx < ed->image_x) cx = ed->image_x;
			if (cx >= (ed->image_x+(ed->image->width*ed->image_scale))) cx = ed->image_x+((ed->image->width)*ed->image_scale);
			if (cy < ed->image_y) cy = ed->image_y;
			if (cy >= (ed->image_y+(ed->image->height*ed->image_scale))) cy = ed->image_y+((ed->image->height)*ed->image_scale);
			bool success = get_pixel_pointed_at(ed, cx, cy, &px, &py);
			// printf("p: %d:%d, %d:%d\n", cx, cy, ed->image_x, ed->image_y);
			assert(success);

			ed->selection.x0 = px;
			ed->selection.y0 = py;
			ed->selection.x1 = px;
			ed->selection.y1 = py;

			ed->select_mode = END_SELECTION;

			ed->is_dirty = true;
			return;
		}
		if (ed->select_mode == END_SELECTION) {
			if (cx < ed->image_x) cx = ed->image_x;
			if (cx >= (ed->image_x+(ed->image->width*ed->image_scale))) cx = ed->image_x+((ed->image->width)*ed->image_scale);
			if (cy < ed->image_y) cy = ed->image_y;
			if (cy >= (ed->image_y+(ed->image->height*ed->image_scale))) cy = ed->image_y+((ed->image->height)*ed->image_scale);
			bool success = get_pixel_pointed_at(ed, cx, cy, &px, &py);
			assert(success);
			ed->selection.x1 = px;
			ed->selection.y1 = py;

			if (!ed->mouse_button_left) {
				ed->select_mode = NOT_SELECTING;
				if (ed->selection.x1 <= ed->selection.x0) {
					int i = ed->selection.x1;
					ed->selection.x1 = ed->selection.x0; 
					ed->selection.x0 = i;
				}
				if (ed->selection.y1 <= ed->selection.y0) {
					int i = ed->selection.y1;
					ed->selection.y1 = ed->selection.y0; 
					ed->selection.y0 = i;
				}
			}

			ed->is_dirty = true;
			return;
		}
		if (ed->select_mode == BEGIN_MOVE_SELECTED_PIXELS && ed->mouse_button_left) {
			if ((ed->selection.x1 - ed->selection.x0 == 0) || (ed->selection.y1 - ed->selection.y0 == 0)) {
				ed->select_mode = NOT_SELECTING;
				return;
			}
			bool success = get_pixel_pointed_at(ed, cx, cy, &px, &py);
			if (!point_in_region(&ed->selection, px, py)) return;

			ed->move_pixels_start_x = px;
			ed->move_pixels_start_y = py;
			ed->move_offset_x = 0;
			ed->move_offset_y = 0;

			Region_Selection *sel = &ed->selection;
			int w = sel->x1 - sel->x0;
			int h = sel->y1 - sel->y0;
			char *move_data = static_cast<char *>(alloc(4 * w * h));
			memset(move_data, 0x80, 4*w*h);
			for (int y = sel->y0; y < sel->y1; ++y) {
				for (int x = sel->x0; x < sel->x1; x++) {
					s32 *out = reinterpret_cast<s32 *>(move_data);
					s32 *in = reinterpret_cast<s32 *>(ed->image->data);

					out[(x - sel->x0) + (y - sel->y0) * w] = in[x + y * ed->image->width];
				}
			}

			ed->move_data = generate_default_image(w, h, move_data);

			ed->select_mode = END_MOVE_SELECTED_PIXELS;
			return;
		}
		if (ed->select_mode == END_MOVE_SELECTED_PIXELS) {
			if (cx < ed->image_x) cx = ed->image_x;
			if (cx >= (ed->image_x+(ed->image->width*ed->image_scale))) cx = ed->image_x+((ed->image->width)*ed->image_scale);
			if (cy < ed->image_y) cy = ed->image_y;
			if (cy >= (ed->image_y+(ed->image->height*ed->image_scale))) cy = ed->image_y+((ed->image->height)*ed->image_scale);
			bool success = get_pixel_pointed_at(ed, cx, cy, &px, &py);
			assert(success);

			Region_Selection *sel = &ed->selection;

			for (int y = sel->y0; y < sel->y1; ++y) {
				for (int x = sel->x0; x < sel->x1; x++) {
					s32 *in = reinterpret_cast<s32 *>(ed->image->data);
					in[x + y * ed->image->width] = 0x00000000; // set current region to transparency
				}
			}

			int pixels_x = px - ed->move_pixels_start_x;
			int pixels_y = py - ed->move_pixels_start_y;

			sel->x0 += pixels_x;
			sel->x1 += pixels_x;
			sel->y0 += pixels_y;
			sel->y1 += pixels_y;

			if (sel->x0 < 0) ed->move_offset_x += -sel->x0;
			if (sel->y0 < 0) ed->move_offset_y += -sel->y0;

			if (sel->x0 < 0) sel->x0 = 0;
			if (sel->x1 < 0) sel->x1 = 0;
			if (sel->x0 >= ed->image->width) sel->x0 = ed->image->width;
			if (sel->x1 >= ed->image->width) sel->x1 = ed->image->width;

			if (sel->y0 < 0) sel->y0 = 0;
			if (sel->y1 < 0) sel->y1 = 0;
			if (sel->y0 > ed->image->height) sel->y0 = ed->image->height;
			if (sel->y1 > ed->image->height) sel->y1 = ed->image->height;

			ed->move_pixels_start_x = px;
			ed->move_pixels_start_y = py;

			int w = ed->move_data->width;
			for (int y = sel->y0; y < sel->y1; ++y) {
				for (int x = sel->x0; x < sel->x1; x++) {
					s32 *out = reinterpret_cast<s32 *>(ed->move_data->data);
					s32 *in = reinterpret_cast<s32 *>(ed->image->data);

					assert(ed->move_offset_x < ed->image->width);
					assert(ed->move_offset_y < ed->image->height);
					in[x + y * ed->image->width] = out[(x - sel->x0 + ed->move_offset_x) + (y - sel->y0 + ed->move_offset_y) * w];
				}
			}			

			if (!ed->mouse_button_left) {
				ed->select_mode = NOT_SELECTING;

				ed->move_pixels_start_x = 0;
				ed->move_pixels_start_y = 0;

				glDeleteTextures(1, &ed->move_data->texID);
				free(ed->move_data->data);
				ed->move_data->data = nullptr;

				delete ed->move_data;
				ed->move_data = nullptr;
			}
			ed->is_dirty = true;
			return;
		}
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
		if (is_in_space_occupied_by_palette(cx, cy, &ed->palette) && ed->mouse_button_left) {
			Palette *p = &ed->palette;
			int tile_size = p->tile_size;
			int x = (cx - p->x) / tile_size;
			int y = (cy - p->y) / tile_size;

			int selection = x + (y * p->line_break);
			// printf("%d\n", selection);
			if (selection >= p->num_colors) return;

			assert(selection >= 0);
			ed->color = p->colors[selection];
		}
		bool success = get_pixel_pointed_at(ed, cx, cy, &px, &py);
		if (success && !ed->lcontrol && ed->mouse_button_left) {
			char *data = ed->image->data;
			data = data + ((px + py*ed->image->width) * 4);
			if (point_in_region(&ed->selection, px, py)) write_color_to(data, ed->color);
			ed->is_dirty = true;
		} else if (success && ed->lcontrol && ed->mouse_button_left) {
			char *data = ed->image->data;
			data = data + ((px + py*ed->image->width) * 4);
			ed->color = get_color_from(data);
			ed->is_dirty = true;
		}
	}
}

// int stbi_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes);
static void write_image_to_disk(OS_Window win, Image *im) {
	if (!im->path) {
		im->path = os_open_file_dialog(win, true);
		if (!im->path) return;

		char *win_text = static_cast<char *>(alloc(strlen("Simp") + strlen(" - ") + strlen(im->path) + 1));
		sprintf(win_text, "%s - %s", "Simp", im->path);
		os_set_window_title(win, win_text);
		free(win_text);
	}
	if (stbi_write_png(im->path, im->width, im->height, 4, im->data, im->width * 4)) {
		printf("Saved image to %s\n", im->path);
	}
}

static void zoom_editor_one_tick(Editor_Window *ed, s32 x, s32 y, bool down) {
	float start_scale = ed->image_scale;
	if (start_scale <= 0.5 && down) return;
	s32 diff_x = ed->image_x-x;
	s32 diff_y = ed->image_y-y;
	// it took me awhile to interalize this:
	// if our corner is -32,-32 from the mouse point, then by 2x zoom, the corner would be pushed out to -64,-64
	// we only need add the diff between -64,-64 which is still -32,-32 so we add that to image_* to adjust it to the -64,-64 point
	// on zoom out the target becomes -16,-16 so we divide by -2 to give us 16,16 pixels -towards- the mouse
	if (down) {
		ed->image_scale *= 0.5;
		diff_x /= -2;
		diff_y /= -2;
	} else {
		ed->image_scale *= 2.0;
	}
	ed->image_x += diff_x;
	ed->image_y += diff_y;
	ed->is_dirty = true;
}

static Array<Editor_Window *> windows;

int main(int argc, char **argv) {

	Editor_Window *ed = create_editor_window(windows);
	os_make_current(ed->os_window, ed->os_gl_context);

	if (argc < 2) {
		ed->image = generate_default_image();
	} else {
		ed->image = load_image(argv[1]);
		if (!ed->image) {
			printf("Couldn't load image: %s\n", argv[1]);
			return -1;
		}
		
		Image *im = ed->image;
		char *win_text = static_cast<char *>(alloc(strlen("Simp") + strlen(" - ") + strlen(im->path) + 1));
		sprintf(win_text, "%s - %s", "Simp", im->path);
		os_set_window_title(ed->os_window, win_text);
		free(win_text);
	}

	ed->selection.x0 = 0;
	ed->selection.y0 = 0;
	ed->selection.x1 = ed->image->width;
	ed->selection.y1 = ed->image->height;

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
						zoom_editor_one_tick(ed, ev.x, ev.y, ev.down);
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
						// printf("LCONTROL\n");
					} else if (ev.key == Key_Type::KEY_S && ev.mod == Key_Type::LCONTROL && ev.down) {
						write_image_to_disk(ed->os_window, ed->image);
					} else if (ev.key == Key_Type::KEY_S && ev.down) {
						if (!ed->select_mode) ed->select_mode = BEGIN_SELECTION;
					} else if (ev.key == Key_Type::KEY_T && ev.down) {
						ed->show_mini_map = !ed->show_mini_map;
						ed->is_dirty = true;
					} else if (ev.key == Key_Type::KEY_M && ev.down) {
						if (!ed->select_mode) ed->select_mode = BEGIN_MOVE_SELECTED_PIXELS;
					} else if (ev.key == Key_Type::KEY_A && ev.down) {
						if (ed->select_mode == BEGIN_SELECTION) {
							ed->select_mode = NOT_SELECTING;
							ed->selection.x0 = 0;
							ed->selection.y0 = 0;
							ed->selection.x1 = ed->image->width;
							ed->selection.y1 = ed->image->height;
						}
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