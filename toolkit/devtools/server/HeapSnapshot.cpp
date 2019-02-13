/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeapSnapshot.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "js/Debug.h"
#include "js/TypeDecls.h"
#include "js/UbiNodeTraverse.h"
#include "mozilla/Attributes.h"
#include "mozilla/devtools/AutoMemMap.h"
#include "mozilla/devtools/CoreDump.pb.h"
#include "mozilla/devtools/DeserializedNode.h"
#include "mozilla/devtools/ZeroCopyNSIOutputStream.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/HeapSnapshotBinding.h"
#include "mozilla/UniquePtr.h"

#include "jsapi.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCRTGlue.h"
#include "nsIOutputStream.h"
#include "nsISupportsImpl.h"
#include "nsNetUtil.h"
#include "prerror.h"
#include "prio.h"
#include "prtypes.h"

namespace mozilla {
namespace devtools {

using namespace JS;
using namespace dom;

using ::google::protobuf::io::ArrayInputStream;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::GzipInputStream;
using ::google::protobuf::io::ZeroCopyInputStream;

NS_IMPL_CYCLE_COLLECTION_CLASS(HeapSnapshot)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HeapSnapshot)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HeapSnapshot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(HeapSnapshot)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(HeapSnapshot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HeapSnapshot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HeapSnapshot)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* virtual */ JSObject*
HeapSnapshot::WrapObject(JSContext* aCx, HandleObject aGivenProto)
{
  return HeapSnapshotBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<HeapSnapshot>
HeapSnapshot::Create(JSContext* cx,
                     GlobalObject& global,
                     const uint8_t* buffer,
                     uint32_t size,
                     ErrorResult& rv)
{
  nsRefPtr<HeapSnapshot> snapshot = new HeapSnapshot(cx, global.GetAsSupports());
  if (!snapshot->init(buffer, size)) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  return snapshot.forget();
}

template<typename MessageType>
static bool
parseMessage(ZeroCopyInputStream& stream, MessageType& message)
{
  // We need to create a new `CodedInputStream` for each message so that the
  // 64MB limit is applied per-message rather than to the whole stream.
  CodedInputStream codedStream(&stream);

  // Because protobuf messages aren't self-delimiting, we serialize each message
  // preceeded by its size in bytes. When deserializing, we read this size and
  // then limit reading from the stream to the given byte size. If we didn't,
  // then the first message would consume the entire stream.

  uint32_t size = 0;
  if (NS_WARN_IF(!codedStream.ReadVarint32(&size)))
    return false;

  auto limit = codedStream.PushLimit(size);
  if (NS_WARN_IF(!message.ParseFromCodedStream(&codedStream)) ||
      NS_WARN_IF(!codedStream.ConsumedEntireMessage()))
  {
    return false;
  }

  codedStream.PopLimit(limit);
  return true;
}

bool
HeapSnapshot::saveNode(const protobuf::Node& node)
{
  if (!node.has_id())
    return false;
  NodeId id = node.id();

  if (!node.has_typename_())
    return false;

  const auto* duplicatedTypeName = reinterpret_cast<const char16_t*>(
    node.typename_().c_str());
  const char16_t* typeName = borrowUniqueString(
    duplicatedTypeName,
    node.typename_().length() / sizeof(char16_t));
  if (!typeName)
    return false;

  if (!node.has_size())
    return false;
  uint64_t size = node.size();

  auto edgesLength = node.edges_size();
  DeserializedNode::EdgeVector edges;
  if (!edges.reserve(edgesLength))
    return false;
  for (decltype(edgesLength) i = 0; i < edgesLength; i++) {
    DeserializedEdge edge;
    if (!edge.init(node.edges(i), *this))
      return false;
    edges.infallibleAppend(Move(edge));
  }

  DeserializedNode dn(id, typeName, size, Move(edges), *this);
  return nodes.putNew(id, Move(dn));
}

static inline bool
StreamHasData(GzipInputStream& stream)
{
  // Test for the end of the stream. The protobuf library gives no way to tell
  // the difference between an underlying read error and the stream being
  // done. All we can do is attempt to read data and extrapolate guestimations
  // from the result of that operation.

  const void* buf;
  int size;
  bool more = stream.Next(&buf, &size);
  if (!more)
    // Could not read any more data. We are optimistic and assume the stream is
    // just exhausted and there is not an underlying IO error, since this
    // function is only called at message boundaries.
    return false;

  // There is more data still available in the stream. Return the data we read
  // to the stream and let the parser get at it.
  stream.BackUp(size);
  return true;
}

bool
HeapSnapshot::init(const uint8_t* buffer, uint32_t size)
{
  if (!nodes.init() || !strings.init())
    return false;

  ArrayInputStream stream(buffer, size);
  GzipInputStream gzipStream(&stream);

  // First is the metadata.

  protobuf::Metadata metadata;
  if (!parseMessage(gzipStream, metadata))
    return false;
  if (metadata.has_timestamp())
    timestamp.emplace(metadata.timestamp());

  // Next is the root node.

  protobuf::Node root;
  if (!parseMessage(gzipStream, root))
    return false;

  // Although the id is optional in the protobuf format for future proofing, we
  // can't currently do anything without it.
  if (NS_WARN_IF(!root.has_id()))
    return false;
  rootId = root.id();

  if (NS_WARN_IF(!saveNode(root)))
    return false;

  // Finally, the rest of the nodes in the core dump.

  while (StreamHasData(gzipStream)) {
    protobuf::Node node;
    if (!parseMessage(gzipStream, node))
      return false;
    if (NS_WARN_IF(!saveNode(node)))
      return false;
  }

  return true;
}

const char16_t*
HeapSnapshot::borrowUniqueString(const char16_t* duplicateString, size_t length)
{
  MOZ_ASSERT(duplicateString);
  UniqueStringHashPolicy::Lookup lookup(duplicateString, length);
  auto ptr = strings.lookupForAdd(lookup);

  if (!ptr) {
    UniqueString owned(NS_strndup(duplicateString, length));
    if (!owned || !strings.add(ptr, Move(owned)))
      return nullptr;
  }

  MOZ_ASSERT(ptr->get() != duplicateString);
  return ptr->get();
}

// If we are only taking a snapshot of the heap affected by the given set of
// globals, find the set of zones the globals are allocated within. Returns
// false on OOM failure.
static bool
PopulateZonesWithGlobals(ZoneSet& zones, AutoObjectVector& globals)
{
  if (!zones.init())
    return false;

  unsigned length = globals.length();
  for (unsigned i = 0; i < length; i++) {
    if (!zones.put(GetObjectZone(globals[i])))
      return false;
  }

  return true;
}

// Add the given set of globals as explicit roots in the given roots
// list. Returns false on OOM failure.
static bool
AddGlobalsAsRoots(AutoObjectVector& globals, ubi::RootList& roots)
{
  unsigned length = globals.length();
  for (unsigned i = 0; i < length; i++) {
    if (!roots.addRoot(ubi::Node(globals[i].get()),
                       MOZ_UTF16("heap snapshot global")))
    {
      return false;
    }
  }
  return true;
}

// Choose roots and limits for a traversal, given `boundaries`. Set `roots` to
// the set of nodes within the boundaries that are referred to by nodes
// outside. If `boundaries` does not include all JS zones, initialize `zones` to
// the set of included zones; otherwise, leave `zones` uninitialized. (You can
// use zones.initialized() to check.)
//
// If `boundaries` is incoherent, or we encounter an error while trying to
// handle it, or we run out of memory, set `rv` appropriately and return
// `false`.
static bool
EstablishBoundaries(JSContext* cx,
                    ErrorResult& rv,
                    const HeapSnapshotBoundaries& boundaries,
                    ubi::RootList& roots,
                    ZoneSet& zones)
{
  MOZ_ASSERT(!roots.initialized());
  MOZ_ASSERT(!zones.initialized());

  bool foundBoundaryProperty = false;

  if (boundaries.mRuntime.WasPassed()) {
    foundBoundaryProperty = true;

    if (!boundaries.mRuntime.Value()) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    if (!roots.init()) {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (boundaries.mDebugger.WasPassed()) {
    if (foundBoundaryProperty) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }
    foundBoundaryProperty = true;

    JSObject* dbgObj = boundaries.mDebugger.Value();
    if (!dbgObj || !dbg::IsDebugger(*dbgObj)) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    AutoObjectVector globals(cx);
    if (!dbg::GetDebuggeeGlobals(cx, *dbgObj, globals) ||
        !PopulateZonesWithGlobals(zones, globals) ||
        !roots.init(zones) ||
        !AddGlobalsAsRoots(globals, roots))
    {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (boundaries.mGlobals.WasPassed()) {
    if (foundBoundaryProperty) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }
    foundBoundaryProperty = true;

    uint32_t length = boundaries.mGlobals.Value().Length();
    if (length == 0) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    AutoObjectVector globals(cx);
    for (uint32_t i = 0; i < length; i++) {
      JSObject* global = boundaries.mGlobals.Value().ElementAt(i);
      if (!JS_IsGlobalObject(global)) {
        rv.Throw(NS_ERROR_INVALID_ARG);
        return false;
      }
      if (!globals.append(global)) {
        rv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return false;
      }
    }

    if (!PopulateZonesWithGlobals(zones, globals) ||
        !roots.init(zones) ||
        !AddGlobalsAsRoots(globals, roots))
    {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (!foundBoundaryProperty) {
    rv.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  MOZ_ASSERT(roots.initialized());
  MOZ_ASSERT_IF(boundaries.mDebugger.WasPassed(), zones.initialized());
  MOZ_ASSERT_IF(boundaries.mGlobals.WasPassed(), zones.initialized());
  return true;
}


// A `CoreDumpWriter` that serializes nodes to protobufs and writes them to the
// given `ZeroCopyOutputStream`.
class MOZ_STACK_CLASS StreamWriter : public CoreDumpWriter
{
  JSContext* cx;
  bool      wantNames;

  ::google::protobuf::io::ZeroCopyOutputStream& stream;

  bool writeMessage(const ::google::protobuf::MessageLite& message) {
    // We have to create a new CodedOutputStream when writing each message so
    // that the 64MB size limit used by Coded{Output,Input}Stream to prevent
    // integer overflow is enforced per message rather than on the whole stream.
    ::google::protobuf::io::CodedOutputStream codedStream(&stream);
    codedStream.WriteVarint32(message.ByteSize());
    message.SerializeWithCachedSizes(&codedStream);
    return !codedStream.HadError();
  }

public:
  StreamWriter(JSContext* cx,
               ::google::protobuf::io::ZeroCopyOutputStream& stream,
               bool wantNames)
    : cx(cx)
    , wantNames(wantNames)
    , stream(stream)
  { }

  ~StreamWriter() override { }

  virtual bool writeMetadata(uint64_t timestamp) override {
    protobuf::Metadata metadata;
    metadata.set_timestamp(timestamp);
    return writeMessage(metadata);
  }

  virtual bool writeNode(const JS::ubi::Node& ubiNode,
                         EdgePolicy includeEdges) override {
    protobuf::Node protobufNode;
    protobufNode.set_id(ubiNode.identifier());

    const char16_t* typeName = ubiNode.typeName();
    size_t length = NS_strlen(typeName) * sizeof(char16_t);
    protobufNode.set_typename_(typeName, length);

    JSRuntime* rt = JS_GetRuntime(cx);
    mozilla::MallocSizeOf mallocSizeOf = dbg::GetDebuggerMallocSizeOf(rt);
    MOZ_ASSERT(mallocSizeOf);
    protobufNode.set_size(ubiNode.size(mallocSizeOf));

    if (includeEdges) {
      auto edges = ubiNode.edges(cx, wantNames);
      if (NS_WARN_IF(!edges))
        return false;

      for ( ; !edges->empty(); edges->popFront()) {
        const ubi::Edge& ubiEdge = edges->front();

        protobuf::Edge* protobufEdge = protobufNode.add_edges();
        if (NS_WARN_IF(!protobufEdge)) {
          return false;
        }

        protobufEdge->set_referent(ubiEdge.referent.identifier());

        if (wantNames && ubiEdge.name) {
          size_t length = NS_strlen(ubiEdge.name) * sizeof(char16_t);
          protobufEdge->set_name(ubiEdge.name, length);
        }
      }
    }

    return writeMessage(protobufNode);
  }
};

// A JS::ubi::BreadthFirst handler that serializes a snapshot of the heap into a
// core dump.
class MOZ_STACK_CLASS HeapSnapshotHandler
{
  CoreDumpWriter& writer;
  JS::ZoneSet*    zones;

public:
  HeapSnapshotHandler(CoreDumpWriter& writer,
                      JS::ZoneSet* zones)
    : writer(writer),
      zones(zones)
  { }

  // JS::ubi::BreadthFirst handler interface.

  class NodeData { };
  typedef JS::ubi::BreadthFirst<HeapSnapshotHandler> Traversal;
  bool operator() (Traversal& traversal,
                   JS::ubi::Node origin,
                   const JS::ubi::Edge& edge,
                   NodeData*,
                   bool first)
  {
    // We're only interested in the first time we reach edge.referent, not in
    // every edge arriving at that node. "But, don't we want to serialize every
    // edge in the heap graph?" you ask. Don't worry! This edge is still
    // serialized into the core dump. Serializing a node also serializes each of
    // its edges, and if we are traversing a given edge, we must have already
    // visited and serialized the origin node and its edges.
    if (!first)
      return true;

    const JS::ubi::Node& referent = edge.referent;

    if (!zones)
      // We aren't targeting a particular set of zones, so serialize all the
      // things!
      return writer.writeNode(referent, CoreDumpWriter::INCLUDE_EDGES);

    // We are targeting a particular set of zones. If this node is in our target
    // set, serialize it and all of its edges. If this node is _not_ in our
    // target set, we also serialize under the assumption that it is a shared
    // resource being used by something in our target zones since we reached it
    // by traversing the heap graph. However, we do not serialize its outgoing
    // edges and we abandon further traversal from this node.

    JS::Zone* zone = referent.zone();

    if (zones->has(zone))
      return writer.writeNode(referent, CoreDumpWriter::INCLUDE_EDGES);

    traversal.abandonReferent();
    return writer.writeNode(referent, CoreDumpWriter::EXCLUDE_EDGES);
  }
};


bool
WriteHeapGraph(JSContext* cx,
               const JS::ubi::Node& node,
               CoreDumpWriter& writer,
               bool wantNames,
               JS::ZoneSet* zones,
               JS::AutoCheckCannotGC& noGC)
{
  // Serialize the starting node to the core dump.

  if (NS_WARN_IF(!writer.writeNode(node, CoreDumpWriter::INCLUDE_EDGES))) {
    return false;
  }

  // Walk the heap graph starting from the given node and serialize it into the
  // core dump.

  HeapSnapshotHandler handler(writer, zones);
  HeapSnapshotHandler::Traversal traversal(cx, handler, noGC);
  if (!traversal.init())
    return false;
  traversal.wantNames = wantNames;

  return traversal.addStartVisited(node) &&
    traversal.traverse();
}

} // namespace devtools

namespace dom {

using namespace JS;
using namespace devtools;

/* static */ void
ThreadSafeChromeUtils::SaveHeapSnapshot(GlobalObject& global,
                                        JSContext* cx,
                                        const nsAString& filePath,
                                        const HeapSnapshotBoundaries& boundaries,
                                        ErrorResult& rv)
{
  bool wantNames = true;
  ZoneSet zones;
  Maybe<AutoCheckCannotGC> maybeNoGC;
  ubi::RootList rootList(cx, maybeNoGC, wantNames);
  if (!EstablishBoundaries(cx, rv, boundaries, rootList, zones))
    return;

  MOZ_ASSERT(maybeNoGC.isSome());
  ubi::Node roots(&rootList);

  nsCOMPtr<nsIFile> file;
  rv = NS_NewLocalFile(filePath, false, getter_AddRefs(file));
  if (NS_WARN_IF(rv.Failed()))
    return;

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), file,
                                   PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                   -1, 0);
  if (NS_WARN_IF(rv.Failed()))
    return;

  ZeroCopyNSIOutputStream zeroCopyStream(outputStream);
  ::google::protobuf::io::GzipOutputStream gzipStream(&zeroCopyStream);

  StreamWriter writer(cx, gzipStream, wantNames);

  // Serialize the initial heap snapshot metadata to the core dump.
  if (!writer.writeMetadata(PR_Now()) ||
      // Serialize the heap graph to the core dump, starting from our list of
      // roots.
      !WriteHeapGraph(cx,
                      roots,
                      writer,
                      wantNames,
                      zones.initialized() ? &zones : nullptr,
                      maybeNoGC.ref()))
    {
      rv.Throw(zeroCopyStream.failed()
               ? zeroCopyStream.result()
               : NS_ERROR_UNEXPECTED);
      return;
    }
}

/* static */ already_AddRefed<HeapSnapshot>
ThreadSafeChromeUtils::ReadHeapSnapshot(GlobalObject& global,
                                        JSContext* cx,
                                        const nsAString& filePath,
                                        ErrorResult& rv)
{
  UniquePtr<char[]> path(ToNewCString(filePath));
  if (!path) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  AutoMemMap mm;
  rv = mm.init(path.get());
  if (rv.Failed())
    return nullptr;

  return HeapSnapshot::Create(cx, global,
                              reinterpret_cast<const uint8_t*>(mm.address()),
                              mm.size(), rv);
}

} // namespace dom
} // namespace mozilla
