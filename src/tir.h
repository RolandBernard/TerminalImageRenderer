// Copyright (c) 2019 Roland Bernard

// Small wrapper to output rgb images to terminal using ncurses

#ifndef __TIR_H__
#define __TIR_H__

typedef unsigned char tir_color_t[3];

tir_color_t* tir_get_pixel(int x, int y);

int tir_get_width();

int tir_get_height();

int tir_init_scr();

tir_color_t* tir_get_buffer();

void tir_refresh();

void tir_refresh_size();

int tir_end_scr();

#endif
