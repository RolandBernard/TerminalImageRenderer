// Copyright (c) 2019 Roland Bernard

// Small library to output rgb images to terminal

#ifndef __TIR_H__
#define __TIR_H__

#define TIR_COPY_COLOR(X, Y) X[0] = Y[0]; X[1] = Y[1]; X[2] = Y[2];

typedef unsigned char tir_color_t[3];

tir_color_t* tir_get_pixel(int x, int y);

void tir_set_winch_callback(void (*func)());

int tir_get_width();

int tir_get_height();

int tir_init_scr();

void tir_lock_buffer();

void tir_unlock_buffer();

tir_color_t* tir_get_buffer();

void tir_refresh();

void tir_refresh_size();

int tir_end_scr();

#endif
