#include "audio_backend.h"
#include <tinyalsa/pcm.h>
#include <cerrno>
#include <cstdio>


class TinyalsaBackend : public AudioBackend {
  public:
    ~TinyalsaBackend() override;
    bool open(const engine_config &cfg, unsigned int threshold_frames) override;
    bool prime(unsigned int frames) override;
    bool start() override { return true; }
    Result read(void *buffer, unsigned int frames) override;
    Result write(const void *buffer, unsigned int frames) override;
  private:
    Result recover(pcm *stream, const char *name);
    pcm *play_pcm = nullptr;
    pcm *cap_pcm  = nullptr;
    unsigned int play_bytes_per_frame = 0;
};


bool TinyalsaBackend::open(const engine_config &cfg,
                           unsigned int threshold_frames)
{
  this->play_bytes_per_frame = cfg.play_channels
                          * (pcm_format_to_bits(cfg.play_format) / 8);

  pcm_config play_config = {};
  play_config.channels        = cfg.play_channels;
  play_config.rate            = cfg.rate;
  play_config.period_size     = cfg.play_period_size;
  play_config.period_count    = cfg.play_period_count;
  play_config.format          = cfg.play_format;
  play_config.start_threshold = threshold_frames;

  this->play_pcm = pcm_open(cfg.play_card, cfg.play_device,
                      PCM_OUT | PCM_NORESTART, &play_config);
  if (!pcm_is_ready(this->play_pcm)) {
      fprintf(stderr, "FAIL: cannot open playback %u,%u: %s\n",
              cfg.play_card, cfg.play_device, pcm_get_error(this->play_pcm));
      return false;
  }

  pcm_config cap_config = {};
  cap_config.channels        = cfg.cap_channels;
  cap_config.rate            = cfg.rate;
  cap_config.period_size     = cfg.cap_period_size;
  cap_config.period_count    = cfg.cap_period_count;
  cap_config.format          = cfg.cap_format;
  cap_config.start_threshold = 1;

  this->cap_pcm = pcm_open(cfg.cap_card, cfg.cap_device,
                      PCM_IN | PCM_NORESTART, &cap_config);
  if (!pcm_is_ready(this->cap_pcm)) {
      fprintf(stderr, "FAIL: cannot open capture %u,%u: %s\n",
              cfg.cap_card, cfg.cap_device, pcm_get_error(this->cap_pcm));
      return false;
  }

  return true;
}

TinyalsaBackend::~TinyalsaBackend()
{
  if (this->cap_pcm && pcm_close(this->cap_pcm) < 0)
      fprintf(stderr, "FAIL: capture pcm_close returned an error\n");

  if (this->play_pcm && pcm_close(this->play_pcm) < 0)
      fprintf(stderr, "FAIL: playback pcm_close returned an error\n");
}

bool TinyalsaBackend::prime(unsigned int frames)
{
  unsigned char *silence =
      new unsigned char[frames * this->play_bytes_per_frame]();

  const int ret = pcm_writei(this->play_pcm, silence, frames);

  delete[] silence;

  if (ret < 0) {
      fprintf(stderr, "FAIL: priming write: %s\n", pcm_get_error(this->play_pcm));
      return false;
  }

  return true;
}

Result TinyalsaBackend::read(void *buffer, unsigned int frames)
{
  if (pcm_readi(cap_pcm, buffer, frames) < 0)
    return recover(cap_pcm, "capture");
  return BACKEND_OK;
}

Result TinyalsaBackend::write(const void *buffer, unsigned int frames)
{
  if (pcm_writei(play_pcm, buffer, frames) < 0)
    return recover(play_pcm, "playback");
  return BACKEND_OK;
}

Result TinyalsaBackend::recover(pcm *stream, const char *name)
{
  // EPIPE: the ring over- or underran. Anything else is a real failure
  if (errno != EPIPE) {
    fprintf(stderr, "\nFAIL: %s: %s\n", name, pcm_get_error(stream));
    return BACKEND_FAIL;
  } else if (pcm_prepare(stream) < 0) {
    fprintf(stderr, "\nFAIL: %s prepare: %s\n", name,
            pcm_get_error(stream));
    return BACKEND_FAIL;
  }
  return BACKEND_XRUN;
}

AudioBackend *create_backend() {
  return new TinyAlsaBackend();
}