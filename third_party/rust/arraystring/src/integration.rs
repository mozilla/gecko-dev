//! Integrates `ArrayString` with other crates' traits

use crate::prelude::*;

#[cfg(all(feature = "diesel-traits", feature = "std"))]
use std::io::Write;

#[cfg(feature = "diesel-traits")]
use diesel::{expression::*, prelude::*, query_builder::*, row::Row, sql_types::*};

#[cfg(feature = "diesel-traits")]
use diesel::backend::Backend;

#[cfg(feature = "diesel-traits")]
use diesel::deserialize::{self, FromSql, FromSqlRow, Queryable};

#[cfg(all(feature = "diesel-traits", feature = "std"))]
use diesel::serialize::{self, Output, ToSql};

#[cfg(feature = "serde-traits")]
use serde::{de::Deserializer, ser::Serializer, Deserialize, Serialize};

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "serde-traits")))]
#[cfg(feature = "serde-traits")]
impl<SIZE> Serialize for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn serialize<S: Serializer>(&self, ser: S) -> Result<S::Ok, S::Error> {
        Serialize::serialize(self.as_str(), ser)
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "serde-traits")))]
#[cfg(feature = "serde-traits")]
impl<'a, SIZE> Deserialize<'a> for ArrayString<SIZE>
where
    SIZE: Capacity,
{
    #[inline]
    fn deserialize<D: Deserializer<'a>>(des: D) -> Result<Self, D::Error> {
        <&str>::deserialize(des).map(Self::from_str_truncate)
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<SIZE: Capacity> Expression for ArrayString<SIZE> {
    type SqlType = VarChar;
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<SIZE: Capacity, QS> SelectableExpression<QS> for ArrayString<SIZE> {}
#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<SIZE: Capacity, QS> AppearsOnTable<QS> for ArrayString<SIZE> {}
#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<SIZE: Capacity> NonAggregate for ArrayString<SIZE> {}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<SIZE, DB> QueryFragment<DB> for ArrayString<SIZE>
where
    SIZE: Capacity,
    DB: Backend + HasSqlType<VarChar>,
{
    #[inline]
    fn walk_ast(&self, mut pass: AstPass<DB>) -> QueryResult<()> {
        pass.push_bind_param::<Varchar, _>(&self.as_str())?;
        Ok(())
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<SIZE, ST, DB> FromSql<ST, DB> for ArrayString<SIZE>
where
    SIZE: Capacity,
    DB: Backend,
    *const str: FromSql<ST, DB>,
{
    #[inline]
    fn from_sql(bytes: Option<&DB::RawValue>) -> deserialize::Result<Self> {
        let ptr: *const str = FromSql::<ST, DB>::from_sql(bytes)?;
        // We know that the pointer impl will never return null
        Ok(Self::from_str_truncate(unsafe { &*ptr }))
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<SIZE, ST, DB> FromSqlRow<ST, DB> for ArrayString<SIZE>
where
    SIZE: Capacity,
    DB: Backend,
    *const str: FromSql<ST, DB>,
{
    const FIELDS_NEEDED: usize = 1;

    #[inline]
    fn build_from_row<T: Row<DB>>(row: &mut T) -> deserialize::Result<Self> {
        FromSql::<ST, DB>::from_sql(row.take())
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<SIZE, ST, DB> Queryable<ST, DB> for ArrayString<SIZE>
where
    SIZE: Capacity,
    DB: Backend,
    *const str: FromSql<ST, DB>,
{
    type Row = Self;

    #[inline]
    fn build(row: Self::Row) -> Self {
        row
    }
}

#[cfg_attr(
    docs_rs_workaround,
    doc(cfg(all(feature = "diesel-traits", feature = "std")))
)]
#[cfg(all(feature = "diesel-traits", feature = "std"))]
impl<SIZE, DB> ToSql<VarChar, DB> for ArrayString<SIZE>
where
    SIZE: Capacity,
    DB: Backend,
{
    #[inline]
    fn to_sql<W: Write>(&self, out: &mut Output<W, DB>) -> serialize::Result {
        ToSql::<VarChar, DB>::to_sql(self.as_str(), out)
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl Expression for CacheString {
    type SqlType = VarChar;
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<QS> SelectableExpression<QS> for CacheString {}
#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<QS> AppearsOnTable<QS> for CacheString {}
#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl NonAggregate for CacheString {}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<DB> QueryFragment<DB> for CacheString
where
    DB: Backend + HasSqlType<VarChar>,
{
    #[inline]
    fn walk_ast(&self, pass: AstPass<DB>) -> QueryResult<()> {
        self.0.walk_ast(pass)
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<ST, DB> FromSql<ST, DB> for CacheString
where
    DB: Backend,
    *const str: FromSql<ST, DB>,
{
    #[inline]
    fn from_sql(bytes: Option<&DB::RawValue>) -> deserialize::Result<Self> {
        Ok(CacheString(FromSql::from_sql(bytes)?))
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<ST, DB> FromSqlRow<ST, DB> for CacheString
where
    DB: Backend,
    *const str: FromSql<ST, DB>,
{
    const FIELDS_NEEDED: usize = 1;

    #[inline]
    fn build_from_row<T: Row<DB>>(row: &mut T) -> deserialize::Result<Self> {
        Ok(CacheString(FromSqlRow::build_from_row(row)?))
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "diesel-traits")))]
#[cfg(feature = "diesel-traits")]
impl<ST, DB> Queryable<ST, DB> for CacheString
where
    DB: Backend,
    *const str: FromSql<ST, DB>,
{
    type Row = Self;

    #[inline]
    fn build(row: Self::Row) -> Self {
        row
    }
}

#[cfg_attr(
    docs_rs_workaround,
    doc(cfg(all(feature = "diesel-traits", feature = "std")))
)]
#[cfg(all(feature = "diesel-traits", feature = "std"))]
impl<DB> ToSql<VarChar, DB> for CacheString
where
    DB: Backend,
{
    #[inline]
    fn to_sql<W: Write>(&self, out: &mut Output<W, DB>) -> serialize::Result {
        ToSql::to_sql(&self.0, out)
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "serde-traits")))]
#[cfg(feature = "serde-traits")]
impl Serialize for CacheString {
    #[inline]
    fn serialize<S: Serializer>(&self, ser: S) -> Result<S::Ok, S::Error> {
        self.0.serialize(ser)
    }
}

#[cfg_attr(docs_rs_workaround, doc(cfg(feature = "serde-traits")))]
#[cfg(feature = "serde-traits")]
impl<'a> Deserialize<'a> for CacheString {
    #[inline]
    fn deserialize<D: Deserializer<'a>>(des: D) -> Result<Self, D::Error> {
        Ok(CacheString(Deserialize::deserialize(des)?))
    }
}

#[cfg(test)]
mod tests {
    #![allow(proc_macro_derive_resolution_fallback)]
    #![allow(unused_import_braces)]

    use super::*;
    use crate::ArrayString;

    #[cfg(feature = "serde-traits")]
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct DeriveSerde(pub ArrayString<typenum::U8>);

    #[cfg(feature = "serde-traits")]
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct Derive2Serde(pub CacheString);

    #[test]
    #[cfg(feature = "serde-traits")]
    fn serde_derive_json() {
        let string =
            serde_json::to_string(&DeriveSerde(ArrayString::try_from_str("abcdefg").unwrap()))
                .unwrap();
        let s: DeriveSerde = serde_json::from_str(&string).unwrap();
        assert_eq!(
            s,
            DeriveSerde(ArrayString::try_from_str("abcdefg").unwrap())
        );
    }

    #[test]
    #[cfg(feature = "serde-traits")]
    fn serde_derive2_json() {
        let string = serde_json::to_string(&Derive2Serde(CacheString(
            ArrayString::try_from_str("abcdefg").unwrap(),
        )))
        .unwrap();
        let s: DeriveSerde = serde_json::from_str(&string).unwrap();
        assert_eq!(
            s,
            DeriveSerde(ArrayString::try_from_str("abcdefg").unwrap())
        );
    }

    #[test]
    #[cfg(feature = "serde-traits")]
    fn serde_json() {
        let string =
            serde_json::to_string(&ArrayString::<typenum::U8>::try_from_str("abcdefg").unwrap())
                .unwrap();
        let s: ArrayString<typenum::U8> = serde_json::from_str(&string).unwrap();
        assert_eq!(
            s,
            ArrayString::<typenum::U8>::try_from_str("abcdefg").unwrap()
        );
    }

    #[cfg(all(feature = "diesel-traits", feature = "std"))]
    use diesel::{debug_query, insert_into, mysql, pg, sqlite, update};

    #[cfg(all(feature = "diesel-traits", feature = "std"))]
    #[macro_use]
    table! {
        derives (name) {
            name -> VarChar,
        }
    }

    #[cfg(all(feature = "diesel-traits", feature = "std"))]
    #[derive(Queryable, Insertable, Clone, Debug)]
    #[table_name = "derives"]
    struct DeriveDiesel {
        pub name: ArrayString<typenum::U32>,
    }

    #[cfg(all(feature = "diesel-traits", feature = "std"))]
    #[derive(Queryable, Insertable, Clone, Debug)]
    #[table_name = "derives"]
    struct Derive2Diesel {
        pub name: CacheString,
    }

    #[cfg(all(feature = "diesel-traits", feature = "std"))]
    #[derive(Queryable, Insertable, Clone, Debug)]
    #[table_name = "derives"]
    struct Derive3Diesel<'a> {
        pub name: &'a str,
    }

    #[cfg(all(feature = "diesel-traits", feature = "std"))]
    #[test]
    fn diesel_derive_query_compare_insert() {
        let array = DeriveDiesel {
            name: ArrayString::try_from_str("Name1").unwrap(),
        };
        let cache = Derive2Diesel {
            name: CacheString(ArrayString::try_from_str("Name1").unwrap()),
        };
        let string = Derive3Diesel { name: "Name1" };

        let insert_array = insert_into(derives::table).values(&array);
        let insert_cache = insert_into(derives::table).values(&cache);
        let insert_string = insert_into(derives::table).values(&string);
        assert_eq!(
            debug_query::<pg::Pg, _>(&insert_array).to_string(),
            debug_query::<pg::Pg, _>(&insert_string).to_string()
        );
        assert_eq!(
            debug_query::<pg::Pg, _>(&insert_cache).to_string(),
            debug_query::<pg::Pg, _>(&insert_string).to_string()
        );
        assert_eq!(
            debug_query::<mysql::Mysql, _>(&insert_array).to_string(),
            debug_query::<mysql::Mysql, _>(&insert_string).to_string()
        );
        assert_eq!(
            debug_query::<mysql::Mysql, _>(&insert_cache).to_string(),
            debug_query::<mysql::Mysql, _>(&insert_string).to_string()
        );
        assert_eq!(
            debug_query::<sqlite::Sqlite, _>(&insert_array).to_string(),
            debug_query::<sqlite::Sqlite, _>(&insert_string).to_string()
        );
        assert_eq!(
            debug_query::<sqlite::Sqlite, _>(&insert_cache).to_string(),
            debug_query::<sqlite::Sqlite, _>(&insert_string).to_string()
        );
    }

    #[test]
    fn diesel_derive_query_compare_update() {
        let array = DeriveDiesel {
            name: ArrayString::try_from_str("Name1").unwrap(),
        };
        let cache = Derive2Diesel {
            name: CacheString(ArrayString::try_from_str("Name1").unwrap()),
        };
        let string = Derive3Diesel { name: "Name1" };
        let update_array = update(derives::table).set(derives::name.eq(&array.name));
        let update_cache = update(derives::table).set(derives::name.eq(&cache.name));
        let update_string = update(derives::table).set(derives::name.eq(&string.name));
        assert_eq!(
            debug_query::<pg::Pg, _>(&update_array).to_string(),
            debug_query::<pg::Pg, _>(&update_string).to_string()
        );
        assert_eq!(
            debug_query::<pg::Pg, _>(&update_cache).to_string(),
            debug_query::<pg::Pg, _>(&update_string).to_string()
        );
        assert_eq!(
            debug_query::<mysql::Mysql, _>(&update_array).to_string(),
            debug_query::<mysql::Mysql, _>(&update_string).to_string()
        );
        assert_eq!(
            debug_query::<mysql::Mysql, _>(&update_cache).to_string(),
            debug_query::<mysql::Mysql, _>(&update_string).to_string()
        );
        assert_eq!(
            debug_query::<sqlite::Sqlite, _>(&update_array).to_string(),
            debug_query::<sqlite::Sqlite, _>(&update_string).to_string()
        );
        assert_eq!(
            debug_query::<sqlite::Sqlite, _>(&update_cache).to_string(),
            debug_query::<sqlite::Sqlite, _>(&update_string).to_string()
        );
    }

    #[test]
    #[ignore]
    #[cfg(feature = "std")]
    fn diesel_select_query_compiles() {
        let conn = pg::PgConnection::establish("").unwrap();
        let select_array: Vec<DeriveDiesel> = derives::table
            .select(derives::all_columns)
            .load(&conn)
            .unwrap();
        let select_cache: Vec<Derive2Diesel> = derives::table
            .select(derives::all_columns)
            .load(&conn)
            .unwrap();
        assert_eq!(
            select_cache
                .into_iter()
                .map(|d| d.name.to_string())
                .collect::<Vec<_>>(),
            select_array
                .into_iter()
                .map(|d| d.name.to_string())
                .collect::<Vec<_>>()
        );
        let _: std::time::SystemTime = derives::table.select(dsl::now).first(&conn).unwrap();
        let _: std::time::SystemTime = derives::table.select(dsl::now).first(&conn).unwrap();

        let conn = mysql::MysqlConnection::establish("").unwrap();
        let select_array: Vec<DeriveDiesel> = derives::table
            .select(derives::all_columns)
            .load(&conn)
            .unwrap();
        let select_cache: Vec<Derive2Diesel> = derives::table
            .select(derives::all_columns)
            .load(&conn)
            .unwrap();
        assert_eq!(
            select_array
                .into_iter()
                .map(|d| d.name.to_string())
                .collect::<Vec<_>>(),
            select_cache
                .into_iter()
                .map(|d| d.name.to_string())
                .collect::<Vec<_>>()
        );
    }

    #[cfg(all(feature = "diesel-traits", feature = "std"))]
    #[test]
    fn diesel_derive_query_sqlite() {
        let conn = diesel::sqlite::SqliteConnection::establish(":memory:").unwrap();
        let _ = diesel::sql_query("CREATE TABLE derives (name VARCHAR(32));")
            .execute(&conn)
            .unwrap();
        let string = DeriveDiesel {
            name: ArrayString::try_from_str("Name1").unwrap(),
        };

        let _ = insert_into(derives::table)
            .values(&string)
            .execute(&conn)
            .unwrap();

        let queried: DeriveDiesel = derives::table.first(&conn).unwrap();
        assert_eq!(queried.name.as_str(), "Name1");
    }

    #[cfg(all(feature = "diesel-traits", feature = "std"))]
    #[test]
    fn diesel_derive2_query_sqlite() {
        let conn = diesel::sqlite::SqliteConnection::establish(":memory:").unwrap();
        let _ = diesel::sql_query("CREATE TABLE derives (name VARCHAR(32));")
            .execute(&conn)
            .unwrap();
        let string = Derive2Diesel {
            name: CacheString(ArrayString::try_from_str("Name1").unwrap()),
        };

        let _ = insert_into(derives::table)
            .values(&string)
            .execute(&conn)
            .unwrap();

        let queried: Derive2Diesel = derives::table.first(&conn).unwrap();
        assert_eq!(queried.name.as_str(), "Name1");
    }
}
