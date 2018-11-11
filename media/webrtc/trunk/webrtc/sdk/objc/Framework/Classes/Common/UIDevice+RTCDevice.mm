/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/UIDevice+RTCDevice.h"

#include <memory>
#import <sys/utsname.h>

@implementation UIDevice (RTCDevice)

+ (RTCDeviceType)deviceType {
  NSDictionary *machineNameToType = @{
    @"iPhone1,1": @(RTCDeviceTypeIPhone1G),
    @"iPhone1,2": @(RTCDeviceTypeIPhone3G),
    @"iPhone2,1": @(RTCDeviceTypeIPhone3GS),
    @"iPhone3,1": @(RTCDeviceTypeIPhone4),
    @"iPhone3,3": @(RTCDeviceTypeIPhone4Verizon),
    @"iPhone4,1": @(RTCDeviceTypeIPhone4S),
    @"iPhone5,1": @(RTCDeviceTypeIPhone5GSM),
    @"iPhone5,2": @(RTCDeviceTypeIPhone5GSM_CDMA),
    @"iPhone5,3": @(RTCDeviceTypeIPhone5CGSM),
    @"iPhone5,4": @(RTCDeviceTypeIPhone5CGSM_CDMA),
    @"iPhone6,1": @(RTCDeviceTypeIPhone5SGSM),
    @"iPhone6,2": @(RTCDeviceTypeIPhone5SGSM_CDMA),
    @"iPhone7,1": @(RTCDeviceTypeIPhone6Plus),
    @"iPhone7,2": @(RTCDeviceTypeIPhone6),
    @"iPhone8,1": @(RTCDeviceTypeIPhone6S),
    @"iPhone8,2": @(RTCDeviceTypeIPhone6SPlus),
    @"iPhone9,1": @(RTCDeviceTypeIPhone7),
    @"iPhone9,2": @(RTCDeviceTypeIPhone7Plus),
    @"iPhone9,3": @(RTCDeviceTypeIPhone7),
    @"iPhone9,4": @(RTCDeviceTypeIPhone7Plus),
    @"iPhone10,1": @(RTCDeviceTypeIPhone8),
    @"iPhone10,2": @(RTCDeviceTypeIPhone8Plus),
    @"iPhone10,3": @(RTCDeviceTypeIPhoneX),
    @"iPhone10,4": @(RTCDeviceTypeIPhone8),
    @"iPhone10,5": @(RTCDeviceTypeIPhone8Plus),
    @"iPhone10,6": @(RTCDeviceTypeIPhoneX),
    @"iPod1,1": @(RTCDeviceTypeIPodTouch1G),
    @"iPod2,1": @(RTCDeviceTypeIPodTouch2G),
    @"iPod3,1": @(RTCDeviceTypeIPodTouch3G),
    @"iPod4,1": @(RTCDeviceTypeIPodTouch4G),
    @"iPod5,1": @(RTCDeviceTypeIPodTouch5G),
    @"iPad1,1": @(RTCDeviceTypeIPad),
    @"iPad2,1": @(RTCDeviceTypeIPad2Wifi),
    @"iPad2,2": @(RTCDeviceTypeIPad2GSM),
    @"iPad2,3": @(RTCDeviceTypeIPad2CDMA),
    @"iPad2,4": @(RTCDeviceTypeIPad2Wifi2),
    @"iPad2,5": @(RTCDeviceTypeIPadMiniWifi),
    @"iPad2,6": @(RTCDeviceTypeIPadMiniGSM),
    @"iPad2,7": @(RTCDeviceTypeIPadMiniGSM_CDMA),
    @"iPad3,1": @(RTCDeviceTypeIPad3Wifi),
    @"iPad3,2": @(RTCDeviceTypeIPad3GSM_CDMA),
    @"iPad3,3": @(RTCDeviceTypeIPad3GSM),
    @"iPad3,4": @(RTCDeviceTypeIPad4Wifi),
    @"iPad3,5": @(RTCDeviceTypeIPad4GSM),
    @"iPad3,6": @(RTCDeviceTypeIPad4GSM_CDMA),
    @"iPad4,1": @(RTCDeviceTypeIPadAirWifi),
    @"iPad4,2": @(RTCDeviceTypeIPadAirCellular),
    @"iPad4,4": @(RTCDeviceTypeIPadMini2GWifi),
    @"iPad4,5": @(RTCDeviceTypeIPadMini2GCellular),
    @"i386": @(RTCDeviceTypeSimulatori386),
    @"x86_64": @(RTCDeviceTypeSimulatorx86_64),
  };

  RTCDeviceType deviceType = RTCDeviceTypeUnknown;
  NSNumber *typeNumber = machineNameToType[[self machineName]];
  if (typeNumber) {
    deviceType = static_cast<RTCDeviceType>(typeNumber.integerValue);
  }
  return deviceType;
}

+ (NSString *)machineName {
  struct utsname systemInfo;
  uname(&systemInfo);
  return [[NSString alloc] initWithCString:systemInfo.machine
                                  encoding:NSUTF8StringEncoding];
}

+ (double)currentDeviceSystemVersion {
  return [self currentDevice].systemVersion.doubleValue;
}

+ (BOOL)isIOS9OrLater {
  return [self currentDeviceSystemVersion] >= 9.0;
}

+ (BOOL)isIOS11OrLater {
  return [self currentDeviceSystemVersion] >= 11.0;
}

@end
