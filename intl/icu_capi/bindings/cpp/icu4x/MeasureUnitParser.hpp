#ifndef icu4x_MeasureUnitParser_HPP
#define icu4x_MeasureUnitParser_HPP

#include "MeasureUnitParser.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "MeasureUnit.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::MeasureUnitParser* icu4x_MeasureUnitParser_create_mv1(void);

    typedef struct icu4x_MeasureUnitParser_create_with_provider_mv1_result {union {icu4x::capi::MeasureUnitParser* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_MeasureUnitParser_create_with_provider_mv1_result;
    icu4x_MeasureUnitParser_create_with_provider_mv1_result icu4x_MeasureUnitParser_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::MeasureUnit* icu4x_MeasureUnitParser_parse_mv1(const icu4x::capi::MeasureUnitParser* self, diplomat::capi::DiplomatStringView unit_id);

    void icu4x_MeasureUnitParser_destroy_mv1(MeasureUnitParser* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::MeasureUnitParser> icu4x::MeasureUnitParser::create() {
  auto result = icu4x::capi::icu4x_MeasureUnitParser_create_mv1();
  return std::unique_ptr<icu4x::MeasureUnitParser>(icu4x::MeasureUnitParser::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<icu4x::MeasureUnitParser>, icu4x::DataError> icu4x::MeasureUnitParser::create_with_provider(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_MeasureUnitParser_create_with_provider_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::MeasureUnitParser>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::MeasureUnitParser>>(std::unique_ptr<icu4x::MeasureUnitParser>(icu4x::MeasureUnitParser::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::MeasureUnitParser>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::MeasureUnit> icu4x::MeasureUnitParser::parse(std::string_view unit_id) const {
  auto result = icu4x::capi::icu4x_MeasureUnitParser_parse_mv1(this->AsFFI(),
    {unit_id.data(), unit_id.size()});
  return std::unique_ptr<icu4x::MeasureUnit>(icu4x::MeasureUnit::FromFFI(result));
}

inline const icu4x::capi::MeasureUnitParser* icu4x::MeasureUnitParser::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::MeasureUnitParser*>(this);
}

inline icu4x::capi::MeasureUnitParser* icu4x::MeasureUnitParser::AsFFI() {
  return reinterpret_cast<icu4x::capi::MeasureUnitParser*>(this);
}

inline const icu4x::MeasureUnitParser* icu4x::MeasureUnitParser::FromFFI(const icu4x::capi::MeasureUnitParser* ptr) {
  return reinterpret_cast<const icu4x::MeasureUnitParser*>(ptr);
}

inline icu4x::MeasureUnitParser* icu4x::MeasureUnitParser::FromFFI(icu4x::capi::MeasureUnitParser* ptr) {
  return reinterpret_cast<icu4x::MeasureUnitParser*>(ptr);
}

inline void icu4x::MeasureUnitParser::operator delete(void* ptr) {
  icu4x::capi::icu4x_MeasureUnitParser_destroy_mv1(reinterpret_cast<icu4x::capi::MeasureUnitParser*>(ptr));
}


#endif // icu4x_MeasureUnitParser_HPP
