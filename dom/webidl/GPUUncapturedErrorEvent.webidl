/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://gpuweb.github.io/gpuweb/
 */

// NOTE: This is part of WebGPU. We don't include this in `WebGPU.webidl`, in part because this
// split allows code to generate properly.

[Func="mozilla::webgpu::Instance::PrefEnabled",
 Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUUncapturedErrorEvent : Event {
    constructor(
        DOMString type,
        GPUUncapturedErrorEventInit gpuUncapturedErrorEventInitDict
    );
    [SameObject] readonly attribute GPUError error;
};

dictionary GPUUncapturedErrorEventInit : EventInit {
    required GPUError error;
};
