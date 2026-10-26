#include "engine.h"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>

void AudioEngine::configure(const engine_config &cfg)
{
    const char *cap_format_name  = "unknown";
    const char *play_format_name = "unknown";

    switch (cfg.cap_format) {
    case PCM_FORMAT_S16_LE:
        cap_bytes_per_sample = 2; cap_format_name = "S16_LE";  break;
    case PCM_FORMAT_S24_3LE:
        cap_bytes_per_sample = 3; cap_format_name = "S24_3LE"; break;
    case PCM_FORMAT_S24_LE:
        cap_bytes_per_sample = 3; cap_format_name = "S24_LE";  break;
    case PCM_FORMAT_S32_LE:
        cap_bytes_per_sample = 4; cap_format_name = "S32_LE";  break;
    default:
        fprintf(stderr, "FAIL: unsupported capture format\n");
        return;
    }

    switch (cfg.play_format) {
    case PCM_FORMAT_S16_LE:
        play_bytes_per_sample = 2; play_format_name = "S16_LE";  break;
    case PCM_FORMAT_S24_3LE:
        play_bytes_per_sample = 3; play_format_name = "S24_3LE"; break;
    case PCM_FORMAT_S24_LE:
        play_bytes_per_sample = 3; play_format_name = "S24_LE";  break;
    case PCM_FORMAT_S32_LE:
        play_bytes_per_sample = 4; play_format_name = "S32_LE";  break;
    default:
        fprintf(stderr, "FAIL: unsupported playback format\n");
        return;
    }

    cap_bytes_per_sample_container  = pcm_format_to_bits(cfg.cap_format) / 8;
    play_bytes_per_sample_container = pcm_format_to_bits(cfg.play_format) / 8;

    cap_samples_per_block  = cfg.render_block_size * cfg.cap_channels;
    play_samples_per_block = cfg.render_block_size * cfg.play_channels;

    const unsigned int cap_bytes_per_block  =
        cap_samples_per_block * cap_bytes_per_sample_container;
    const unsigned int play_bytes_per_block =
        play_samples_per_block * play_bytes_per_sample_container;

    cap_scale  = (1LL << (cap_bytes_per_sample * 8 - 1)) - 1;
    play_scale = (1LL << (play_bytes_per_sample * 8 - 1)) - 1;

    const unsigned int floor_frames =
        cfg.cap_period_size + cfg.render_block_size;
    const unsigned int play_ring =
        cfg.play_period_size * cfg.play_period_count;
    const unsigned int cap_ring  =
        cfg.cap_period_size * cfg.cap_period_count;

    threshold_frames = cfg.priming_frames ? cfg.priming_frames
                                      : floor_frames + cfg.render_block_size;

    cap_block_raw    = new unsigned char[cap_bytes_per_block]();
    play_block_raw   = new unsigned char[play_bytes_per_block]();
    cap_block_float  = new float[cap_samples_per_block]();
    play_block_float = new float[play_samples_per_block]();

    ctx.rate         = cfg.rate;
    ctx.in_channels  = cfg.cap_channels;
    ctx.out_channels = cfg.play_channels;
    ctx.block_size   = cfg.render_block_size;
    ctx.block_in     = cap_block_float;
    ctx.block_out    = play_block_float;

    printf("capture      : card %u, device %u\n",
           cfg.cap_card, cfg.cap_device);
    printf("playback     : card %u, device %u\n",
           cfg.play_card, cfg.play_device);
    printf("sample rate  : %6u Hz \n", cfg.rate);
    printf("render block : %6u frames -> %.2f ms per block\n",
           cfg.render_block_size, 1000.0 * cfg.render_block_size / cfg.rate);
    printf("cap ring     : %6u frames x %u -> %.2f ms\n",
           cfg.cap_period_size, cfg.cap_period_count,
           1000.0 * cfg.cap_period_size * cfg.cap_period_count / cfg.rate);
    printf("play ring    : %6u frames x %u -> %.2f ms\n",
           cfg.play_period_size, cfg.play_period_count,
           1000.0 * play_ring / cfg.rate);
    printf("priming      : %6u frames -> %.2f ms of latency\n",
           threshold_frames, 1000.0 * threshold_frames / cfg.rate);
    printf("channels     : capture %u, playback %u\n",
           cfg.cap_channels, cfg.play_channels);
    printf("format       : capture %s, playback %s\n\n",
           cap_format_name, play_format_name);

    if (threshold_frames < floor_frames) {
        fprintf(stderr, "FAIL: priming (%u frames) is below the floor of %u "
                        "frames\n", threshold_frames, floor_frames);
        return;
    }

    if (threshold_frames > play_ring) {
        fprintf(stderr, "FAIL: priming (%u frames) exceeds the playback ring "
                        "(%u frames)\n", threshold_frames, play_ring);
        return;
    }

    if (threshold_frames > cap_ring)
        fprintf(stderr, "NOTE: priming (%u frames) is deeper than the capture "
                        "ring (%u frames)\n\n", threshold_frames, cap_ring);

    pcm_config play_config = {};
    play_config.channels        = cfg.play_channels;
    play_config.rate            = cfg.rate;
    play_config.period_size     = cfg.play_period_size;
    play_config.period_count    = cfg.play_period_count;
    play_config.format          = cfg.play_format;
    play_config.start_threshold = threshold_frames;

    play_pcm = pcm_open(cfg.play_card, cfg.play_device,
                        PCM_OUT | PCM_NORESTART, &play_config);
    if (!pcm_is_ready(play_pcm)) {
        fprintf(stderr, "FAIL: cannot open playback %u,%u: %s\n",
                cfg.play_card, cfg.play_device, pcm_get_error(play_pcm));
        return;
    }

    pcm_config cap_config = {};
    cap_config.channels        = cfg.cap_channels;
    cap_config.rate            = cfg.rate;
    cap_config.period_size     = cfg.cap_period_size;
    cap_config.period_count    = cfg.cap_period_count;
    cap_config.format          = cfg.cap_format;
    cap_config.start_threshold = 1;

    cap_pcm = pcm_open(cfg.cap_card, cfg.cap_device,
                       PCM_IN | PCM_NORESTART, &cap_config);
    if (!pcm_is_ready(cap_pcm)) {
        fprintf(stderr, "FAIL: cannot open capture %u,%u: %s\n",
                cfg.cap_card, cfg.cap_device, pcm_get_error(cap_pcm));
        return;
    }

    ready = true;
}

AudioEngine::~AudioEngine()
{
    if (cap_pcm && pcm_close(cap_pcm) < 0)
        fprintf(stderr, "FAIL: capture pcm_close returned an error\n");

    if (play_pcm && pcm_close(play_pcm) < 0)
        fprintf(stderr, "FAIL: playback pcm_close returned an error\n");

    delete[] play_block_float;
    delete[] cap_block_float;
    delete[] play_block_raw;
    delete[] cap_block_raw;
}

void AudioEngine::convert_to_float()
{
    const unsigned int bits     = cap_bytes_per_sample * 8;
    const uint32_t     sign_bit = 1u << (bits - 1);
    const uint32_t     ext_mask = ~(sign_bit | (sign_bit - 1));

    for (unsigned int i = 0; i < cap_samples_per_block; i++) {
        const unsigned int sample_start = i * cap_bytes_per_sample_container;

        uint32_t sample = 0;

        for (unsigned int b = 0; b < cap_bytes_per_sample; b++)
            sample |= static_cast<uint32_t>(cap_block_raw[sample_start + b])
                      << (8 * b);

        if (sample & sign_bit)
            sample |= ext_mask;

        cap_block_float[i] =
            static_cast<float>(static_cast<int32_t>(sample) / cap_scale);
    }
}

void AudioEngine::convert_to_raw()
{
    for (unsigned int i = 0; i < play_samples_per_block; i++) {
        const float x = std::fmin(std::fmax(play_block_float[i], -1.0f), 1.0f);
        const long  q = std::lrint(x * play_scale);

        const unsigned int sample_start = i * play_bytes_per_sample_container;

        for (unsigned int b = 0; b < play_bytes_per_sample; b++)
            play_block_raw[sample_start + b] = q >> (8 * b);
    }
}

bool AudioEngine::prime_playback()
{
    const unsigned int bytes_per_frame = ctx.out_channels
                                         * play_bytes_per_sample_container;

    unsigned char *silence =
        new unsigned char[threshold_frames * bytes_per_frame]();

    const int ret = pcm_writei(play_pcm, silence, threshold_frames);

    delete[] silence;

    if (ret < 0) {
        fprintf(stderr, "FAIL: priming write: %s\n", pcm_get_error(play_pcm));
        return false;
    }

    return true;
}

bool AudioEngine::run()
{
    if (!prime_playback())
        return false;

    bool failed = false;

    while (running) {
        if (pcm_readi(cap_pcm, cap_block_raw, ctx.block_size) < 0) {
            if (errno == EPIPE) {
                printf("\nOverrun detected! Recovery in progress...\n");

                if (pcm_prepare(cap_pcm) < 0) {
                    fprintf(stderr, "\nFAIL: capture prepare: %s\n",
                            pcm_get_error(cap_pcm));
                    failed = true;
                    break;
                }

                continue;
            }

            fprintf(stderr, "\nFAIL: read: %s\n", pcm_get_error(cap_pcm));
            failed = true;
            break;
        }

        convert_to_float();

        // Whichever derived class this is, its render() runs here
        render(ctx);

        convert_to_raw();

        if (pcm_writei(play_pcm, play_block_raw, ctx.block_size) < 0) {
            if (errno == EPIPE) {
                printf("\nUnderrun detected! Recovery in progress...\n");

                if (pcm_prepare(play_pcm) < 0) {
                    fprintf(stderr, "\nFAIL: playback prepare: %s\n",
                            pcm_get_error(play_pcm));
                    failed = true;
                    break;
                }

                continue;
            }

            fprintf(stderr, "\nFAIL: write: %s\n", pcm_get_error(play_pcm));
            failed = true;
            break;
        }
    }

    return !failed;
}
