/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#define __MSVCRT_VERSION__ 0x0700
#define WINVER 0x0501
#define WIN32_LEAN_AND_MEAN
#include <malloc.h>
#include <assert.h>
#include <windows.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <process.h>
#include <stdlib.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"

/* This is missing from the MinGW headers. Use a safe fallback. */
#if !defined(MEMORY_ALLOCATION_ALIGNMENT)
#define MEMORY_ALLOCATION_ALIGNMENT 16
#endif

#define CUBEB_STREAM_MAX 32
#define NBUFS 4

const GUID KSDATAFORMAT_SUBTYPE_PCM =
{ 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID KSDATAFORMAT_SUBTYPE_IEEE_FLOAT =
{ 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

struct cubeb_stream_item {
  SLIST_ENTRY head;
  cubeb_stream * stream;
};

static struct cubeb_ops const winmm_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  HANDLE event;
  HANDLE thread;
  int shutdown;
  PSLIST_HEADER work;
  CRITICAL_SECTION lock;
  unsigned int active_streams;
  unsigned int minimum_latency;
};

struct cubeb_stream {
  cubeb * context;
  cubeb_stream_params params;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * user_ptr;
  WAVEHDR buffers[NBUFS];
  size_t buffer_size;
  int next_buffer;
  int free_buffers;
  int shutdown;
  int draining;
  HANDLE event;
  HWAVEOUT waveout;
  CRITICAL_SECTION lock;
  uint64_t written;
};

static size_t
bytes_per_frame(cubeb_stream_params params)
{
  size_t bytes;

  switch (params.format) {
  case CUBEB_SAMPLE_S16LE:
    bytes = sizeof(signed short);
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    bytes = sizeof(float);
    break;
  default:
    assert(0);
  }

  return bytes * params.channels;
}

static WAVEHDR *
winmm_get_next_buffer(cubeb_stream * stm)
{
  WAVEHDR * hdr = NULL;

  assert(stm->free_buffers > 0 && stm->free_buffers <= NBUFS);
  hdr = &stm->buffers[stm->next_buffer];
  assert(hdr->dwFlags & WHDR_PREPARED ||
         (hdr->dwFlags & WHDR_DONE && !(hdr->dwFlags & WHDR_INQUEUE)));
  stm->next_buffer = (stm->next_buffer + 1) % NBUFS;
  stm->free_buffers -= 1;

  return hdr;
}

static void
winmm_refill_stream(cubeb_stream * stm)
{
  WAVEHDR * hdr;
  long got;
  long wanted;
  MMRESULT r;

  EnterCriticalSection(&stm->lock);
  stm->free_buffers += 1;
  assert(stm->free_buffers > 0 && stm->free_buffers <= NBUFS);

  if (stm->draining) {
    LeaveCriticalSection(&stm->lock);
    if (stm->free_buffers == NBUFS) {
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
    }
    SetEvent(stm->event);
    return;
  }

  if (stm->shutdown) {
    LeaveCriticalSection(&stm->lock);
    SetEvent(stm->event);
    return;
  }

  hdr = winmm_get_next_buffer(stm);

  wanted = (DWORD) stm->buffer_size / bytes_per_frame(stm->params);

  /* It is assumed that the caller is holding this lock.  It must be dropped
     during the callback to avoid deadlocks. */
  LeaveCriticalSection(&stm->lock);
  got = stm->data_callback(stm, stm->user_ptr, hdr->lpData, wanted);
  EnterCriticalSection(&stm->lock);
  if (got < 0) {
    LeaveCriticalSection(&stm->lock);
    /* XXX handle this case */
    assert(0);
    return;
  } else if (got < wanted) {
    stm->draining = 1;
  }
  stm->written += got;

  assert(hdr->dwFlags & WHDR_PREPARED);

  hdr->dwBufferLength = got * bytes_per_frame(stm->params);
  assert(hdr->dwBufferLength <= stm->buffer_size);

  r = waveOutWrite(stm->waveout, hdr, sizeof(*hdr));
  if (r != MMSYSERR_NOERROR) {
    LeaveCriticalSection(&stm->lock);
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
    return;
  }

  LeaveCriticalSection(&stm->lock);
}

static unsigned __stdcall
winmm_buffer_thread(void * user_ptr)
{
  cubeb * ctx = (cubeb *) user_ptr;
  assert(ctx);

  for (;;) {
    DWORD rv;
    PSLIST_ENTRY item;

    rv = WaitForSingleObject(ctx->event, INFINITE);
    assert(rv == WAIT_OBJECT_0);

    /* Process work items in batches so that a single stream can't
       starve the others by continuously adding new work to the top of
       the work item stack. */
    item = InterlockedFlushSList(ctx->work);
    while (item != NULL) {
      PSLIST_ENTRY tmp = item;
      winmm_refill_stream(((struct cubeb_stream_item *) tmp)->stream);
      item = item->Next;
      _aligned_free(tmp);
    }

    if (ctx->shutdown) {
      break;
    }
  }

  return 0;
}

static void CALLBACK
winmm_buffer_callback(HWAVEOUT waveout, UINT msg, DWORD_PTR user_ptr, DWORD_PTR p1, DWORD_PTR p2)
{
  cubeb_stream * stm = (cubeb_stream *) user_ptr;
  struct cubeb_stream_item * item;

  if (msg != WOM_DONE) {
    return;
  }

  item = _aligned_malloc(sizeof(struct cubeb_stream_item), MEMORY_ALLOCATION_ALIGNMENT);
  assert(item);
  item->stream = stm;
  InterlockedPushEntrySList(stm->context->work, &item->head);

  SetEvent(stm->context->event);
}

static unsigned int
calculate_minimum_latency(void)
{
  OSVERSIONINFOEX osvi;
  DWORDLONG mask;

  /* Running under Terminal Services results in underruns with low latency. */
  if (GetSystemMetrics(SM_REMOTESESSION) == TRUE) {
    return 500;
  }

  /* Vista's WinMM implementation underruns when less than 200ms of audio is buffered. */
  memset(&osvi, 0, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  osvi.dwMajorVersion = 6;
  osvi.dwMinorVersion = 0;

  mask = 0;
  VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_EQUAL);
  VER_SET_CONDITION(mask, VER_MINORVERSION, VER_EQUAL);

  if (VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, mask) != 0) {
    return 200;
  }

  return 100;
}

static void winmm_destroy(cubeb * ctx);

/*static*/ int
winmm_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;

  assert(context);
  *context = NULL;

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &winmm_ops;

  ctx->work = _aligned_malloc(sizeof(*ctx->work), MEMORY_ALLOCATION_ALIGNMENT);
  assert(ctx->work);
  InitializeSListHead(ctx->work);

  ctx->event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!ctx->event) {
    winmm_destroy(ctx);
    return CUBEB_ERROR;
  }

  ctx->thread = (HANDLE) _beginthreadex(NULL, 64 * 1024, winmm_buffer_thread, ctx, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
  if (!ctx->thread) {
    winmm_destroy(ctx);
    return CUBEB_ERROR;
  }

  SetThreadPriority(ctx->thread, THREAD_PRIORITY_TIME_CRITICAL);

  InitializeCriticalSection(&ctx->lock);
  ctx->active_streams = 0;

  ctx->minimum_latency = calculate_minimum_latency();

  *context = ctx;

  return CUBEB_OK;
}

static char const *
winmm_get_backend_id(cubeb * ctx)
{
  return "winmm";
}

static void
winmm_destroy(cubeb * ctx)
{
  DWORD rv;

  assert(ctx->active_streams == 0);
  assert(!InterlockedPopEntrySList(ctx->work));

  DeleteCriticalSection(&ctx->lock);

  if (ctx->thread) {
    ctx->shutdown = 1;
    SetEvent(ctx->event);
    rv = WaitForSingleObject(ctx->thread, INFINITE);
    assert(rv == WAIT_OBJECT_0);
    CloseHandle(ctx->thread);
  }

  if (ctx->event) {
    CloseHandle(ctx->event);
  }

  _aligned_free(ctx->work);

  free(ctx);
}

static void winmm_stream_destroy(cubeb_stream * stm);

static int
winmm_stream_init(cubeb * context, cubeb_stream ** stream, char const * stream_name,
                  cubeb_stream_params stream_params, unsigned int latency,
                  cubeb_data_callback data_callback,
                  cubeb_state_callback state_callback,
                  void * user_ptr)
{
  MMRESULT r;
  WAVEFORMATEXTENSIBLE wfx;
  cubeb_stream * stm;
  int i;
  size_t bufsz;

  assert(context);
  assert(stream);

  *stream = NULL;

  memset(&wfx, 0, sizeof(wfx));
  if (stream_params.channels > 2) {
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.cbSize = sizeof(wfx) - sizeof(wfx.Format);
  } else {
    wfx.Format.wFormatTag = WAVE_FORMAT_PCM;
    if (stream_params.format == CUBEB_SAMPLE_FLOAT32LE) {
      wfx.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    }
    wfx.Format.cbSize = 0;
  }
  wfx.Format.nChannels = stream_params.channels;
  wfx.Format.nSamplesPerSec = stream_params.rate;

  /* XXX fix channel mappings */
  wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;

  switch (stream_params.format) {
  case CUBEB_SAMPLE_S16LE:
    wfx.Format.wBitsPerSample = 16;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    wfx.Format.wBitsPerSample = 32;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  wfx.Format.nBlockAlign = (wfx.Format.wBitsPerSample * wfx.Format.nChannels) / 8;
  wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
  wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;

  EnterCriticalSection(&context->lock);
  /* CUBEB_STREAM_MAX is a horrible hack to avoid a situation where, when
     many streams are active at once, a subset of them will not consume (via
     playback) or release (via waveOutReset) their buffers. */
  if (context->active_streams >= CUBEB_STREAM_MAX) {
    LeaveCriticalSection(&context->lock);
    return CUBEB_ERROR;
  }
  context->active_streams += 1;
  LeaveCriticalSection(&context->lock);

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = context;

  stm->params = stream_params;

  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->written = 0;

  if (latency < context->minimum_latency) {
    latency = context->minimum_latency;
  }

  bufsz = (size_t) (stm->params.rate / 1000.0 * latency * bytes_per_frame(stm->params) / NBUFS);
  if (bufsz % bytes_per_frame(stm->params) != 0) {
    bufsz += bytes_per_frame(stm->params) - (bufsz % bytes_per_frame(stm->params));
  }
  assert(bufsz % bytes_per_frame(stm->params) == 0);

  stm->buffer_size = bufsz;

  InitializeCriticalSection(&stm->lock);

  stm->event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!stm->event) {
    winmm_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  /* winmm_buffer_callback will be called during waveOutOpen, so all
     other initialization must be complete before calling it. */
  r = waveOutOpen(&stm->waveout, WAVE_MAPPER, &wfx.Format,
                  (DWORD_PTR) winmm_buffer_callback, (DWORD_PTR) stm,
                  CALLBACK_FUNCTION);
  if (r != MMSYSERR_NOERROR) {
    winmm_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  r = waveOutPause(stm->waveout);
  if (r != MMSYSERR_NOERROR) {
    winmm_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  for (i = 0; i < NBUFS; ++i) {
    WAVEHDR * hdr = &stm->buffers[i];

    hdr->lpData = calloc(1, bufsz);
    assert(hdr->lpData);
    hdr->dwBufferLength = bufsz;
    hdr->dwFlags = 0;

    r = waveOutPrepareHeader(stm->waveout, hdr, sizeof(*hdr));
    if (r != MMSYSERR_NOERROR) {
      winmm_stream_destroy(stm);
      return CUBEB_ERROR;
    }

    winmm_refill_stream(stm);
  }

  *stream = stm;

  return CUBEB_OK;
}

static void
winmm_stream_destroy(cubeb_stream * stm)
{
  DWORD rv;
  int i;
  int enqueued;

  if (stm->waveout) {
    EnterCriticalSection(&stm->lock);
    stm->shutdown = 1;

    waveOutReset(stm->waveout);

    enqueued = NBUFS - stm->free_buffers;
    LeaveCriticalSection(&stm->lock);

    /* Wait for all blocks to complete. */
    while (enqueued > 0) {
      rv = WaitForSingleObject(stm->event, INFINITE);
      assert(rv == WAIT_OBJECT_0);

      EnterCriticalSection(&stm->lock);
      enqueued = NBUFS - stm->free_buffers;
      LeaveCriticalSection(&stm->lock);
    }

    EnterCriticalSection(&stm->lock);

    for (i = 0; i < NBUFS; ++i) {
      if (stm->buffers[i].dwFlags & WHDR_PREPARED) {
        waveOutUnprepareHeader(stm->waveout, &stm->buffers[i], sizeof(stm->buffers[i]));
      }
    }

    waveOutClose(stm->waveout);

    LeaveCriticalSection(&stm->lock);
  }

  if (stm->event) {
    CloseHandle(stm->event);
  }

  DeleteCriticalSection(&stm->lock);

  for (i = 0; i < NBUFS; ++i) {
    free(stm->buffers[i].lpData);
  }

  EnterCriticalSection(&stm->context->lock);
  assert(stm->context->active_streams >= 1);
  stm->context->active_streams -= 1;
  LeaveCriticalSection(&stm->context->lock);

  free(stm);
}

static int
winmm_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  assert(ctx && max_channels);

  /* We don't support more than two channels in this backend. */
  *max_channels = 2;

  return CUBEB_OK;
}

static int
winmm_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency)
{
  // 100ms minimum, if we are not in a bizarre configuration.
  *latency = ctx->minimum_latency;

  return CUBEB_OK;
}

static int
winmm_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  WAVEOUTCAPS woc;
  MMRESULT r;

  r = waveOutGetDevCaps(WAVE_MAPPER, &woc, sizeof(WAVEOUTCAPS));
  if (r != MMSYSERR_NOERROR) {
    return CUBEB_ERROR;
  }

  /* Check if we support 48kHz, but not 44.1kHz. */
  if (!(woc.dwFormats & WAVE_FORMAT_4S16) &&
      woc.dwFormats & WAVE_FORMAT_48S16) {
    *rate = 48000;
    return CUBEB_OK;
  }
  /* Prefer 44.1kHz between 44.1kHz and 48kHz. */
  *rate = 44100;

  return CUBEB_OK;
}

static int
winmm_stream_start(cubeb_stream * stm)
{
  MMRESULT r;

  EnterCriticalSection(&stm->lock);
  r = waveOutRestart(stm->waveout);
  LeaveCriticalSection(&stm->lock);

  if (r != MMSYSERR_NOERROR) {
    return CUBEB_ERROR;
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);

  return CUBEB_OK;
}

static int
winmm_stream_stop(cubeb_stream * stm)
{
  MMRESULT r;

  EnterCriticalSection(&stm->lock);
  r = waveOutPause(stm->waveout);
  LeaveCriticalSection(&stm->lock);

  if (r != MMSYSERR_NOERROR) {
    return CUBEB_ERROR;
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);

  return CUBEB_OK;
}

static int
winmm_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  MMRESULT r;
  MMTIME time;

  EnterCriticalSection(&stm->lock);
  time.wType = TIME_SAMPLES;
  r = waveOutGetPosition(stm->waveout, &time, sizeof(time));
  LeaveCriticalSection(&stm->lock);

  if (r != MMSYSERR_NOERROR || time.wType != TIME_SAMPLES) {
    return CUBEB_ERROR;
  }

  *position = time.u.sample;

  return CUBEB_OK;
}

static int
winmm_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  MMRESULT r;
  MMTIME time;
  uint64_t written;

  EnterCriticalSection(&stm->lock);
  time.wType = TIME_SAMPLES;
  r = waveOutGetPosition(stm->waveout, &time, sizeof(time));
  written = stm->written;
  LeaveCriticalSection(&stm->lock);

  *latency = written - time.u.sample;

  return CUBEB_OK;
}

static struct cubeb_ops const winmm_ops = {
  /*.init =*/ winmm_init,
  /*.get_backend_id =*/ winmm_get_backend_id,
  /*.get_max_channel_count=*/ winmm_get_max_channel_count,
  /*.get_min_latency=*/ winmm_get_min_latency,
  /*.get_preferred_sample_rate =*/ winmm_get_preferred_sample_rate,
  /*.destroy =*/ winmm_destroy,
  /*.stream_init =*/ winmm_stream_init,
  /*.stream_destroy =*/ winmm_stream_destroy,
  /*.stream_start =*/ winmm_stream_start,
  /*.stream_stop =*/ winmm_stream_stop,
  /*.stream_get_position =*/ winmm_stream_get_position,
  /*.stream_get_latency = */ winmm_stream_get_latency
};
