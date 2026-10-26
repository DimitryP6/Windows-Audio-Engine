#ifndef CLI_H
#define CLI_H

#include "pcm_format.h

enum ParseResult {
    PARSE_OK,
    PARSE_HELP,
    PARSE_ERROR
};

ParseResult parse_args(int argc, char **argv,
                       unsigned int *play_card,
                       unsigned int *play_device,
                       unsigned int *cap_card,
                       unsigned int *cap_device,
                       unsigned int *srate,
                       unsigned int *render_block_size,
                       unsigned int *cap_period_size,
                       unsigned int *play_period_size,
                       unsigned int *cap_period_count,
                       unsigned int *play_period_count,
                       unsigned int *priming_frames,
                       unsigned int *cap_channels,
                       unsigned int *play_channels,
                       pcm_format   *cap_format,
                       pcm_format   *play_format);

#endif
