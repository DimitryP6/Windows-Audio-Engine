#ifndef PCM_FORMAT_H
#define PCM_FORMAT_H


#ifdef _WIN32
enum pcm_format {
  PCM_FORMAT_S16_LE,
  PCM_FORMAT_S24_3LE,
  PCM_FORMAT_S24_LE,
  PCM_FORMAT_S32_LE
};

/**
 * Given a PCM format enumeration, returns the amount of bits in the container.
 * @param format is the PCM format.
 * @return the amount of bits each sample occupies in memory from the ring buffer.
 * @return 16 as the default.
 */
static inline unsigned int pcm_format_to_bits(pcm_format format) {
  switch (format) {
    case PCM_FORMAT_S32_LE:
    case PCM_FORMAT_S24_LE:
      return 32;
    case PCM_FORMAT_S24_3LE:
      return 24;
    default:
    case PCM_FORMAT_S16_LE:
      return 16;
  }
}
#else
#include <tinyalsa/pcm.h>
#endif


#endif // PCM_FORMAT_H