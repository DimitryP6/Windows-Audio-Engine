#include "audio_backend.h"
#include <windows.h> // HANDLE, DWORD, etc.
#include "mmdeviceapi.h" // Required by IMMDevice...
#include "audioclient.h" // Require by IAudioClient...


// KSDATAFORMAT_SUBTYPE_PCM for format configuration.
static const GUID pcm_subtype = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};


class WindowsBackend : AudioBackend {
  public:
    ~AudioBackend() override;
    bool open(const engine_config &cfg, unsigned int threshold_frames) override;
    bool prime(unsigned int frames) override;
    bool start() override;
    Result write(const void *buffer, unsigned int frames) override;
    Result read(void *buffer, unsigned int frames) override;
  private:
    /**
     * First are the field for the device enumerator, device, client, render client, and a handle.
     * https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream
     */
    // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nn-mmdeviceapi-immdeviceenumerator
    IMMDeviceEnumerator *enumerator = nullptr;
    // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nn-mmdeviceapi-immdevice
    IMMDevice *device = nullptr;
    // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nn-audioclient-iaudioclient
    IAudioClient *client = nullptr;
    // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nn-audioclient-iaudiorenderclient
    IAudioRenderClient *renderer = nullptr;
    // WINDOWS calls return an opaque handle object.
    HANDLE event = nullptr;
    // In WASAPI, nothing blocks, so keep track of buffer frames available to write to.
    UINT32 buffer_frames = 0;
    // TinyAlsa knows the format, and writes for us. WASAPI hands a device pointer, so have to set ourselves.
    unsigned int play_bytes_per_frame = 0;
    unsigned int cap_bytes_per_frame  = 0;
    bool com_ready = false;
    bool started   = false;
};

WasapiBackend::~WasapiBackend() {
  if (this->started)
    this->client->Stop();
  if (this->render) 
    this->render->Release();
  if (this->client)
    this->client->Release();
  if (this->device)
    this->device->Release();
  if (this->enumerator)
    this->enumerator->Release();
  if (this->event)
    CloseHandle(this->event);
  // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-couninitialize
  if (this->com_ready)
    CoUninitialize();
}

// https://learn.microsoft.com/en-us/windows/win32/com/error-handling-in-com
bool WindowsBackend::check(HRESULT hr, const char *what)
{
  if (SUCCEEDED(hr))
    return true;
  // https://learn.microsoft.com/en-us/office/client-developer/outlook/mapi/hresult
  fprintf(stderr, "FAIL: %s: HRESULT 0x%08lx\n", what,
          static_cast<unsigned long>(hr));
  return false;
}

// Called on the audio thread, from the engine's run.
bool WindowsBackend::open(const engine_config &cfg, unsigned int threshold_frames) {
  // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex
  if (!this->check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "CoInitializeEx"))
    // https://learn.microsoft.com/en-us/windows/win32/api/objbase/ne-objbase-coinit
    return false;
  this->com_ready = true;
  // Where this pattern is from: https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream
  // CoCreateInstance docs: // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cocreateinstance
  if (!this->check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                    CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                    reinterpret_cast<void **>(this.enumerator)),
                                    "create device enumerator"))
    return false;
  // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdeviceenumerator-getdefaultaudioendpoint
  if (!this->check(this->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, this.device), 
  "get default playback device"))
    return false;
  // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdevice-activate
  if (!this->check(this->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(this.client)),
  "activate audio client"))
    return false;
  // https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex

  WAVEFORMATEX *pwfx = nullptr;
  if (SUCCEEDED(this->client->GetMixFormat(&pwfx))) {
    // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getmixformat
    printf("WASAPI mix: %u samples per second, %u channels, and %u bits per sample.\n", 
            pwfx->nSamplesPerSec, pwfx->nChannels, pwfx->wBitsPerSample);
    // Need to free wave format structure: https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cotaskmemfree
    CoTaskMemFree(pwfx);
  }
  const unsigned int container_bits = pcm_format_to_bits(cfg.play_format);
  unsigned int valid_bits = container_bits;
  if (cfg.play_format == PCM_FORMAT_S24_LE) 
    valid_bits = 24;
  // Need to set an extensible wave format: https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatextensible
  WAVEFORMATEXTENSIBLE format = {}
  format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format.Format.nChannels = cfg.play_channels;
  format.Format.nSamplesPerSec = cfg.rate;
  format.Format.wBitsPerSample = container_bits;
  format.Format.nBlockAlign = cfg.play_channels * container_bits / 8;
  format.Format.nAvgBytesPerSec = cfg.rate * format.Format.nBlockAlign;
  format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  format.Samples.wValidBitsPerSample = valid_bits;
  format.SubFormat = pcm_subtype;
  // Set the channel mask: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-ksaudio_channel_config
  if (cfg.play_channels == 1)
    format.dwChannelMask = SPEAKER_FRONT_CENTER;
  else if (cfg.play_channels == 2)
    format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  this->play_bytes_per_frame = format.Format.nBlockAlign;
  this->cap_bytes_per_frame = cfg.cap_channels * (pcm_format_to_bits(cfg.cap_format) / 8);

  // Set auto conversion, event-driven, and quality enhancement flags:
  // https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants
  DWORD flags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 
                || AUDCLNT_STREAMFLAGS_EVENTCALLBACK 
                || AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
  // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize
  if (!this->check(this->client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0,
                                  &format.Format, nullptr),
                    "initialize shared stream"))
    return false;
  // Get the number of audio frames that the buffer can hold.
  // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getbuffersize
  if (!this->check(this->client->GetBufferSize(&buffer_frames), "get buffer size"))
    return false;
  
  // Create an event: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventw
  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!event) {
    fprintf(stderr, "FAIL: CreateEvent: %lu\n", GetLastError());
    return false;
  }
  // Set event handle: https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-seteventhandle
  if (!check(client->SetEventHandle(event), "set event handle"))
    return false;
  // Start the render service: https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getservice
  if (!check(client->GetService(__uuidof(IAudioRenderClient),
                                  reinterpret_cast<void **>(this.renderer)),
              "get render client"))
    return false;
  return true;
}