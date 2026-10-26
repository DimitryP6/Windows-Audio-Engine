#ifndef SINE_ENGINE_H
#define SINE_ENGINE_H

#include "engine.h"
#include "fifo.h"

// EDIT: complete the class with the methods and variables the app needs,
// and write the methods' bodies, here and in sine_engine.cpp
// Hint: AudioEngine in 0_control_fifo had all of them
class SineEngine : public AudioEngine {
public:
    // The control thread's way in: the message waits in the FIFO until
    // render() picks it up, at the start of the next block
    bool push_control(const ControlMessage &msg)
    {
        return control_fifo.push(msg);
    }

private:
    // override: the compiler checks that this really replaces AudioEngine's
    // render()
    void render(const AudioContext &ctx) override;

    // Only render() touches these, on the audio thread: plain floats
    float frequency = 440.0f;
    float amplitude = 0.0f;
    float phase     = 0.0f;

    ControlFifo control_fifo;

    // Still shared with the control thread and the signal handler
    std::atomic<bool> running = true;
};

#endif
