/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Rooting_h
#define gc_Rooting_h

#include "js/GCVector.h"
#include "js/RootingAPI.h"

class JSAtom;
class JSLinearString;

namespace js {

class PropertyName;
class NativeObject;
class ArrayObject;
class GlobalObject;
class PlainObject;
class ScriptSourceObject;
class SavedFrame;
class Shape;
class ObjectGroup;
class DebuggerEnvironment;
class DebuggerFrame;
class DebuggerObject;
class Scope;

// These are internal counterparts to the public types such as HandleObject.

typedef JS::Handle<NativeObject*>           HandleNativeObject;
typedef JS::Handle<Shape*>                  HandleShape;
typedef JS::Handle<ObjectGroup*>            HandleObjectGroup;
typedef JS::Handle<JSAtom*>                 HandleAtom;
typedef JS::Handle<JSLinearString*>         HandleLinearString;
typedef JS::Handle<PropertyName*>           HandlePropertyName;
typedef JS::Handle<ArrayObject*>            HandleArrayObject;
typedef JS::Handle<PlainObject*>            HandlePlainObject;
typedef JS::Handle<SavedFrame*>             HandleSavedFrame;
typedef JS::Handle<ScriptSourceObject*>     HandleScriptSource;
typedef JS::Handle<DebuggerEnvironment*>    HandleDebuggerEnvironment;
typedef JS::Handle<DebuggerFrame*>          HandleDebuggerFrame;
typedef JS::Handle<DebuggerObject*>         HandleDebuggerObject;
typedef JS::Handle<Scope*>                  HandleScope;

typedef JS::MutableHandle<Shape*>               MutableHandleShape;
typedef JS::MutableHandle<JSAtom*>              MutableHandleAtom;
typedef JS::MutableHandle<NativeObject*>        MutableHandleNativeObject;
typedef JS::MutableHandle<PlainObject*>         MutableHandlePlainObject;
typedef JS::MutableHandle<SavedFrame*>          MutableHandleSavedFrame;
typedef JS::MutableHandle<DebuggerEnvironment*> MutableHandleDebuggerEnvironment;
typedef JS::MutableHandle<DebuggerFrame*>       MutableHandleDebuggerFrame;
typedef JS::MutableHandle<DebuggerObject*>      MutableHandleDebuggerObject;
typedef JS::MutableHandle<Scope*>               MutableHandleScope;

typedef JS::Rooted<NativeObject*>           RootedNativeObject;
typedef JS::Rooted<Shape*>                  RootedShape;
typedef JS::Rooted<ObjectGroup*>            RootedObjectGroup;
typedef JS::Rooted<JSAtom*>                 RootedAtom;
typedef JS::Rooted<JSLinearString*>         RootedLinearString;
typedef JS::Rooted<PropertyName*>           RootedPropertyName;
typedef JS::Rooted<ArrayObject*>            RootedArrayObject;
typedef JS::Rooted<GlobalObject*>           RootedGlobalObject;
typedef JS::Rooted<PlainObject*>            RootedPlainObject;
typedef JS::Rooted<SavedFrame*>             RootedSavedFrame;
typedef JS::Rooted<ScriptSourceObject*>     RootedScriptSource;
typedef JS::Rooted<DebuggerEnvironment*>    RootedDebuggerEnvironment;
typedef JS::Rooted<DebuggerFrame*>          RootedDebuggerFrame;
typedef JS::Rooted<DebuggerObject*>         RootedDebuggerObject;
typedef JS::Rooted<Scope*>                  RootedScope;

typedef JS::GCVector<JSFunction*>   FunctionVector;
typedef JS::GCVector<PropertyName*> PropertyNameVector;
typedef JS::GCVector<Shape*>        ShapeVector;
typedef JS::GCVector<JSString*>     StringVector;

} /* namespace js */

#endif /* gc_Rooting_h */
