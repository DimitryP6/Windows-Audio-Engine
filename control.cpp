#include "control.h"

#include <cmath>
#include <cstdio>

// The steps of 3_thread_prios (50 cents, 1 dB), one message per key as in
// 0_control_fifo. The switch only changes the state; the message goes out
// after it
void run_controls(SineEngine *engine)
{
    float frequency = 440.0f;
    float level     = -6.0f;
    bool  playing   = false;

    const float max_hz    = 440.0f * std::pow(2.0f, 5400.0f / 1200.0f);
    const float min_hz    = 440.0f * std::pow(2.0f, -5400.0f / 1200.0f);
    const float max_level = 0.0f;
    const float min_level = -60.0f;

    int key;

    while ((key = getchar()) != EOF) {
        float phase = -1.0f;

        switch (key) {
        case 'a': frequency *= std::pow(2.0f, 50.0f / 1200.0f);  break;
        case 'z': frequency *= std::pow(2.0f, -50.0f / 1200.0f); break;
        case 's': level     += 1.0f;                             break;
        case 'x': level     -= 1.0f;                             break;
        case ' ':
            playing = !playing;
            if (playing) {
                frequency = 880.0f;
                phase     = 0.0f;
            }
            break;
        case 'q':
            engine->stop();
            return;
        default:
            continue;
        }

        frequency = std::fmin(std::fmax(frequency, min_hz), max_hz);
        level     = std::fmin(std::fmax(level, min_level), max_level);

        ControlMessage msg = {};
        msg.frequency = frequency;
        msg.amplitude = playing ? std::pow(10.0f, level / 20.0f) : 0.0f;
        msg.phase     = phase;

        if (!engine->push_control(msg))
            fprintf(stderr, "WARNING: FIFO full, message dropped\n");

        printf("frequency %.2f Hz, level %.1f dB, %s\n", frequency, level,
               playing ? "playing" : "stopped");
    }
}
