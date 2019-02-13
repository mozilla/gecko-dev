/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ANDROIDFW_KEYBOARD_H
#define _ANDROIDFW_KEYBOARD_H

#include "Input.h"
#include "InputDevice.h"
#include <utils/Errors.h>
#include <utils/String8.h>
#include <utils/PropertyMap.h>

namespace android {

enum {
    /* Device id of the built in keyboard. */
    DEVICE_ID_BUILT_IN_KEYBOARD = 0,

    /* Device id of a generic virtual keyboard with a full layout that can be used
     * to synthesize key events. */
    DEVICE_ID_VIRTUAL_KEYBOARD = -1,
};

class KeyLayoutMap;
class KeyCharacterMap;

/**
 * Loads the key layout map and key character map for a keyboard device.
 */
class KeyMap {
public:
    String8 keyLayoutFile;
    sp<KeyLayoutMap> keyLayoutMap;

    String8 keyCharacterMapFile;
    sp<KeyCharacterMap> keyCharacterMap;

    KeyMap();
    ~KeyMap();

    status_t load(const InputDeviceIdentifier& deviceIdenfier,
            const PropertyMap* deviceConfiguration);

    inline bool haveKeyLayout() const {
        return !keyLayoutFile.isEmpty();
    }

    inline bool haveKeyCharacterMap() const {
        return !keyCharacterMapFile.isEmpty();
    }

    inline bool isComplete() const {
        return haveKeyLayout() && haveKeyCharacterMap();
    }

private:
    bool probeKeyMap(const InputDeviceIdentifier& deviceIdentifier, const String8& name);
    status_t loadKeyLayout(const InputDeviceIdentifier& deviceIdentifier, const String8& name);
    status_t loadKeyCharacterMap(const InputDeviceIdentifier& deviceIdentifier,
            const String8& name);
    String8 getPath(const InputDeviceIdentifier& deviceIdentifier,
            const String8& name, InputDeviceConfigurationFileType type);
};

/**
 * Returns true if the keyboard is eligible for use as a built-in keyboard.
 */
extern bool isEligibleBuiltInKeyboard(const InputDeviceIdentifier& deviceIdentifier,
        const PropertyMap* deviceConfiguration, const KeyMap* keyMap);

/**
 * Gets a key code by its short form label, eg. "HOME".
 * Returns 0 if unknown.
 */
extern int32_t getKeyCodeByLabel(const char* label);

/**
 * Gets a key flag by its short form label, eg. "WAKE".
 * Returns 0 if unknown.
 */
extern uint32_t getKeyFlagByLabel(const char* label);

/**
 * Gets a axis by its short form label, eg. "X".
 * Returns -1 if unknown.
 */
extern int32_t getAxisByLabel(const char* label);

/**
 * Gets a axis label by its id.
 * Returns NULL if unknown.
 */
extern const char* getAxisLabel(int32_t axisId);

extern int32_t getLedByLabel(const char* label);

/**
 * Updates a meta state field when a key is pressed or released.
 */
extern int32_t updateMetaState(int32_t keyCode, bool down, int32_t oldMetaState);

/**
 * Returns true if a key is a meta key like ALT or CAPS_LOCK.
 */
extern bool isMetaKey(int32_t keyCode);

} // namespace android

#endif // _ANDROIDFW_KEYBOARD_H
