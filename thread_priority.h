#ifndef THREAD_PRIORITY_H
#define THREAD_PRIORITY_H


#include <cstring>
#include <cstdio>
static inline void* raise_audio_priority();
static inline void restore_audio_priority(void* optional_handle);
static inline void raise_control_priority();


#ifdef _WIN32


#include <windows.h>
#include <avrt.h>
// General documentation: https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service


static inline void* raise_audio_priority() {
  DWORD task_index = 0;
  // https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avsetmmthreadcharacteristicsw
  HANDLE handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
  if (handle == NULL) {
    fprintf(stderr,"Failed to set MM thread characteristics. Error: %lu\n", GetLastError());
    return nullptr;
  }
  return handle;
}

static inline void restore_audio_priority(void* optional_handle) {
  if (optional_handle == NULL) {
    return;
  // https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avrevertmmthreadcharacteristics
  } else if (!AvRevertMmThreadCharacteristics(optional_handle)) {
    fprintf(stderr, "Failed to revert MM thread characteristics. Error: %lu\n", GetLastError());
  }
}

static inline void raise_control_priority() {
  // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
  // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthread
  if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
    fprintf(stderr, "WARNING: cannot set control thread priority. Error: %lu\n",
            GetLastError());
  }
}


#else


#include <pthread.h>


static inline void* raise_audio_priority() {
  sched_param param = {};
    param.sched_priority = 99;
    const int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0)
      fprintf(stderr, "WARNING: cannot set audio thread priority: %s\n",
              strerror(ret));
    return nullptr;
}
static inline void restore_audio_priority(void* optional_handle) {
  return;
}

static inline void raise_control_priority() {
  sched_param param = {};
    param.sched_priority = 90;
    const int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0)
        fprintf(stderr, "WARNING: cannot set control thread priority: %s\n",
                strerror(ret));
}


#endif


#endif // THREAD_PRIORITY_H