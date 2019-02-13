/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBObjectStore
 */

dictionary IDBObjectStoreParameters {
    (DOMString or sequence<DOMString>)? keyPath = null;
    boolean                             autoIncrement = false;
};

[Exposed=(Window,Worker)]
interface IDBObjectStore {
    readonly    attribute DOMString      name;

    [Throws]
    readonly    attribute any            keyPath;

    readonly    attribute DOMStringList  indexNames;
    readonly    attribute IDBTransaction transaction;
    readonly    attribute boolean        autoIncrement;

    [Throws]
    IDBRequest put (any value, optional any key);

    [Throws]
    IDBRequest add (any value, optional any key);

    [Throws]
    IDBRequest delete (any key);

    [Throws]
    IDBRequest get (any key);

    [Throws]
    IDBRequest clear ();

    [Throws]
    IDBRequest openCursor (optional any range, optional IDBCursorDirection direction = "next");

    // Bug 899972
    // IDBIndex   createIndex (DOMString name, (DOMString or sequence<DOMString>) keyPath, optional IDBIndexParameters optionalParameters);

    [Throws]
    IDBIndex   createIndex (DOMString name, DOMString keyPath, optional IDBIndexParameters optionalParameters);

    [Throws]
    IDBIndex   createIndex (DOMString name, sequence<DOMString> keyPath, optional IDBIndexParameters optionalParameters);

    [Throws]
    IDBIndex   index (DOMString name);

    [Throws]
    void       deleteIndex (DOMString indexName);

    [Throws]
    IDBRequest count (optional any key);
};

partial interface IDBObjectStore {
    // Success fires IDBTransactionEvent, result == array of values for given keys
    [Throws]
    IDBRequest mozGetAll (optional any key, optional unsigned long limit);

    [Throws,
     Func="mozilla::dom::indexedDB::IndexedDatabaseManager::ExperimentalFeaturesEnabled"]
    IDBRequest getAll (optional any key, optional unsigned long limit);

    [Throws,
     Func="mozilla::dom::indexedDB::IndexedDatabaseManager::ExperimentalFeaturesEnabled"]
    IDBRequest getAllKeys (optional any key, optional unsigned long limit);

    [Throws,
     Func="mozilla::dom::indexedDB::IndexedDatabaseManager::ExperimentalFeaturesEnabled"]
    IDBRequest openKeyCursor (optional any range, optional IDBCursorDirection direction = "next");
};
