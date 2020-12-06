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
#include <sys/time.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

using namespace rgb_matrix;

#define WAV_FN "Korg-M3R-High-Wood-Block.wav"
#define PCM_DEVICE "default"
#define BDF_FONT_FILE "fonts/teletactile.bdf"
#define BUTTON_1_PIN 10 /* MOSI */
#define BUTTON_2_PIN 9 /* MISO */

// Layout
#define SECONDS_X 0
#define SECONDS_Y 0

volatile sig_atomic_t stop_val = 0;
static 	snd_pcm_t *m_pcm_handle = NULL;
static unsigned int m_rate = 44100;
static const unsigned m_channels = 2;
static snd_pcm_uframes_t m_nframes = 0;
static rgb_matrix::Font m_font;
static RGBMatrix* m_matrix = NULL;
static unsigned char* m_wavbuf = NULL;

static void init_gpio(void);
static RGBMatrix* init_matrix(void);
static void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result);

static void load_font(void);
static void init_sound(void);
static void shutdown_sound(void);
static int get_file_sz(FILE* fd);
static unsigned char* get_wavfile(const char* wav_fn);
static void play_tick(void);
static void start_interval_timer(void);
static void install_sigint_handler(void);


static void sigint_handler(int signum) {
	printf("sigint\n");
	stop_val = 1;
}

void button_1_event(int gpio, int level, uint32_t tick) {
  printf("Button 1 level=%d tick=%u\n", level, tick);
  start_interval_timer();
}

void button_2_event(int gpio, int level, uint32_t tick) {
  printf("Button 2 level=%d tick=%u\n", level, tick);
}


static void init_gpio(void) {
  assert(gpioInitialise()>=0);

  gpioSetMode(BUTTON_1_PIN, PI_INPUT);  
  gpioSetPullUpDown(BUTTON_1_PIN, PI_PUD_UP); // Sets a pull-up.
  gpioSetAlertFunc(BUTTON_1_PIN, button_1_event);

  gpioSetMode(BUTTON_2_PIN, PI_INPUT);  
  gpioSetPullUpDown(BUTTON_2_PIN, PI_PUD_UP); // Sets a pull-up.
  gpioSetAlertFunc(BUTTON_2_PIN, button_2_event);

}

static RGBMatrix* init_matrix(void) {
  RGBMatrix::Options matrix_options;
  RuntimeOptions runtime_opt;
  matrix_options.rows = 64;
  matrix_options.cols = 64;

  RGBMatrix *m = rgb_matrix::CreateMatrixFromOptions(matrix_options, runtime_opt);
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

static void update_counter(unsigned int seconds, RGBMatrix* matrix, rgb_matrix::Font& font) {
  Color fg_color(255, 255, 0);
  Color bg_color(0, 0, 0);
  const int letter_spacing=0;

  char seconds_text[16];
  sprintf(seconds_text, "%5u", seconds);
  // Draw seconds
  rgb_matrix::DrawText(matrix, font, SECONDS_X-10, SECONDS_Y + font.baseline(),
                        fg_color, &bg_color, seconds_text,
                        letter_spacing);

	  const int width = m_matrix->width() - 1;
	  const int height = m_matrix->height() - 1;
	  // Borders
	  DrawLine(m_matrix, 0, 0, width, 0, Color(255, 0, 0));

}

static void shutdown_sound(void) {
	snd_pcm_close(m_pcm_handle);
	free(m_wavbuf);
}

static int get_file_sz(FILE* fd) {
	int cur = ftell(fd);
	fseek(fd, 0, SEEK_END);
	int sz = ftell(fd);
	fseek(fd, cur, SEEK_SET);
	return sz;
}

static unsigned char* get_wavfile(const char* wav_fn) {
	FILE* fd = fopen (wav_fn, "rb");
	int sz = get_file_sz(fd);
	assert(sz > 44);
	//wav header is 44
	fseek(fd, 44, SEEK_SET);
	unsigned char* buf = (unsigned char*)malloc(sz-44);
	assert(buf);
	int ret = fread(buf, sz-44, 1, fd);	
	assert(ret==1);
	fclose(fd);
	
	m_nframes = sz/4;

	return buf;
}
static void play_tick(void) {
	snd_pcm_wait(m_pcm_handle, 2000);
	snd_pcm_prepare(m_pcm_handle);
	snd_pcm_writei (m_pcm_handle, m_wavbuf, m_nframes/2);
}

void init_sound(void) {
	snd_pcm_hw_params_t *params;

	int err = snd_pcm_open(&m_pcm_handle, PCM_DEVICE,	SND_PCM_STREAM_PLAYBACK, 0);
	if(err <0) printf("ERROR: Can't open \"%s\" PCM device. %s\n", PCM_DEVICE, snd_strerror(err));

	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(m_pcm_handle, params);

	err = snd_pcm_hw_params_set_access(m_pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if(err<0) printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(err));

	err = snd_pcm_hw_params_set_format(m_pcm_handle, params, SND_PCM_FORMAT_S16_LE);
	if(err<0) printf("ERROR: Can't set format. %s\n", snd_strerror(err));
	
	err = snd_pcm_hw_params_set_channels(m_pcm_handle, params, m_channels);
	if(err<0) printf("ERROR: Can't set channels number. %s\n", snd_strerror(err));

	err = snd_pcm_hw_params_set_rate_near(m_pcm_handle, params, &m_rate, 0);
	if(err<0) printf("ERROR: Can't set rate. %s\n", snd_strerror(err));

	err = snd_pcm_hw_params(m_pcm_handle, params);
	if(err<0) printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(err));
	
	m_wavbuf = get_wavfile(WAV_FN);
}


void timer_handler (int signum)
{
 static int count = 1;
 update_counter(count, m_matrix, m_font);
 play_tick();
 count++;
}

static void start_interval_timer(void) {
	struct sigaction sa;
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
 	
	sigaction (SIGALRM , &sa, NULL);
	struct itimerval timer;

    timer.it_value.tv_sec = 1;
	timer.it_value.tv_usec = 0;
	/* ... and every 1000 msec after that. */
	timer.it_interval.tv_sec = 1;
	timer.it_interval.tv_usec = 0;
	
	setitimer (ITIMER_REAL, &timer, NULL);
}

static void install_sigint_handler(void) {
	struct sigaction sa;
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &sigint_handler;
	sigaction(SIGINT, &sa, NULL);
}

static void load_font(void) {
	if (!m_font.LoadFont(BDF_FONT_FILE)) fprintf(stderr, "Couldn't load font '%s'\n", BDF_FONT_FILE);
}

int main(int argc, char *argv[]) {
  install_sigint_handler();

  //init_gpio();
  m_matrix = init_matrix();
  assert(m_matrix);
  
  load_font();
  init_sound();
  start_interval_timer();
  
  play_tick();
  while(!stop_val) {}

  // Finished. Shut down the RGB matrix.
  printf("Shut 'er down\n");

  m_matrix->Clear();
  delete m_matrix;

  //gpioTerminate();
  shutdown_sound();

  return EXIT_SUCCESS;
}
