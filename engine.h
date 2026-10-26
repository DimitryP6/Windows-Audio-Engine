#ifndef ENGINE_H
#define ENGINE_H

#include "pcm_format.h"

#include <atomic>

struct engine_config {
    unsigned int play_card, play_device;
    unsigned int cap_card, cap_device;
    unsigned int rate, render_block_size;
    unsigned int cap_period_size, play_period_size;
    unsigned int cap_period_count, play_period_count;
    unsigned int priming_frames;
    unsigned int cap_channels, play_channels;
    pcm_format   cap_format, play_format;
};

// The part every app shares. It plays nothing by itself: a derived class
// writes render()
class AudioEngine {
public:
    struct AudioContext {
        unsigned rate, in_channels, out_channels, block_size;
        const float* block_in;
        float*       block_out;
    };

    // Virtual: deleting a derived engine through an AudioEngine pointer then
    // runs the derived destructor too
    virtual ~AudioEngine();

    void configure(const engine_config &cfg);

    bool is_ready() { return ready; }

    bool run();

    void stop() { running = false; }

private:
    // Pure virtual (= 0): AudioEngine has no render() of its own, so it
    // cannot be created, and each derived engine must write one. Private: a
    // derived class overrides it but cannot call it; the public run() does
    virtual void render(const AudioContext &ctx) = 0;

    void convert_to_float();
    void convert_to_raw();

    bool prime_playback();

    pcm *play_pcm = nullptr;
    pcm *cap_pcm  = nullptr;

    unsigned char *cap_block_raw  = nullptr;
    unsigned char *play_block_raw = nullptr;

    float *cap_block_float  = nullptr;
    float *play_block_float = nullptr;

    AudioContext ctx = {};

    unsigned int threshold_frames                = 0;
    unsigned int cap_bytes_per_sample            = 0;
    unsigned int play_bytes_per_sample           = 0;
    unsigned int cap_bytes_per_sample_container  = 0;
    unsigned int play_bytes_per_sample_container = 0;
    unsigned int cap_samples_per_block           = 0;
    unsigned int play_samples_per_block          = 0;
    double       cap_scale                       = 0.0;
    double       play_scale                      = 0.0;

    std::atomic<bool> running = true;

    bool ready = false;
};

#endif
