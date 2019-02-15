// Copyright (c) 2019 Roland Bernard

#include <stdlib.h>
#include <assert.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "tir.h"

#define SUP_SAMP_VER 2
#define SUP_SAMP_HOR 2

#define TIR_WIDTH() (SUP_SAMP_HOR*tir_width)
#define TIR_HEIGHT() (SUP_SAMP_VER*tir_height)

#define ERR -1
#define OK 0

static int tir_width = 0;
static int tir_height = 0;

static tir_color_t* tir_buffer = NULL;
static struct termios tir_old_term;

static void (*tir_winch_callback)() = NULL;

int tir_get_width() {
	return TIR_WIDTH();
}

int tir_get_height() {
	return TIR_HEIGHT();
}

static void term_reset() { 
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	write(STDOUT_FILENO, "\x1b[m", 3);
	tcsetattr(STDIN_FILENO, 0, &tir_old_term);
}

void tir_set_winch_callback(void (*func)()) {
	tir_winch_callback = func;
}

static void sig_hand(int sig) {
	if(sig == SIGINT)
		exit(EXIT_SUCCESS);
	else if(sig == SIGWINCH) {
		tir_refresh_size();
		if(tir_winch_callback != NULL)
			tir_winch_callback();
	}
}

int tir_init_scr() {
	if(tir_buffer != NULL)
		return ERR;

	if(tcgetattr(STDIN_FILENO, &tir_old_term) == -1)
		return ERR;
	atexit(term_reset);
	signal(SIGINT, sig_hand);
	signal(SIGWINCH, sig_hand);

	struct termios newterm = tir_old_term;
	newterm.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	newterm.c_oflag &= ~(OPOST);
	newterm.c_cflag |= (CS8);
	newterm.c_lflag &= ~(ICANON | ECHO | IEXTEN);
	newterm.c_cc[VMIN] = 0;
	newterm.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, 0, &newterm);

	tir_refresh_size();
	return OK;
}

tir_color_t* tir_get_buffer() {
	return tir_buffer;
}

tir_color_t* tir_get_pixel(int x, int y) {
	if(x >= 0 && x < TIR_WIDTH() && y >= 0 && y < TIR_HEIGHT())
		return tir_buffer + y*TIR_WIDTH() + x;
	else
		return NULL;
}

static tir_color_t* tir_get_char_pixel(int cx, int cy, int x, int y) {
	return tir_get_pixel(cx*SUP_SAMP_HOR+x, cy*SUP_SAMP_VER+y);
}

static int tir_get_char_loss(const unsigned char* map, int pw, int ph, int cx, int cy, tir_color_t* colf, tir_color_t* colb) {
	int fcolor[3] = {0, 0, 0};
	int bcolor[3] = {0, 0, 0};
	int num_forg = 0;
	int num_backg = 0;
	// Full char
	for(int x = 0; x < pw; x++)
		for(int y = 0; y < ph; y++) {
			tir_color_t* pcolor = tir_get_char_pixel(cx, cy, x*SUP_SAMP_HOR/pw, y*SUP_SAMP_VER/ph);
			if(map[y*pw+x]) {
				fcolor[0] += (int)pcolor[0][0];
				fcolor[1] += (int)pcolor[0][1];
				fcolor[2] += (int)pcolor[0][2];
				num_forg++;
			} else {
				bcolor[0] += (int)pcolor[0][0];
				bcolor[1] += (int)pcolor[0][1];
				bcolor[2] += (int)pcolor[0][2];
				num_backg++;
			}
		}
	if(num_forg != 0) {
		fcolor[0] /= num_forg;
		fcolor[1] /= num_forg;
		fcolor[2] /= num_forg;
	}
	if(num_backg != 0) {
		bcolor[0] /= num_backg;
		bcolor[1] /= num_backg;
		bcolor[2] /= num_backg;
	}
	
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

// Defining posible chars
static const unsigned char tir_map_full_block[] = {1};
static const unsigned char tir_map_half_block[] = {
	1,
	0
};
static const unsigned char tir_map_quart_upleft_block[] = {
	1, 0,
	0, 0
};
static const unsigned char tir_map_quart_upright_block[] = {
	0, 1,
	0, 0
};
static const unsigned char tir_map_quart_downleft_block[] = {
	0, 0,
	1, 0
};
static const unsigned char tir_map_quart_downright_block[] = {
	0, 0,
	0, 1
};
static const unsigned char tir_map_quart_diag_block[] = {
	1, 0,
	0, 1
};

static const unsigned char* tir_maps[] = {
	tir_map_full_block,
	tir_map_half_block,
	tir_map_quart_upleft_block,
	tir_map_quart_upright_block,
	tir_map_quart_downleft_block,
	tir_map_quart_downright_block,
	tir_map_quart_diag_block,
};
static const int tir_map_sizes[][2] = {
	{1, 1},
	{1, 2},
	{2, 2},
	{2, 2},
	{2, 2},
	{2, 2},
	{2, 2},
};
static const char* tir_map_chars[] = {
	"█",
	"▀",
	"▘",
	"▝",
	"▖",
	"▗",
	"▚",
};

void tir_refresh() {
	if(tir_buffer != NULL) /*Init has to be called*/ {
		char screen_buffer[tir_width*tir_height*64];
		int buffer_size = 0;

		memcpy(screen_buffer+buffer_size, "\x1b[H\x1b[?25l", 9);
		buffer_size += 9;

		for(int cy = 0; cy < tir_height; cy++) {
			for(int cx = 0; cx < tir_width; cx++) {
				int min_loss = 3*255*SUP_SAMP_HOR*SUP_SAMP_VER+1; // Maximum posible loss
				const char* min = " ";
				tir_color_t min_fc = {0};
				tir_color_t min_bc = {0};

				tir_color_t tmp_fc;
				tir_color_t tmp_bc;

				for(int i = 0; i < sizeof(tir_maps)/sizeof(tir_maps[0]); i++) {
					int loss = tir_get_char_loss(tir_maps[i], tir_map_sizes[i][0], tir_map_sizes[i][1], cx, cy, &tmp_fc, &tmp_bc);
					if(loss < min_loss) {
						min = tir_map_chars[i];
						TIR_COPY_COLOR(min_bc, tmp_bc)
						TIR_COPY_COLOR(min_fc, tmp_fc)
						min_loss = loss;
					}
				}

				// TODO Different color support
				char str[39];
				sprintf(str, "\x1b[38;2;%hhu;%hhu;%hhum\x1b[48;2;%hhu;%hhu;%hhum",
						min_fc[0], min_fc[1], min_fc[2], min_bc[0], min_bc[1], min_bc[2]);
				memcpy(screen_buffer+buffer_size, str, strlen(str));
				buffer_size += strlen(str);
				memcpy(screen_buffer+buffer_size, min, strlen(min));
				buffer_size += strlen(min);
			}
			if(cy != tir_height-1) {
				memcpy(screen_buffer+buffer_size, "\n\r", 2);
				buffer_size += 2;
			} else {
				memcpy(screen_buffer+buffer_size, "\x1b[m", 3);
				buffer_size += 3;
			}
		}
		memcpy(screen_buffer+buffer_size, "\x1b[?25h", 6);
		buffer_size += 6;
		
		write(STDOUT_FILENO, screen_buffer, buffer_size);
	}
}

void tir_refresh_size() {
	int new_width;
	int new_height;

	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	new_width = w.ws_col;
	new_height = w.ws_row;

	if(new_width != tir_width || new_height != tir_height) {
		tir_width = new_width;
		tir_height = new_height;
		tir_buffer = realloc(tir_buffer, TIR_WIDTH()*TIR_HEIGHT()*sizeof(tir_color_t));
		assert(tir_buffer != NULL);
	}
}

int tir_end_scr() {
	if(tir_buffer != NULL) {
		free(tir_buffer);
		tir_buffer = NULL;
		return OK;
	} else
		return ERR;
}


