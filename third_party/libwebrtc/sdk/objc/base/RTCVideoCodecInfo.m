/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoCodecInfo.h"

@implementation RTC_OBJC_TYPE (RTCVideoCodecInfo)

@synthesize name = _name;
@synthesize parameters = _parameters;
@synthesize scalabilityModes = _scalabilityModes;

- (instancetype)initWithName:(NSString *)name {
  return [self initWithName:name parameters:@{} scalabilityModes:@[]];
}

- (instancetype)initWithName:(NSString *)name
                  parameters:(nullable NSDictionary<NSString *, NSString *> *)parameters {
  NSDictionary<NSString *, NSString *> *params = parameters ? parameters : @{};
  return [self initWithName:name parameters:params scalabilityModes:@[]];
}

- (instancetype)initWithName:(NSString *)name
                  parameters:(NSDictionary<NSString *, NSString *> *)parameters
            scalabilityModes:(NSArray<NSString *> *)scalabilityModes {
  self = [super init];
  if (self) {
    _name = name;
    _parameters = parameters;
    _scalabilityModes = scalabilityModes;
  }

  return self;
}

- (BOOL)isEqualToCodecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info {
  if (!info || ![self.name isEqualToString:info.name] ||
      ![self.parameters isEqualToDictionary:info.parameters] ||
      ![self.scalabilityModes isEqualToArray:info.scalabilityModes]) {
    return NO;
  }
  return YES;
}

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;
  if (![object isKindOfClass:[self class]])
    return NO;
  return [self isEqualToCodecInfo:object];
}

- (NSUInteger)hash {
  return [self.name hash] ^ [self.parameters hash];
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder *)decoder {
  return [self initWithName:[decoder decodeObjectForKey:@"name"]
                 parameters:[decoder decodeObjectForKey:@"parameters"]];
}

- (void)encodeWithCoder:(NSCoder *)encoder {
  [encoder encodeObject:_name forKey:@"name"];
  [encoder encodeObject:_parameters forKey:@"parameters"];
}

@end
