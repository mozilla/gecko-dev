/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{convert::TryInto, ops::RangeInclusive};

use nserror::nsresult;
use nsstring::{nsCString, nsString};
use rusqlite::{
    types::{FromSql, FromSqlError, FromSqlResult, ToSqlOutput, ValueRef},
    ToSql,
};
use storage_variant::{DataType, NsIVariantExt, VariantType};
use xpcom::{interfaces::nsIVariant, RefPtr};

/// The range of integral values that an `f64` can represent
/// without losing precision.
const F64_INTEGER_RANGE: RangeInclusive<f64> =
    ((-1i64 << f64::MANTISSA_DIGITS) + 1) as f64..=((1i64 << f64::MANTISSA_DIGITS) - 1) as f64;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Value(serde_json::Value);

impl Value {
    pub fn from_variant(variant: &nsIVariant) -> Result<Option<Self>, ValueError> {
        let raw_type = variant.get_data_type();
        let result = || -> Result<Option<Self>, nsresult> {
            Ok(Some(Self(match raw_type.try_into()? {
                DataType::Bool => bool::from_variant(variant)?.into(),
                DataType::Int32 => i32::from_variant(variant)?.into(),
                DataType::Int64 => i64::from_variant(variant)?.into(),
                DataType::Double => {
                    let value = f64::from_variant(variant)?;
                    let n = value.trunc();
                    if F64_INTEGER_RANGE.contains(&value) && value - n == 0.0 {
                        // `serde_json::Number` loses precision when
                        // deserializing either bound of the safe integer range
                        // into an `f64` [1], so we serialize all
                        // integral values in that range as `i64`s.
                        //
                        // [1]: https://github.com/serde-rs/json/issues/1170
                        (n as i64).into()
                    } else {
                        value.into()
                    }
                }
                DataType::AString | DataType::WCharStr | DataType::WStringSizeIs => {
                    nsString::from_variant(variant)?.to_string().into()
                }
                DataType::CString
                | DataType::CharStr
                | DataType::StringSizeIs
                | DataType::Utf8String => nsCString::from_variant(variant)?.to_utf8().into(),
                DataType::Void | DataType::EmptyArray | DataType::Empty => return Ok(None),
            })))
        }();
        result.map_err(|code| ValueError::FromVariant(raw_type, code))
    }

    pub fn to_variant(&self) -> Result<RefPtr<nsIVariant>, ValueError> {
        Ok(match &self.0 {
            serde_json::Value::Null => ().into_variant(),
            serde_json::Value::Bool(v) => v.into_variant(),
            serde_json::Value::Number(n) if n.is_i64() => n
                .as_i64()
                .map(i64::into_variant)
                .ok_or(ValueError::ToVariant)?,
            serde_json::Value::Number(n) if n.is_f64() => n
                .as_f64()
                .map(f64::into_variant)
                .ok_or(ValueError::ToVariant)?,
            serde_json::Value::String(s) => nsCString::from(s).into_variant(),
            serde_json::Value::Number(_)
            | serde_json::Value::Array(_)
            | serde_json::Value::Object(_) => Err(ValueError::ToVariant)?,
        })
    }
}

impl ToSql for Value {
    fn to_sql(&self) -> rusqlite::Result<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(serde_json::to_string(&self.0).map_err(
            |e| rusqlite::Error::ToSqlConversionFailure(e.into()),
        )?))
    }
}

impl FromSql for Value {
    fn column_result(value: ValueRef<'_>) -> FromSqlResult<Self> {
        Ok(Self(
            serde_json::from_slice(value.as_bytes()?).map_err(|e| FromSqlError::Other(e.into()))?,
        ))
    }
}

#[derive(thiserror::Error, Debug)]
pub enum ValueError {
    #[error("from variant of type {0}: {1}")]
    FromVariant(u16, nsresult),
    #[error("to variant")]
    ToVariant,
}
