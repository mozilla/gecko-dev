/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_RENDERTHREAD_H
#define MOZILLA_LAYERS_RENDERTHREAD_H

#include "base/basictypes.h"            // for DISALLOW_EVIL_CONSTRUCTORS
#include "base/platform_thread.h"       // for PlatformThreadId
#include "base/thread.h"                // for Thread
#include "base/message_loop.h"
#include "nsISupportsImpl.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Mutex.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/SynchronousTask.h"
#include "GLContext.h"

#include <list>
#include <queue>
#include <unordered_map>

namespace mozilla {
namespace wr {

typedef MozPromise<MemoryReport, bool, true> MemoryReportPromise;

class RendererOGL;
class RenderTextureHost;
class RenderThread;

/// A rayon thread pool that is shared by all WebRender instances within a process.
class WebRenderThreadPool {
public:
  WebRenderThreadPool();

  ~WebRenderThreadPool();

  wr::WrThreadPool* Raw() { return mThreadPool; }

protected:
  wr::WrThreadPool* mThreadPool;
};

class WebRenderProgramCache {
public:
  explicit WebRenderProgramCache(wr::WrThreadPool* aThreadPool);

  ~WebRenderProgramCache();

  wr::WrProgramCache* Raw() { return mProgramCache; }

protected:
  wr::WrProgramCache* mProgramCache;
};

class WebRenderShaders {
public:
  WebRenderShaders(gl::GLContext* gl, WebRenderProgramCache* programCache);
  ~WebRenderShaders();

  wr::WrShaders* RawShaders() { return mShaders; }

protected:
  RefPtr<gl::GLContext> mGL;
  wr::WrShaders* mShaders;
};

/// Base class for an event that can be scheduled to run on the render thread.
///
/// The event can be passed through the same channels as regular WebRender messages
/// to preserve ordering.
class RendererEvent
{
public:
  virtual ~RendererEvent() {}
  virtual void Run(RenderThread& aRenderThread, wr::WindowId aWindow) = 0;
};

/// The render thread is where WebRender issues all of its GPU work, and as much
/// as possible this thread should only serve this purpose.
///
/// The render thread owns the different RendererOGLs (one per window) and implements
/// the RenderNotifier api exposed by the WebRender bindings.
///
/// We should generally avoid posting tasks to the render thread's event loop directly
/// and instead use the RendererEvent mechanism which avoids races between the events
/// and WebRender's own messages.
///
/// The GL context(s) should be created and used on this thread only.
/// XXX - I've tried to organize code so that we can potentially avoid making
/// this a singleton since this bad habit has a tendency to bite us later, but
/// I haven't gotten all the way there either, in order to focus on the more
/// important pieces first. So we are a bit in-between (this is totally a singleton
/// but in some places we pretend it's not). Hopefully we can evolve this in a way
/// that keeps the door open to removing the singleton bits.
class RenderThread final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(RenderThread)

public:
  /// Can be called from any thread.
  static RenderThread* Get();

  /// Can only be called from the main thread.
  static void Start();

  /// Can only be called from the main thread.
  static void ShutDown();

  /// Can be called from any thread.
  /// In most cases it is best to post RendererEvents through WebRenderAPI instead
  /// of scheduling directly to this message loop (so as to preserve the ordering
  /// of the messages).
  static MessageLoop* Loop();

  /// Can be called from any thread.
  static bool IsInRenderThread();

  // Can be called from any thread. Dispatches an event to the Renderer thread
  // to iterate over all Renderers, accumulates memory statistics, and resolves
  // the return promise.
  static RefPtr<MemoryReportPromise> AccumulateMemoryReport(MemoryReport aInitial);

  /// Can only be called from the render thread.
  void AddRenderer(wr::WindowId aWindowId, UniquePtr<RendererOGL> aRenderer);

  /// Can only be called from the render thread.
  void RemoveRenderer(wr::WindowId aWindowId);

  /// Can only be called from the render thread.
  RendererOGL* GetRenderer(wr::WindowId aWindowId);

  // RenderNotifier implementation

  /// Automatically forwarded to the render thread.
  void HandleFrame(wr::WindowId aWindowId, bool aRender);

  /// Automatically forwarded to the render thread.
  void WakeUp(wr::WindowId aWindowId);

  /// Automatically forwarded to the render thread.
  void PipelineSizeChanged(wr::WindowId aWindowId, uint64_t aPipelineId, float aWidth, float aHeight);

  /// Automatically forwarded to the render thread.
  void RunEvent(wr::WindowId aWindowId, UniquePtr<RendererEvent> aCallBack);

  /// Can only be called from the render thread.
  void UpdateAndRender(wr::WindowId aWindowId,
                       const TimeStamp& aStartTime,
                       bool aRender,
                       const Maybe<gfx::IntSize>& aReadbackSize,
                       const Maybe<Range<uint8_t>>& aReadbackBuffer,
                       bool aHadSlowFrame);

  void Pause(wr::WindowId aWindowId);
  bool Resume(wr::WindowId aWindowId);

  /// Can be called from any thread.
  void RegisterExternalImage(uint64_t aExternalImageId, already_AddRefed<RenderTextureHost> aTexture);

  /// Can be called from any thread.
  void UnregisterExternalImage(uint64_t aExternalImageId);

  /// Can be called from any thread.
  void UpdateRenderTextureHost(uint64_t aSrcExternalImageId, uint64_t aWrappedExternalImageId);

  /// Can only be called from the render thread.
  void UnregisterExternalImageDuringShutdown(uint64_t aExternalImageId);

  /// Can only be called from the render thread.
  RenderTextureHost* GetRenderTexture(WrExternalImageId aExternalImageId);

  /// Can be called from any thread.
  bool IsDestroyed(wr::WindowId aWindowId);
  /// Can be called from any thread.
  void SetDestroyed(wr::WindowId aWindowId);
  /// Can be called from any thread.
  bool TooManyPendingFrames(wr::WindowId aWindowId);
  /// Can be called from any thread.
  void IncPendingFrameCount(wr::WindowId aWindowId, const TimeStamp& aStartTime);
  /// Can be called from any thread.
  void DecPendingFrameCount(wr::WindowId aWindowId);
  /// Can be called from any thread.
  void IncRenderingFrameCount(wr::WindowId aWindowId);
  /// Can be called from any thread.
  void FrameRenderingComplete(wr::WindowId aWindowId);

  void NotifySlowFrame(wr::WindowId aWindowId);

  /// Can be called from any thread.
  WebRenderThreadPool& ThreadPool() { return mThreadPool; }

  /// Can only be called from the render thread.
  WebRenderProgramCache* ProgramCache();

  /// Can only be called from the render thread.
  WebRenderShaders* Shaders() { return mShaders.get(); }

  /// Can only be called from the render thread.
  gl::GLContext* SharedGL();

  void ClearSharedGL();

  /// Can only be called from the render thread.
  void HandleDeviceReset(const char* aWhere, bool aNotify);
  /// Can only be called from the render thread.
  bool IsHandlingDeviceReset();
  /// Can be called from any thread.
  void SimulateDeviceReset();

  size_t RendererCount();

private:
  explicit RenderThread(base::Thread* aThread);

  void DeferredRenderTextureHostDestroy();
  void ShutDownTask(layers::SynchronousTask* aTask);
  void InitDeviceTask();

  void DoAccumulateMemoryReport(MemoryReport, const RefPtr<MemoryReportPromise::Private>&);

  ~RenderThread();

  base::Thread* const mThread;

  WebRenderThreadPool mThreadPool;

  UniquePtr<WebRenderProgramCache> mProgramCache;
  UniquePtr<WebRenderShaders> mShaders;

  // An optional shared GLContext to be used for all
  // windows.
  RefPtr<gl::GLContext> mSharedGL;

  std::map<wr::WindowId, UniquePtr<RendererOGL>> mRenderers;

  struct WindowInfo {
    bool mIsDestroyed = false;
    int64_t mPendingCount = 0;
    int64_t mRenderingCount = 0;
    // One entry in this queue for each pending frame, so the length
    // should always equal mPendingCount
    std::queue<TimeStamp> mStartTimes;
    bool mHadSlowFrame = false;
  };

  Mutex mFrameCountMapLock;
  std::unordered_map<uint64_t, WindowInfo*> mWindowInfos;

  Mutex mRenderTextureMapLock;
  std::unordered_map<uint64_t, RefPtr<RenderTextureHost>> mRenderTextures;
  // Used to remove all RenderTextureHost that are going to be removed by
  // a deferred callback and remove them right away without waiting for the callback.
  // On device reset we have to remove all GL related resources right away.
  std::list<RefPtr<RenderTextureHost>> mRenderTexturesDeferred;
  bool mHasShutdown;

  bool mHandlingDeviceReset;
};

} // namespace wr
} // namespace mozilla

#endif
