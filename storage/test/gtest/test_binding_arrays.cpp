/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "mozStorageHelper.h"

using namespace mozilla;

/**
 * This file tests binding and reading out array parameters through the
 * mozIStorageStatement API.
 */

TEST(storage_binding_arrays, Integers)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  (void)db->ExecuteSimpleSQL("CREATE TABLE test (val BLOB)"_ns);

  nsCOMPtr<mozIStorageStatement> insert, select;
  (void)db->CreateStatement(
      "INSERT INTO test (val) SELECT value FROM carray(?1)"_ns,
      getter_AddRefs(insert));
  (void)db->CreateStatement("SELECT val FROM test WHERE val IN carray(?1)"_ns,
                            getter_AddRefs(select));

  nsTArray<int64_t> inserted = {1, 2};
  {
    mozStorageStatementScoper scoper(insert);
    bool hasResult;
    do_check_true(
        NS_SUCCEEDED(insert->BindArrayOfIntegersByIndex(0, inserted)));
    do_check_true(NS_SUCCEEDED(insert->ExecuteStep(&hasResult)));
    do_check_false(hasResult);
  }

  nsAutoCString result;
  {
    mozStorageStatementScoper scoper(select);
    do_check_true(
        NS_SUCCEEDED(select->BindArrayOfIntegersByIndex(0, inserted)));
    bool hasResult;
    for (auto expected : inserted) {
      do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
      do_check_true(hasResult);
      int64_t result;
      do_check_true(NS_SUCCEEDED(select->GetInt64(0, &result)));
      do_check_true(result == expected);
    }
  }
}

TEST(storage_binding_arrays, Doubles)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  (void)db->ExecuteSimpleSQL("CREATE TABLE test (val BLOB)"_ns);

  nsCOMPtr<mozIStorageStatement> insert, select;
  (void)db->CreateStatement(
      "INSERT INTO test (val) SELECT value FROM carray(?1)"_ns,
      getter_AddRefs(insert));
  (void)db->CreateStatement("SELECT val FROM test WHERE val IN carray(?1)"_ns,
                            getter_AddRefs(select));

  nsTArray<double> inserted = {1.1, 2.2};
  {
    mozStorageStatementScoper scoper(insert);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(insert->BindArrayOfDoublesByIndex(0, inserted)));
    do_check_true(NS_SUCCEEDED(insert->ExecuteStep(&hasResult)));
    do_check_false(hasResult);
  }

  nsAutoCString result;
  {
    mozStorageStatementScoper scoper(select);
    do_check_true(NS_SUCCEEDED(select->BindArrayOfDoublesByIndex(0, inserted)));
    bool hasResult;
    for (auto expected : inserted) {
      do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
      do_check_true(hasResult);
      double result;
      do_check_true(NS_SUCCEEDED(select->GetDouble(0, &result)));
      do_check_true(result == expected);
    }
  }
}

TEST(storage_binding_arrays, UTF8Strings)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  (void)db->ExecuteSimpleSQL("CREATE TABLE test (val BLOB)"_ns);

  nsCOMPtr<mozIStorageStatement> insert, select;
  (void)db->CreateStatement(
      "INSERT INTO test (val) SELECT value FROM carray(?1)"_ns,
      getter_AddRefs(insert));
  (void)db->CreateStatement("SELECT val FROM test WHERE val IN carray(?1)"_ns,
                            getter_AddRefs(select));

  nsTArray<nsCString> inserted = {"test1"_ns, "test2"_ns};
  {
    mozStorageStatementScoper scoper(insert);
    bool hasResult;
    do_check_true(
        NS_SUCCEEDED(insert->BindArrayOfUTF8StringsByIndex(0, inserted)));
    do_check_true(NS_SUCCEEDED(insert->ExecuteStep(&hasResult)));
    do_check_false(hasResult);
  }

  nsAutoCString result;
  {
    mozStorageStatementScoper scoper(select);
    bool hasResult;
    do_check_true(
        NS_SUCCEEDED(select->BindArrayOfUTF8StringsByIndex(0, inserted)));
    for (const auto& expected : inserted) {
      do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
      do_check_true(hasResult);
      nsCString result;
      do_check_true(NS_SUCCEEDED(select->GetUTF8String(0, result)));
      do_check_true(result.Equals(expected));
    }
  }
}

TEST(storage_binding_arrays, AsyncStatement_BindingParamsArray)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  (void)db->ExecuteSimpleSQL("CREATE TABLE test (val BLOB)"_ns);

  nsCOMPtr<mozIStorageAsyncStatement> insert;
  (void)db->CreateAsyncStatement(
      "INSERT INTO test (val) SELECT value FROM carray(:values)"_ns,
      getter_AddRefs(insert));
  nsTArray<int64_t> insertedIntegers = {1, 2};
  nsTArray<nsCString> insertedStrings = {"test1"_ns, "test2"_ns};
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  insert->NewBindingParamsArray(getter_AddRefs(paramsArray));
  nsCOMPtr<mozIStorageBindingParams> intParams;
  paramsArray->NewBindingParams(getter_AddRefs(intParams));
  do_check_true(NS_SUCCEEDED(
      intParams->BindArrayOfIntegersByName("values"_ns, insertedIntegers)));
  do_check_true(NS_SUCCEEDED(paramsArray->AddParams(intParams)));
  intParams = nullptr;
  nsCOMPtr<mozIStorageBindingParams> strParams;
  paramsArray->NewBindingParams(getter_AddRefs(strParams));
  do_check_true(NS_SUCCEEDED(
      strParams->BindArrayOfUTF8StringsByName("values"_ns, insertedStrings)));
  do_check_true(NS_SUCCEEDED(paramsArray->AddParams(strParams)));
  strParams = nullptr;
  do_check_true(NS_SUCCEEDED(insert->BindParameters(paramsArray)));
  paramsArray = nullptr;
  blocking_async_execute(insert);
  insert->Finalize();

  nsCOMPtr<mozIStorageStatement> select;
  (void)db->CreateStatement("SELECT count(*) FROM test"_ns,
                            getter_AddRefs(select));
  bool hasResult;
  do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
  do_check_true(hasResult);
  do_check_true(select->AsInt64(0) == 4);
  select->Finalize();

  blocking_async_close(db);
}
