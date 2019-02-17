// Copyright (c) 2019 Roland Bernard

#include <stdlib.h>
#include <assert.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "tir.h"

#define SUP_SAMP_VER 2
#define SUP_SAMP_HOR 2

#define TIR_WIDTH() (SUP_SAMP_HOR*tir_width)
#define TIR_HEIGHT() (SUP_SAMP_VER*tir_height)

#define ERR -1
#define OK 0

static pthread_rwlock_t tir_buffer_lock = PTHREAD_RWLOCK_INITIALIZER;

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

void tir_set_winch_callback(void (*func)()) {
	tir_winch_callback = func;
}

/*
 * We need a own thread for resizing because we have to wait until the buffer is no longer used
 */
static void* sig_thread(void* ud) {
	long sig = (long)ud;
	if(sig == SIGWINCH) {
		tir_refresh_size();
		if(tir_winch_callback != NULL)
			tir_winch_callback();
	}
	return NULL;
}

static void sig_hand(int sig) {
	pthread_t thr;
	pthread_create(&thr, NULL, sig_thread, (void*)(long)sig);
}

int tir_init_scr() {
	if(tir_buffer != NULL)
		return ERR;

	if(tcgetattr(STDIN_FILENO, &tir_old_term) == -1)
		return ERR;

	struct termios newterm = tir_old_term;
	newterm.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	newterm.c_oflag &= ~(OPOST);
	newterm.c_cflag |= (CS8);
	newterm.c_lflag &= ~(ICANON | ECHO | IEXTEN);
	newterm.c_cc[VMIN] = 0;
	newterm.c_cc[VTIME] = 0;
	if(tcsetattr(STDIN_FILENO, 0, &newterm) == -1)
		return ERR;
	write(STDIN_FILENO, "\x1b[?25l", 6);

	tir_refresh_size();
	
	if(signal(SIGWINCH, sig_hand) == SIG_ERR)
		return ERR;
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

static void tir_w_lock() {
	pthread_rwlock_wrlock(&tir_buffer_lock);
}

void tir_lock_buffer() {
	pthread_rwlock_rdlock(&tir_buffer_lock);
}

void tir_unlock_buffer() {
	pthread_rwlock_unlock(&tir_buffer_lock);
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
	for(int x = 0; x < SUP_SAMP_HOR; x++)
		for(int y = 0; y < SUP_SAMP_VER; y++) {
			tir_color_t* pcolor = tir_get_char_pixel(cx, cy, x, y);
			if(map[y*ph/SUP_SAMP_VER*pw+x*pw/SUP_SAMP_HOR]) {
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
#if SUP_SAMP_HOR >= 2
static const unsigned char tir_map_half_vert_block[] = {
	1, 0
};
#endif
#if SUP_SAMP_VER >= 2
static const unsigned char tir_map_half_hor_block[] = {
	1,
	0
};
#if SUP_SAMP_HOR >= 2
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
#endif
#endif

static const unsigned char* tir_maps[] = {
	tir_map_full_block,
#if SUP_SAMP_HOR >= 2
	tir_map_half_vert_block,
#endif
#if SUP_SAMP_VER >= 2
	tir_map_half_hor_block,
#if SUP_SAMP_HOR >= 2
	tir_map_quart_upleft_block,
	tir_map_quart_upright_block,
	tir_map_quart_downleft_block,
	tir_map_quart_downright_block,
	tir_map_quart_diag_block,
#endif
#endif
};
static const int tir_map_sizes[][2] = {
	{1, 1},
#if SUP_SAMP_HOR >= 2
	{2, 1},
#endif
#if SUP_SAMP_VER >= 2
	{1, 2},
#if SUP_SAMP_HOR >= 2
	{2, 2},
	{2, 2},
	{2, 2},
	{2, 2},
	{2, 2},
#endif
#endif
};
static const char* tir_map_chars[] = {
	"█",
#if SUP_SAMP_HOR >= 2
	"▌",
#endif
#if SUP_SAMP_VER >= 2
	"▀",
#if SUP_SAMP_HOR >= 2
	"▘",
	"▝",
	"▖",
	"▗",
	"▚",
#endif
#endif
};

void tir_refresh() {
	tir_lock_buffer();
	if(tir_buffer != NULL) /*Init has to be called*/ {
		char screen_buffer[tir_width*tir_height*64];
		int buffer_size = 0;

		memcpy(screen_buffer+buffer_size, "\x1b[H", 3);
		buffer_size += 3;

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
			}
		}
		
		write(STDOUT_FILENO, screen_buffer, buffer_size);
	}
	tir_unlock_buffer();
}

void tir_refresh_size() {
	int new_width;
	int new_height;

	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	new_width = w.ws_col;
	new_height = w.ws_row;

	if(new_width != tir_width || new_height != tir_height) {
		tir_w_lock();
		tir_width = new_width;
		tir_height = new_height;
		tir_buffer = realloc(tir_buffer, TIR_WIDTH()*TIR_HEIGHT()*sizeof(tir_color_t));
		assert(tir_buffer != NULL);
		tir_unlock_buffer();
	}
}

int tir_end_scr() {
	if(tir_buffer != NULL) {
		tir_w_lock();
		free(tir_buffer);
		tir_buffer = NULL;
		tcsetattr(STDIN_FILENO, 0, &tir_old_term);
		write(STDOUT_FILENO, "\x1b[m\x1b[2J\x1b[H\x1b[?25h", 16);
		tir_unlock_buffer();
		return OK;
	} else
		return ERR;
}


