/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __HAL_SENSOR_H_
#define __HAL_SENSOR_H_

#include "mozilla/Observer.h"

namespace mozilla {
namespace hal {

/**
 * Enumeration of sensor types.  They are used to specify type while
 * register or unregister an observer for a sensor of given type.
 * If you add or change any here, do the same in GeckoHalDefines.java.
 */
enum SensorType {
  SENSOR_UNKNOWN = -1,
  SENSOR_ORIENTATION = 0,
  SENSOR_ACCELERATION = 1,
  SENSOR_PROXIMITY = 2,
  SENSOR_LINEAR_ACCELERATION = 3,
  SENSOR_GYROSCOPE = 4,
  SENSOR_LIGHT = 5,
  SENSOR_ROTATION_VECTOR = 6,
  SENSOR_GAME_ROTATION_VECTOR = 7,
  NUM_SENSOR_TYPE
};

class SensorData;

typedef Observer<SensorData> ISensorObserver;

/**
 * Enumeration of sensor accuracy types.
 */
enum SensorAccuracyType {
  SENSOR_ACCURACY_UNKNOWN = -1,
  SENSOR_ACCURACY_UNRELIABLE,
  SENSOR_ACCURACY_LOW,
  SENSOR_ACCURACY_MED,
  SENSOR_ACCURACY_HIGH,
  NUM_SENSOR_ACCURACY_TYPE
};

class SensorAccuracy;

typedef Observer<SensorAccuracy> ISensorAccuracyObserver;

} // namespace hal
} // namespace mozilla

#include "ipc/IPCMessageUtils.h"

namespace IPC {
  /**
   * Serializer for SensorType
   */
  template <>
  struct ParamTraits<mozilla::hal::SensorType>:
    public ContiguousEnumSerializer<
             mozilla::hal::SensorType,
             mozilla::hal::SENSOR_UNKNOWN,
             mozilla::hal::NUM_SENSOR_TYPE> {
  };

  template <>
  struct ParamTraits<mozilla::hal::SensorAccuracyType>:
    public ContiguousEnumSerializer<
             mozilla::hal::SensorAccuracyType,
             mozilla::hal::SENSOR_ACCURACY_UNKNOWN,
             mozilla::hal::NUM_SENSOR_ACCURACY_TYPE> {

  };
} // namespace IPC

#endif /* __HAL_SENSOR_H_ */
