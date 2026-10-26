/* MUST5973 ST: Audio Software Architecture and Design
 * Northeastern University — Fall 2026
 * Victor Zappi, 2026
 *
 * Unit 2 — Class 8: App Framework
 *
 * 1_sine_app — the first app, built on a reusable engine
 *
 * AudioEngine is split in two. The base class keeps what every app needs:
 * the PCMs, the conversions, priming, run() and stop(). Its render() is
 * pure virtual, so the base class no longer decides what is played.
 * SineEngine derives from it and adds this app's part. A new app needs only
 * a new derived class.
 *
 * Messages through a FIFO plus a derived engine: this is the app framework,
 * the smallest structure that every app from here on reuses.
 *
 * The keys move in musical steps again, as in 3_thread_prios.
 *
 * One piece is left for you, in sine_engine.h and sine_engine.cpp: the rest
 * of SineEngine.
 *
 * main.cpp — audio parameters, the signal handler, the two thread functions
 *            and the app's engine
 *
 * build: g++ -Wall main.cpp engine.cpp sine_engine.cpp cli.cpp control.cpp \
 *            -ltinyalsa_t3 -pthread -o 1_sine_app
 * run:   ./1_sine_app [-b frames] [-p CAP,PLAY] [-n CAP,PLAY]
 *              [-c CAP,PLAY] [-f CAP,PLAY] [-t frames] [-r rate]
 *                     [-o CARD,DEVICE] [-i CARD,DEVICE] (-h for help)
 *        then a/z (frequency), s/x (amplitude), space (start / stop)
 *        or q (quit), each + Enter
 * kill:  pkill -f 1_sine_app   (from a second shell, if it hangs)
 */

#include "sine_engine.h"
#include "cli.h"
#include "control.h"

#include "pcm_format.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <signal.h>
#include <thread>

static unsigned int play_card    = 0;
static unsigned int play_device  = 2;
static unsigned int cap_card     = 0;
static unsigned int cap_device   = 3;

static unsigned int srate             = 48000;
static unsigned int render_block_size = 960;
static unsigned int cap_period_size   = 960;
static unsigned int play_period_size  = 960;
static unsigned int cap_period_count  = 2;
static unsigned int play_period_count = 4;
static unsigned int priming_frames    = 0;
static unsigned int cap_channels      = 2;
static unsigned int play_channels     = 2;
static pcm_format   cap_format        = PCM_FORMAT_S16_LE;
static pcm_format   play_format       = PCM_FORMAT_S16_LE;

// AudioEngine is abstract now: main() creates the app's engine
static SineEngine engine;

static void on_sigint(int)
{
    engine.stop();
}

static int install_sigint_handler()
{
    struct sigaction sa = {};

    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    return sigaction(SIGINT, &sa, nullptr);
}

static void start_engine(SineEngine *engine, bool *run_ok)
{
    sched_param param = {};
    param.sched_priority = 99;

    const int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0)
        fprintf(stderr, "WARNING: cannot set audio thread priority: %s\n",
                strerror(ret));

    *run_ok = engine->run();
}

static void start_controls(SineEngine *engine)
{
    sched_param param = {};
    param.sched_priority = 90;

    const int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0)
        fprintf(stderr, "WARNING: cannot set control thread priority: %s\n",
                strerror(ret));

    run_controls(engine);
}

int main(int argc, char **argv)
{
    switch (parse_args(argc, argv, &play_card, &play_device,
                       &cap_card, &cap_device, &srate, &render_block_size,
                       &cap_period_size, &play_period_size,
                       &cap_period_count, &play_period_count,
                       &priming_frames, &cap_channels, &play_channels,
                       &cap_format, &play_format)) {
    case PARSE_HELP:  return EXIT_SUCCESS;
    case PARSE_ERROR: return EXIT_FAILURE;
    case PARSE_OK:    break;
    }

    if (install_sigint_handler() < 0) {
        fprintf(stderr, "FAIL: cannot install SIGINT handler\n");
        return EXIT_FAILURE;
    }

    printf("Sine App — Class 8: App Framework\n\n");

    engine_config config = {};
    config.play_card         = play_card;
    config.play_device       = play_device;
    config.cap_card          = cap_card;
    config.cap_device        = cap_device;
    config.rate              = srate;
    config.render_block_size = render_block_size;
    config.cap_period_size   = cap_period_size;
    config.play_period_size  = play_period_size;
    config.cap_period_count  = cap_period_count;
    config.play_period_count = play_period_count;
    config.priming_frames    = priming_frames;
    config.cap_channels      = cap_channels;
    config.play_channels     = play_channels;
    config.cap_format        = cap_format;
    config.play_format       = play_format;

    engine.configure(config);

    if (!engine.is_ready())
        return EXIT_FAILURE;

    printf("a / z : frequency up / down 50 cents\n");
    printf("s / x : amplitude up / down 1 dB\n");
    printf("space : start / stop\n");
    printf("q     : quit\n");
    printf("press Enter after each key\n\n");

    printf("starting the engine\n");
    printf("Ctrl-C to stop.\n");

    bool run_ok = false;

    std::thread audio_thread(start_engine, &engine, &run_ok);
    std::thread control_thread(start_controls, &engine);

    audio_thread.join();

    const int cancel_ret = pthread_cancel(control_thread.native_handle());
    if (cancel_ret != 0)
        fprintf(stderr, "FAIL: cannot cancel control thread: %s\n",
                strerror(cancel_ret));

    control_thread.join();

    if (!run_ok)
        return EXIT_FAILURE;

    printf("\nEngine stopped cleanly (:\n");

    return EXIT_SUCCESS;
}
