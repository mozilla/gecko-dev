/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the middleman process uses to send messages to
// the UI process with painting data from the child process.

#include "ParentInternal.h"

#include "chrome/common/mach_ipc_mac.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/PTextureChild.h"

#include <mach/mach_vm.h>

namespace mozilla {
namespace recordreplay {
namespace parent {

// Graphics memory buffer shared with all child processes.
void* gGraphicsMemory;

static mach_port_t gGraphicsPort;
static ReceivePort* gGraphicsReceiver;

void InitializeGraphicsMemory() {
  mach_vm_address_t address;
  kern_return_t kr = mach_vm_allocate(mach_task_self(), &address,
                                      GraphicsMemorySize, VM_FLAGS_ANYWHERE);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);

  memory_object_size_t memoryObjectSize = GraphicsMemorySize;
  kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, address,
                                 VM_PROT_DEFAULT, &gGraphicsPort,
                                 MACH_PORT_NULL);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);
  MOZ_RELEASE_ASSERT(memoryObjectSize == GraphicsMemorySize);

  gGraphicsMemory = (void*)address;
  gGraphicsReceiver =
      new ReceivePort(nsPrintfCString("WebReplay.%d", getpid()).get());
}

void SendGraphicsMemoryToChild() {
  MachReceiveMessage handshakeMessage;
  kern_return_t kr = gGraphicsReceiver->WaitForMessage(&handshakeMessage, 0);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);

  MOZ_RELEASE_ASSERT(handshakeMessage.GetMessageID() ==
                     GraphicsHandshakeMessageId);
  mach_port_t childPort = handshakeMessage.GetTranslatedPort(0);
  MOZ_RELEASE_ASSERT(childPort != MACH_PORT_NULL);

  MachSendMessage message(GraphicsMemoryMessageId);
  message.AddDescriptor(
      MachMsgPortDescriptor(gGraphicsPort, MACH_MSG_TYPE_COPY_SEND));

  MachPortSender sender(childPort);
  kr = sender.SendMessage(message, 1000);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);
}

// Global object for the sandbox used to paint graphics data in this process.
static JS::PersistentRootedObject* gGraphicsSandbox;

static void InitGraphicsSandbox() {
  MOZ_RELEASE_ASSERT(!gGraphicsSandbox);

  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::PrivilegedJunkScope())) {
    MOZ_CRASH("InitGraphicsSandbox");
  }

  JSContext* cx = jsapi.cx();

  xpc::SandboxOptions options;
  options.sandboxName.AssignLiteral("Record/Replay Graphics Sandbox");
  options.invisibleToDebugger = true;
  RootedValue v(cx);
  nsresult rv =
      CreateSandboxObject(cx, &v, nsXPConnect::SystemPrincipal(), options);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  gGraphicsSandbox = new JS::PersistentRootedObject(cx);
  *gGraphicsSandbox = ::js::UncheckedUnwrap(&v.toObject());

  JSAutoRealm ar(cx, *gGraphicsSandbox);

  ErrorResult er;
  dom::GlobalObject global(cx, *gGraphicsSandbox);
  RootedObject obj(cx);
  dom::ChromeUtils::Import(
      global,
      NS_LITERAL_STRING("resource://devtools/server/actors/replay/graphics.js"),
      dom::Optional<HandleObject>(), &obj, er);
  MOZ_RELEASE_ASSERT(!er.Failed());
}

// Buffer used to transform graphics memory, if necessary.
static void* gBufferMemory;

// The dimensions of the data in the graphics shmem buffer.
static size_t gLastPaintWidth, gLastPaintHeight;

// Explicit Paint messages received from the child need to be handled with
// care to make sure we show correct graphics. Each Paint message is for the
// the process state at the most recent checkpoint in the past. When running
// (forwards or backwards) between the checkpoint and the Paint message,
// we could pause at a breakpoint and repaint the graphics at that point,
// reflecting the process state at a point later than at the checkpoint.
// In this case the Paint message's graphics will be stale. To avoid showing
// its graphics, we wait until both the Paint and the checkpoint itself have
// been hit, with no intervening repaint.

// The last explicit paint message received from the child, if there has not
// been an intervening repaint.
static UniquePtr<PaintMessage> gLastExplicitPaint;

// The last checkpoint the child reached, if there has not been an intervening
// repaint.
static size_t gLastCheckpoint;

void UpdateGraphicsInUIProcess(const PaintMessage* aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (aMsg) {
    gLastPaintWidth = aMsg->mWidth;
    gLastPaintHeight = aMsg->mHeight;
  }

  if (!gLastPaintWidth || !gLastPaintHeight) {
    return;
  }

  bool hadFailure = !aMsg;

  // Clear out the last explicit paint information. This may delete aMsg.
  gLastExplicitPaint = nullptr;
  gLastCheckpoint = CheckpointId::Invalid;

  // Make sure there is a sandbox which is running the graphics JS module.
  if (!gGraphicsSandbox) {
    InitGraphicsSandbox();
  }

  size_t width = gLastPaintWidth;
  size_t height = gLastPaintHeight;
  size_t stride =
      layers::ImageDataSerializer::ComputeRGBStride(gSurfaceFormat, width);

  // Make sure the width and height are appropriately sized.
  CheckedInt<size_t> scaledWidth = CheckedInt<size_t>(width) * 4;
  CheckedInt<size_t> scaledHeight = CheckedInt<size_t>(height) * stride;
  MOZ_RELEASE_ASSERT(scaledWidth.isValid() && scaledWidth.value() <= stride);
  MOZ_RELEASE_ASSERT(scaledHeight.isValid() &&
                     scaledHeight.value() <= GraphicsMemorySize);

  // Get memory which we can pass to the graphics module to store in a canvas.
  // Use the shared memory buffer directly, unless we need to transform the
  // data due to extra memory in each row of the data which the child process
  // sent us.
  MOZ_RELEASE_ASSERT(gGraphicsMemory);
  void* memory = gGraphicsMemory;
  if (stride != width * 4) {
    if (!gBufferMemory) {
      gBufferMemory = malloc(GraphicsMemorySize);
    }
    memory = gBufferMemory;
    for (size_t y = 0; y < height; y++) {
      char* src = (char*)gGraphicsMemory + y * stride;
      char* dst = (char*)gBufferMemory + y * width * 4;
      memcpy(dst, src, width * 4);
    }
  }

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, *gGraphicsSandbox);

  JSObject* bufferObject =
      JS_NewArrayBufferWithExternalContents(cx, width * height * 4, memory);
  MOZ_RELEASE_ASSERT(bufferObject);

  JS::AutoValueArray<4> args(cx);
  args[0].setObject(*bufferObject);
  args[1].setInt32(width);
  args[2].setInt32(height);
  args[3].setBoolean(hadFailure);

  // Call into the graphics module to update the canvas it manages.
  RootedValue rval(cx);
  if (!JS_CallFunctionName(cx, *gGraphicsSandbox, "UpdateCanvas", args,
                           &rval)) {
    MOZ_CRASH("UpdateGraphicsInUIProcess");
  }
}

static void MaybeTriggerExplicitPaint() {
  if (gLastExplicitPaint &&
      gLastExplicitPaint->mCheckpointId == gLastCheckpoint) {
    UpdateGraphicsInUIProcess(gLastExplicitPaint.get());
  }
}

void MaybeUpdateGraphicsAtPaint(const PaintMessage& aMsg) {
  gLastExplicitPaint.reset(new PaintMessage(aMsg));
  MaybeTriggerExplicitPaint();
}

void MaybeUpdateGraphicsAtCheckpoint(size_t aCheckpointId) {
  gLastCheckpoint = aCheckpointId;
  MaybeTriggerExplicitPaint();
}

bool InRepaintStressMode() {
  static bool checked = false;
  static bool rv;
  if (!checked) {
    AutoEnsurePassThroughThreadEvents pt;
    rv = TestEnv("MOZ_RECORD_REPLAY_REPAINT_STRESS");
    checked = true;
  }
  return rv;
}

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla
