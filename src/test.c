// Copyright (c) 2019 Roland Bernard

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include "tir.h"

void draw() {
	tir_lock_buffer();
	for(int x = 0; x < tir_get_width(); x++)
		for(int y = 0; y < tir_get_height(); y++) {
			tir_color_t* pix = tir_get_pixel(x, y);
			float xf = 4.0f*x/tir_get_width()-2;
			float yf = 4.0f*y/tir_get_height()-2;
			if(xf*xf+yf*yf <= 1) {
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

static int running = 1;

void sig_hand(int sig) {
	if(sig == SIGINT)
		running = 0;
}

int main() {
	if(signal(SIGINT, sig_hand) == SIG_ERR) {
		perror("couldn't set signal");
		exit(EXIT_FAILURE);
	}
	tir_init_scr();

	draw();
	tir_set_winch_callback(draw);

	while(running) {
		usleep(100000);
	}

	tir_end_scr();
	return 0;
}
