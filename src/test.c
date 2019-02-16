// Copyright (c) 2019 Roland Bernard

#include <unistd.h>
#include <stdio.h>

#include "tir.h"

void draw() {
	tir_lock_buffer();
	for(int x = 0; x < tir_get_width(); x++)
		for(int y = 0; y < tir_get_height(); y++) {
			tir_color_t* pix = tir_get_pixel(x, y);
			float xf = 2.0f*x/tir_get_width()-1;
			float yf = 2.0f*y/tir_get_height()-1;
			if(xf*xf+yf*yf < 1) {
				pix[0][0] = 101;
				pix[0][1] = 101;
				pix[0][2] = 101;
			} else {
				pix[0][0] = 255;
				pix[0][1] = 101;
				pix[0][2] = 255;
			}
		}
	tir_refresh();
	tir_unlock_buffer();
}

int main() {
	tir_init_scr();

	draw();
	tir_set_winch_callback(draw);

	while(1) {
		sleep(2);
	}

	tir_end_scr();
	return 0;
}
