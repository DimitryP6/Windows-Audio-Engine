#include "audio_backend.h"

class TinyAlsaBackend: public AudioBackend {
  public: 
    bool open(const engine_config &cfg, unsigned int threshold_frames) override {
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
          return false;
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
          return false;
      }
      return true;
    }
    bool prime(unsigned int frames) override {
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
    virtual bool start() = 0;
    virtual Result write(const void *buffer, unsigned int frames) = 0;
    virtual Result read(void *buffer, unsigned int frames) = 0;
};

AudioBackend *create_backend() {

}