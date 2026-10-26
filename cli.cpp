#include "cli.h"

#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>

struct FormatSpec {
    int          spec;
    pcm_format   pcm;
    const char  *name;
};

static const FormatSpec formats[] = {
    {  2, PCM_FORMAT_S16_LE,  "S16_LE"  },
    {  3, PCM_FORMAT_S24_3LE, "S24_3LE" },
    { -3, PCM_FORMAT_S24_LE,  "S24_LE"  },
    {  4, PCM_FORMAT_S32_LE,  "S32_LE"  },
};

constexpr unsigned int num_formats = sizeof(formats) / sizeof(formats[0]);

static const FormatSpec *lookup_spec(int spec)
{
    for (unsigned int i = 0; i < num_formats; i++)
        if (formats[i].spec == spec)
            return &formats[i];

    return nullptr;
}

static const FormatSpec *lookup_format(pcm_format format)
{
    for (unsigned int i = 0; i < num_formats; i++)
        if (formats[i].pcm == format)
            return &formats[i];

    return nullptr;
}

static void print_usage(const char *prog,
                        unsigned int def_play_card,
                        unsigned int def_play_device,
                        unsigned int def_cap_card,
                        unsigned int def_cap_device,
                        unsigned int def_srate,
                        unsigned int def_block,
                        unsigned int def_cap_period,
                        unsigned int def_play_period,
                        unsigned int def_cap_count,
                        unsigned int def_play_count,
                        unsigned int def_cap_channels,
                        unsigned int def_play_channels,
                        pcm_format def_cap_format,
                        pcm_format def_play_format)
{
    printf("usage: %s [options]\n\n", prog);

    printf("  -o, --playback=CARD,DEVICE  playback card and device  [%u,%u]\n",
           def_play_card, def_play_device);
    printf("  -i, --capture=CARD,DEVICE   capture card and device   [%u,%u]\n",
           def_cap_card, def_cap_device);
    printf("  -b, --block=FRAMES          render block size         [%u]\n",
           def_block);
    printf("  -p, --period=CAP,PLAY       period size per stream    [%u,%u]\n",
           def_cap_period, def_play_period);
    printf("  -n, --periods=CAP,PLAY      period count per stream   [%u,%u]\n",
           def_cap_count, def_play_count);
    printf("  -r, --rate=HZ               sample rate in Hz         [%u]\n",
           def_srate);
    printf("  -t, --priming=FRAMES        priming, in frames        [auto]\n");
    printf("  -f, --format=CAP,PLAY       sample format per stream  [%d,%d]\n",
           lookup_format(def_cap_format)->spec,
           lookup_format(def_play_format)->spec);
    printf("  -c, --channels=CAP,PLAY     channels per stream       [%u,%u]\n",
           def_cap_channels, def_play_channels);
    printf("  -h, --help                  show this help and exit\n\n");

    printf("  -t defaults to capture period + render block, plus one\n");
    printf("  render block of margin. It cannot go below that floor, or\n");
    printf("  above the playback ring\n\n");

    printf("  SPEC   format\n");
    printf("  ----   ------\n");

    for (unsigned int i = 0; i < num_formats; i++)
        printf("  %4d   %s\n", formats[i].spec, formats[i].name);
}

static bool to_long(const char *s, long *out)
{
    char *end = nullptr;

    *out = strtol(s, &end, 10);

    return end != s && *end == '\0';
}

static bool opt_uint(const char *arg, const char *what, unsigned int *out)
{
    long value = 0;

    if (!to_long(arg, &value) || value <= 0 || value > UINT_MAX) {
        fprintf(stderr, "FAIL: bad %s '%s' (must be a positive number)\n\n",
                what, arg);
        return false;
    }

    *out = static_cast<unsigned int>(value);

    return true;
}

static bool opt_pair(const char *arg, const char *what,
                            unsigned int *first, unsigned int *second)
{
    const char *comma    = strchr(arg, ',');
    char       *first_end = nullptr;
    char       *second_end  = nullptr;

    if (comma) {
        const long c = strtol(arg, &first_end, 10);
        const long d = strtol(comma + 1, &second_end, 10);

        if (first_end == comma && second_end != comma + 1 && *second_end == '\0' &&
            c >= 0 && c <= UINT_MAX && d >= 0 && d <= UINT_MAX) {
            *first  = static_cast<unsigned int>(c);
            *second = static_cast<unsigned int>(d);
            return true;
        }
    }

    fprintf(stderr, "FAIL: bad %s '%s' (expected A,B)\n\n", what, arg);

    return false;
}

static bool opt_format_pair(const char *arg,
                            pcm_format *first, pcm_format *second)
{
    const char       *comma = strchr(arg, ',');
    char             *end   = nullptr;
    const FormatSpec *f     = nullptr;
    const FormatSpec *s     = nullptr;

    if (comma) {
        const long a = strtol(arg, &end, 10);

        if (end == comma)
            f = lookup_spec(static_cast<int>(a));

        const long b = strtol(comma + 1, &end, 10);

        if (end != comma + 1 && *end == '\0')
            s = lookup_spec(static_cast<int>(b));
    }

    if (!f || !s) {
        fprintf(stderr, "FAIL: bad format '%s' (expected CAP,PLAY)\n\n", arg);
        return false;
    }

    *first  = f->pcm;
    *second = s->pcm;

    return true;
}

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
                       pcm_format   *play_format)
{
    static const struct optparse_long long_options[] = {
        { "playback", 'o', OPTPARSE_REQUIRED },
        { "capture",  'i', OPTPARSE_REQUIRED },
        { "block",    'b', OPTPARSE_REQUIRED },
        { "priming",  't', OPTPARSE_REQUIRED },
        { "rate",     'r', OPTPARSE_REQUIRED },
        { "period",   'p', OPTPARSE_REQUIRED },
        { "periods",  'n', OPTPARSE_REQUIRED },
        { "format",   'f', OPTPARSE_REQUIRED },
        { "channels", 'c', OPTPARSE_REQUIRED },
        { "help",     'h', OPTPARSE_NONE     },
        { nullptr,     0,  OPTPARSE_NONE     },
    };

    const unsigned int def_play_card   = *play_card;
    const unsigned int def_play_device = *play_device;
    const unsigned int def_cap_card    = *cap_card;
    const unsigned int def_cap_device  = *cap_device;

    const unsigned int def_srate       = *srate;
    const unsigned int def_block       = *render_block_size;
    const unsigned int def_cap_period  = *cap_period_size;
    const unsigned int def_play_period = *play_period_size;
    const unsigned int def_cap_count   = *cap_period_count;
    const unsigned int def_play_count  = *play_period_count;
    const unsigned int def_cap_channels  = *cap_channels;
    const unsigned int def_play_channels = *play_channels;
    const pcm_format   def_cap_format    = *cap_format;
    const pcm_format   def_play_format   = *play_format;

    if (!lookup_format(def_cap_format) || !lookup_format(def_play_format)) {
        fprintf(stderr, "FAIL: default format is not one of the supported ones\n");
        return PARSE_ERROR;
    }

    (void)argc;

    struct optparse opts;
    optparse_init(&opts, argv);

    bool ok = true;
    int  c;

    while (ok && (c = optparse_long(&opts, long_options, nullptr)) != -1) {
        switch (c) {
        case 'o':
            ok = opt_pair(opts.optarg, "playback device",
                                 play_card, play_device);
            break;
        case 'i':
            ok = opt_pair(opts.optarg, "capture device",
                                 cap_card, cap_device);
            break;
        case 'r':
            ok = opt_uint(opts.optarg, "sample rate", srate);
            break;
        case 'b':
            ok = opt_uint(opts.optarg, "render block size", render_block_size);
            break;
        case 't':
            ok = opt_uint(opts.optarg, "priming", priming_frames);
            break;
        case 'p':
            ok = opt_pair(opts.optarg, "period size",
                          cap_period_size, play_period_size);
            break;
        case 'n':
            ok = opt_pair(opts.optarg, "period count",
                          cap_period_count, play_period_count);
            break;
        case 'f':
            ok = opt_format_pair(opts.optarg, cap_format, play_format);
            break;
        case 'c':
            ok = opt_pair(opts.optarg, "channel count",
                          cap_channels, play_channels);
            break;
        case 'h':
            print_usage(argv[0], def_play_card, def_play_device,
                        def_cap_card, def_cap_device, def_srate, def_block,
                        def_cap_period, def_play_period, def_cap_count,
                        def_play_count, def_cap_channels,
                        def_play_channels, def_cap_format, def_play_format);
            return PARSE_HELP;
        default:
            fprintf(stderr, "FAIL: %s\n\n", opts.errmsg);
            ok = false;
            break;
        }
    }

    const char *extra = ok ? optparse_arg(&opts) : nullptr;

    if (extra) {
        fprintf(stderr, "FAIL: unexpected argument '%s'\n\n", extra);
        ok = false;
    }

    if (!ok) {
        print_usage(argv[0], def_play_card, def_play_device,
                    def_cap_card, def_cap_device, def_srate, def_block,
                    def_cap_period, def_play_period, def_cap_count,
                    def_play_count, def_cap_channels,
                    def_play_channels, def_cap_format, def_play_format);
        return PARSE_ERROR;
    }

    return PARSE_OK;
}
