/* MUST5973 ST: Audio Software Architecture and Design
 * Northeastern University — Fall 2026
 * Victor Zappi, 2026
 *
 * Mini Project
 *
 * Linux build: g++ -Wall -std=c++17 main.cpp engine.cpp sine_engine.cpp cli.cpp control.cpp tinyalsa_backend.cpp \
 * -ltinyalsa_t3 -pthread -o 1_sine_app
 * 
 * https://learn.microsoft.com/en-us/windows/win32/tapi/libraries-required for ole32 for COM. MMCS in MMCS documentation.
 * Windows build: g++ -Wall -std=c++17 main.cpp engine.cpp sine_engine.cpp cli.cpp control.cpp windows_backend.cpp \
 * -lole32 -lavrt -o 1_sine_app.exe
 * 
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
#include "thread_priority.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>


#ifndef _WIN32
#include <pthread.h>
#endif


#include <signal.h>
#include <csignal>
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
#ifdef _WIN32
    return std::signal(SIGINT, on_sigint) == SIG_ERR ? -1 : 0;
#else
    struct sigaction sa = {};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    return sigaction(SIGINT, &sa, nullptr);
#endif
}

static void start_engine(SineEngine *engine, bool *run_ok)
{
    void* optional_handle = raise_audio_priority();
    *run_ok = engine->run();
    restore_audio_priority(optional_handle);
}

static void start_controls(SineEngine *engine)
{
    raise_control_priority();
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

    printf("Mini Project - Sine App\n\n");

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


#ifdef _WIN32
    // No pthread cancel on windows so let it go.
    control_thread.detach();
#else
    const int cancel_ret = pthread_cancel(control_thread.native_handle());
    if (cancel_ret != 0)
        fprintf(stderr, "FAIL: cannot cancel control thread: %s\n",
                strerror(cancel_ret));
    
    control_thread.join();
#endif


    if (!run_ok)
        return EXIT_FAILURE;

    printf("\nEngine stopped cleanly (:\n");

    return EXIT_SUCCESS;
}
