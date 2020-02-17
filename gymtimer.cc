// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Small example how write text.
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)

#include "led-matrix.h"
#include "graphics.h"
#include <pigpio.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


using namespace rgb_matrix;

#define BDF_FONT_FILE "fonts/gohufont-11.bdf"
#define BUTTON_1_PIN 10 /* MOSI */
#define BUTTON_2_PIN 9 /* MISO */

// Layout
#define SECONDS_X 0
#define SECONDS_Y 0

volatile sig_atomic_t stop_val = 0;

static void sighandler(int signum) {
	stop_val = 1;
}

void install_sighandler(void) {
  signal(SIGINT, sighandler);
}

void button_1_event(int gpio, int level, uint32_t tick) {
  printf("Button 1 level=%d tick=%u\n", level, tick);
}

void button_2_event(int gpio, int level, uint32_t tick) {
  printf("Button 2 level=%d tick=%u\n", level, tick);
}

void init_gpio(void) {
  assert(gpioInitialise()>=0);

  gpioSetMode(BUTTON_1_PIN, PI_INPUT);  
  gpioSetPullUpDown(BUTTON_1_PIN, PI_PUD_UP); // Sets a pull-up.
  gpioSetAlertFunc(BUTTON_1_PIN, button_1_event);

  gpioSetMode(BUTTON_2_PIN, PI_INPUT);  
  gpioSetPullUpDown(BUTTON_2_PIN, PI_PUD_UP); // Sets a pull-up.
  gpioSetAlertFunc(BUTTON_2_PIN, button_2_event);

}


RGBMatrix* init_matrix(void) {
  RGBMatrix::Options matrix_options;
  RuntimeOptions runtime_opt;
  matrix_options.rows = 64;
  matrix_options.cols = 64;

  RGBMatrix *m = rgb_matrix::CreateMatrixFromOptions(matrix_options,
                                                     runtime_opt);

  m->SetPWMBits(1); //1 seems ok?  May need more for colors??
  
  return m;
}

void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

void update_counter(unsigned int seconds, RGBMatrix* matrix, rgb_matrix::Font& font) {
  Color fg_color(255, 255, 0);
  Color bg_color(0, 0, 0);
  const int letter_spacing=0;

  char seconds_text[16];
  sprintf(seconds_text, "%5u", seconds);
  rgb_matrix::DrawText(matrix, font, SECONDS_X, SECONDS_Y + font.baseline(),
                        fg_color, &bg_color, seconds_text,
                        letter_spacing);
 
}

void clock_exec(RGBMatrix* matrix) {
  static const time_t MAX_SECONDS_DISP=1000;
  
  rgb_matrix::Font font;
  if (!font.LoadFont(BDF_FONT_FILE)) {
    fprintf(stderr, "Couldn't load font '%s'\n", BDF_FONT_FILE);
    return;
  }

  timespec starttime;
  clock_gettime(CLOCK_MONOTONIC, &starttime);

  timespec curtime;
  timespec difftime;
  while(!stop_val) {
    clock_gettime(CLOCK_MONOTONIC, &curtime);
    timespec_diff(&starttime, &curtime, &difftime);
    time_t seconds = difftime.tv_sec < MAX_SECONDS_DISP ? difftime.tv_sec : difftime.tv_sec % MAX_SECONDS_DISP;
    update_counter(seconds, matrix, font);
  }
}

int main(int argc, char *argv[]) {
  install_sighandler();

  init_gpio();
  RGBMatrix* matrix = init_matrix();
  assert(matrix);
    
  clock_exec(matrix);
  
  // Finished. Shut down the RGB matrix.
  printf("Shut 'er down\n");
  matrix->Clear();
  delete matrix;
  gpioTerminate();

  return EXIT_SUCCESS;
}
