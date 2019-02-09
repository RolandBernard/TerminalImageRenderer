// Copyright (c) 2019 Roland Bernard

#include <ncurses.h>
#include <stdlib.h>
#include <assert.h>

#include "tir.h"

#define SUP_SAMP_VER 1
#define SUP_SAMP_HOR 1

/* 0 - full
 * 1 - half
 * 2 - quart
 */
#define SUP_CHAR_MODE 0

#define TIR_WIDTH() (SUP_SAMP_HOR*tir_width)
#define TIR_HEIGHT() (SUP_SAMP_VER*tir_height)

static int tir_width = 0;
static int tir_height = 0;

static tir_color_t* tir_buffer = NULL;

int tir_get_width() {
	return TIR_WIDTH();
}

int tir_get_height() {
	return TIR_HEIGHT();
}

int tir_init_scr() {
	if(initscr() == NULL ||
		cbreak() == ERR ||
		noecho() == ERR)
		return ERR;
	else {
		tir_refresh_size();
		return OK;
	}
}

tir_color_t* tir_get_buffer() {
	return tir_buffer;
}

tir_color_t* tir_get_pixel(int x, int y) {
	if(x >= 0 && x < tir_width && y >= 0 && y < tir_height)
		return tir_buffer + y*tir_width + x;
	else
		return NULL;
}

static tir_color_t* tir_get_char_pixel(int cx, int cy, int x, int y) {
	return tir_get_pixel(cx*SUP_SAMP_HOR+x, cy*SUP_SAMP_VER+y);
}

static int tir_get_char_loss(unsigned char* map, int pw, int ph, int cx, int cy, tir_color_t* colf, tir_color_t* colb) {
			int fcolor[3] = {0, 0, 0};
			int bcolor[3] = {0, 0, 0};
			// Full char
			for(int x = 0; x < SUP_SAMP_HOR; x++)
				for(int y = 0; y < SUP_SAMP_VER; y++) {
					tir_color_t* pcolor = tir_get_char_pixel(cx, cy, x, y);
					if(map[y*ph/SUP_SAMP_VER*pw+x*pw/SUP_SAMP_HOR]) {
						fcolor[0] += (int)pcolor[0][0];
						fcolor[1] += (int)pcolor[0][1];
						fcolor[2] += (int)pcolor[0][2];
					} else {
						bcolor[0] += (int)pcolor[0][0];
						bcolor[1] += (int)pcolor[0][1];
						bcolor[2] += (int)pcolor[0][2];
					}
				}
			fcolor[0] /= SUP_SAMP_HOR*SUP_SAMP_VER;
			fcolor[1] /= SUP_SAMP_HOR*SUP_SAMP_VER;
			fcolor[2] /= SUP_SAMP_HOR*SUP_SAMP_VER;
			int c_loss = 0;
			for(int x = 0; x < SUP_SAMP_HOR; x++)
				for(int y = 0; y < SUP_SAMP_VER; y++) {
					tir_color_t* pcolor = tir_get_char_pixel(cx, cy, x, y);
					if(map[y*ph/SUP_SAMP_VER*pw+x*pw/SUP_SAMP_HOR]) {
						c_loss += abs((int)pcolor[0][0] - fcolor[0]);
						c_loss += abs((int)pcolor[0][1] - fcolor[1]);
						c_loss += abs((int)pcolor[0][2] - fcolor[2]);
					} else {
						c_loss += abs((int)pcolor[0][0] - bcolor[0]);
						c_loss += abs((int)pcolor[0][1] - bcolor[1]);
						c_loss += abs((int)pcolor[0][2] - bcolor[2]);
					}
				}
			colf[0][0] = (unsigned char)fcolor[0];
			colf[0][1] = (unsigned char)fcolor[1];
			colf[0][2] = (unsigned char)fcolor[2];
			
			colb[0][0] = (unsigned char)bcolor[0];
			colb[0][1] = (unsigned char)bcolor[1];
			colb[0][2] = (unsigned char)bcolor[2];

			return c_loss;
}

static const unsigned char map_block[] = {1};

void tir_refresh() {
	for(int cx = 0; cx < tir_width; cx++)
		for(int cy = 0; cy < tir_height; cy++) {
			int min_loss = 3*255*SUP_SAMP_HOR*SUP_SAMP_VER+1; // Maximum posible loss
			const char* min = NULL;
			tir_color_t min_fc;
			tir_color_t min_bc;

			tir_color_t tmp_fc;
			tir_color_t tmp_bc;

			// Full Block
			int loss = tir_get_char_loss(map_block, 1, 1, cx, cy, &tmp_fc, &tmp_bc);
			if(loss < min_loss) {
				min = "";
				min_fc[0] = tmp_fc[0];
				min_fc[1] = tmp_fc[1];
				min_fc[2] = tmp_fc[2];
				min_bc[0] = tmp_bc[0];
				min_bc[1] = tmp_bc[1];
				min_bc[2] = tmp_bc[2];
				min_loss = loss;
			}

			
		}
}

void tir_refresh_size() {
	int new_width;
	int new_height;

	getmaxyx(stdscr, new_width, new_height);

	if(new_width != tir_width || new_height != tir_height) {
		tir_width = new_width;
		tir_height = new_height;
		tir_buffer = realloc(tir_buffer, TIR_WIDTH()*TIR_HEIGHT()*sizeof(tir_color_t));
		// TODO
		assert(tir_buffer != NULL);
	}
}

int tir_end_scr() {
	return endwin();
}


