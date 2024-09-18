// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/color_description.h"

#include <errno.h>

#include <cmath>

#include "lib/jxl/base/common.h"

namespace jxl {

namespace {

template <typename T>
struct EnumName {
  const char* name;
  T value;
};

constexpr auto kJxlColorSpaceNames =
    to_array<EnumName<JxlColorSpace>>({{"RGB", JXL_COLOR_SPACE_RGB},
                                       {"Gra", JXL_COLOR_SPACE_GRAY},
                                       {"XYB", JXL_COLOR_SPACE_XYB},
                                       {"CS?", JXL_COLOR_SPACE_UNKNOWN}});

constexpr auto kJxlWhitePointNames =
    to_array<EnumName<JxlWhitePoint>>({{"D65", JXL_WHITE_POINT_D65},
                                       {"Cst", JXL_WHITE_POINT_CUSTOM},
                                       {"EER", JXL_WHITE_POINT_E},
                                       {"DCI", JXL_WHITE_POINT_DCI}});

constexpr auto kJxlPrimariesNames =
    to_array<EnumName<JxlPrimaries>>({{"SRG", JXL_PRIMARIES_SRGB},
                                      {"Cst", JXL_PRIMARIES_CUSTOM},
                                      {"202", JXL_PRIMARIES_2100},
                                      {"DCI", JXL_PRIMARIES_P3}});

constexpr auto kJxlTransferFunctionNames =
    to_array<EnumName<JxlTransferFunction>>(
        {{"709", JXL_TRANSFER_FUNCTION_709},
         {"TF?", JXL_TRANSFER_FUNCTION_UNKNOWN},
         {"Lin", JXL_TRANSFER_FUNCTION_LINEAR},
         {"SRG", JXL_TRANSFER_FUNCTION_SRGB},
         {"PeQ", JXL_TRANSFER_FUNCTION_PQ},
         {"DCI", JXL_TRANSFER_FUNCTION_DCI},
         {"HLG", JXL_TRANSFER_FUNCTION_HLG},
         {"", JXL_TRANSFER_FUNCTION_GAMMA}});

constexpr auto kJxlRenderingIntentNames =
    to_array<EnumName<JxlRenderingIntent>>(
        {{"Per", JXL_RENDERING_INTENT_PERCEPTUAL},
         {"Rel", JXL_RENDERING_INTENT_RELATIVE},
         {"Sat", JXL_RENDERING_INTENT_SATURATION},
         {"Abs", JXL_RENDERING_INTENT_ABSOLUTE}});

template <typename T, size_t N>
Status ParseEnum(const std::string& token,
                 const std::array<EnumName<T>, N>& enum_values, T* value) {
  for (size_t i = 0; i < enum_values.size(); i++) {
    if (enum_values[i].name == token) {
      *value = enum_values[i].value;
      return true;
    }
  }
  return false;
}

class Tokenizer {
 public:
  Tokenizer(const std::string* input, char separator)
      : input_(input), separator_(separator) {}

  Status Next(std::string* next) {
    const size_t end = input_->find(separator_, start_);
    if (end == std::string::npos) {
      *next = input_->substr(start_);  // rest of string
    } else {
      *next = input_->substr(start_, end - start_);
    }
    if (next->empty()) return JXL_FAILURE("Missing token");
    start_ = end + 1;
    return true;
  }

 private:
  const std::string* const input_;  // not owned
  const char separator_;
  size_t start_ = 0;  // of next token
};

Status ParseDouble(const std::string& num, double* d) {
  char* end;
  errno = 0;
  *d = strtod(num.c_str(), &end);
  if (*d == 0.0 && end == num.c_str()) {
    return JXL_FAILURE("Invalid double: %s", num.c_str());
  }
  if (std::isnan(*d)) {
    return JXL_FAILURE("Invalid double: %s", num.c_str());
  }
  if (errno == ERANGE) {
    return JXL_FAILURE("Double out of range: %s", num.c_str());
  }
  return true;
}

Status ParseDouble(Tokenizer* tokenizer, double* d) {
  std::string num;
  JXL_RETURN_IF_ERROR(tokenizer->Next(&num));
  return ParseDouble(num, d);
}

Status ParseColorSpace(Tokenizer* tokenizer, JxlColorEncoding* c) {
  std::string str;
  JXL_RETURN_IF_ERROR(tokenizer->Next(&str));
  JxlColorSpace cs;
  if (ParseEnum(str, kJxlColorSpaceNames, &cs)) {
    c->color_space = cs;
    return true;
  }

  return JXL_FAILURE("Unknown ColorSpace %s", str.c_str());
}

Status ParseWhitePoint(Tokenizer* tokenizer, JxlColorEncoding* c) {
  if (c->color_space == JXL_COLOR_SPACE_XYB) {
    // Implicit white point.
    c->white_point = JXL_WHITE_POINT_D65;
    return true;
  }

  std::string str;
  JXL_RETURN_IF_ERROR(tokenizer->Next(&str));
  if (ParseEnum(str, kJxlWhitePointNames, &c->white_point)) return true;

  Tokenizer xy_tokenizer(&str, ';');
  c->white_point = JXL_WHITE_POINT_CUSTOM;
  JXL_RETURN_IF_ERROR(ParseDouble(&xy_tokenizer, c->white_point_xy + 0));
  JXL_RETURN_IF_ERROR(ParseDouble(&xy_tokenizer, c->white_point_xy + 1));
  return true;
}

Status ParsePrimaries(Tokenizer* tokenizer, JxlColorEncoding* c) {
  if (c->color_space == JXL_COLOR_SPACE_GRAY ||
      c->color_space == JXL_COLOR_SPACE_XYB) {
    // No primaries case.
    return true;
  }

  std::string str;
  JXL_RETURN_IF_ERROR(tokenizer->Next(&str));
  if (ParseEnum(str, kJxlPrimariesNames, &c->primaries)) return true;

  Tokenizer xy_tokenizer(&str, ';');
  JXL_RETURN_IF_ERROR(ParseDouble(&xy_tokenizer, c->primaries_red_xy + 0));
  JXL_RETURN_IF_ERROR(ParseDouble(&xy_tokenizer, c->primaries_red_xy + 1));
  JXL_RETURN_IF_ERROR(ParseDouble(&xy_tokenizer, c->primaries_green_xy + 0));
  JXL_RETURN_IF_ERROR(ParseDouble(&xy_tokenizer, c->primaries_green_xy + 1));
  JXL_RETURN_IF_ERROR(ParseDouble(&xy_tokenizer, c->primaries_blue_xy + 0));
  JXL_RETURN_IF_ERROR(ParseDouble(&xy_tokenizer, c->primaries_blue_xy + 1));
  c->primaries = JXL_PRIMARIES_CUSTOM;

  return true;
}

Status ParseRenderingIntent(Tokenizer* tokenizer, JxlColorEncoding* c) {
  std::string str;
  JXL_RETURN_IF_ERROR(tokenizer->Next(&str));
  if (ParseEnum(str, kJxlRenderingIntentNames, &c->rendering_intent))
    return true;

  return JXL_FAILURE("Invalid RenderingIntent %s\n", str.c_str());
}

Status ParseTransferFunction(Tokenizer* tokenizer, JxlColorEncoding* c) {
  if (c->color_space == JXL_COLOR_SPACE_XYB) {
    // Implicit TF.
    c->transfer_function = JXL_TRANSFER_FUNCTION_GAMMA;
    c->gamma = 1 / 3.;
    return true;
  }

  std::string str;
  JXL_RETURN_IF_ERROR(tokenizer->Next(&str));
  if (ParseEnum(str, kJxlTransferFunctionNames, &c->transfer_function)) {
    return true;
  }

  if (str[0] == 'g') {
    JXL_RETURN_IF_ERROR(ParseDouble(str.substr(1), &c->gamma));
    c->transfer_function = JXL_TRANSFER_FUNCTION_GAMMA;
    return true;
  }

  return JXL_FAILURE("Invalid gamma %s", str.c_str());
}

}  // namespace

Status ParseDescription(const std::string& description, JxlColorEncoding* c) {
  *c = {};
  if (description == "sRGB") {
    c->color_space = JXL_COLOR_SPACE_RGB;
    c->white_point = JXL_WHITE_POINT_D65;
    c->primaries = JXL_PRIMARIES_SRGB;
    c->transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
    c->rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL;
  } else if (description == "DisplayP3") {
    c->color_space = JXL_COLOR_SPACE_RGB;
    c->white_point = JXL_WHITE_POINT_D65;
    c->primaries = JXL_PRIMARIES_P3;
    c->transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
    c->rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL;
  } else if (description == "Rec2100PQ") {
    c->color_space = JXL_COLOR_SPACE_RGB;
    c->white_point = JXL_WHITE_POINT_D65;
    c->primaries = JXL_PRIMARIES_2100;
    c->transfer_function = JXL_TRANSFER_FUNCTION_PQ;
    c->rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
  } else if (description == "Rec2100HLG") {
    c->color_space = JXL_COLOR_SPACE_RGB;
    c->white_point = JXL_WHITE_POINT_D65;
    c->primaries = JXL_PRIMARIES_2100;
    c->transfer_function = JXL_TRANSFER_FUNCTION_HLG;
    c->rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
  } else {
    Tokenizer tokenizer(&description, '_');
    JXL_RETURN_IF_ERROR(ParseColorSpace(&tokenizer, c));
    JXL_RETURN_IF_ERROR(ParseWhitePoint(&tokenizer, c));
    JXL_RETURN_IF_ERROR(ParsePrimaries(&tokenizer, c));
    JXL_RETURN_IF_ERROR(ParseRenderingIntent(&tokenizer, c));
    JXL_RETURN_IF_ERROR(ParseTransferFunction(&tokenizer, c));
  }
  return true;
}

}  // namespace jxl
