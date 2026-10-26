#ifndef AUDIO_BACKEND_H
#define AUDIO_BACKEND_H


#include "pcm_format.h"


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

enum Result {
  BACKEND_OK,
  BACKEND_XRUN,
  BACKEND_FAIL
};


/**
 * This class represents an audio backend, which can initialize the device,
 * prime it, start it, read from it, and write to it.
 */
class AudioBackend {
  public:
    virtual ~AudioBackend() {};
    /**
     * Given the engine configuration and the threshold frames,
     * initializes the device for this backend.
     * @param cfg is the engine configuration.
     * @param threshold_frames is the amount of silent frames before playback.
     * @return whether the backend was initialized successfully.
     */
    virtual bool open(const engine_config &cfg, unsigned int threshold_frames) = 0;
    /**
     * Primes the backend device with the given amount of frames.
     * @param frames is the amount of frames to prime the playback buffer before it can play.
     * @return whether the backend device was primed successfully.
     */
    virtual bool prime(unsigned int frames) = 0;
    /**
     * Starts the backend device playback.
     * @return whether the backend device playback was started successfully.
     */
    virtual bool start() = 0;
    /**
     * Writes to the backend device.
     * @param buffer is the raw block that the engine will write from.
     * @param frames is the amount of frames to write.
     * @return a @code{Result} of the operation.
     */
    virtual Result write(const void *buffer, unsigned int frames) = 0;
    /**
     * Reads from the backend device.
     * @param buffer is the raw block that the engine will read into.
     * @param frames is the amount of frames to read.
     * @return a @code{Result} of the operation.
     */
    virtual Result read(void *buffer, unsigned int frames) = 0;
};

/**
 * Creates this backend.
 * @return an AudioBackend.
 */
AudioBackend *create_backend();


#endif // AUDIO_BACKEND_H