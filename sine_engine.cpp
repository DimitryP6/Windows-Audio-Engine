#include "sine_engine.h"

#include <cmath>

void SineEngine::render(const AudioContext &ctx)
{
    // Empty the FIFO and keep only the newest message: each message carries
    // the whole state, so the older ones are already out of date
    ControlMessage msg      = {};
    bool           received = false;

    while (control_fifo.pop(msg))
        received = true;

    if (received) {
        frequency = msg.frequency;
        amplitude = msg.amplitude;

        if (msg.phase >= 0.0f)
            phase = msg.phase;
    }

    constexpr float two_pi = 2.0f * M_PI;

    const float phase_step = two_pi * frequency / static_cast<float>(ctx.rate);

    for (unsigned int n = 0; n < ctx.block_size; n++) {
        const float sample = amplitude * std::sin(phase);

        for (unsigned int c = 0; c < ctx.out_channels; c++)
            ctx.block_out[n * ctx.out_channels + c] = sample;

        phase += phase_step;
        if (phase >= two_pi)
            phase -= two_pi;
    }
}
