/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_UbiNodeTraverse_h
#define js_UbiNodeTraverse_h

#include "js/UbiNode.h"
#include "js/Utility.h"
#include "js/Vector.h"

namespace JS {
namespace ubi {

// A breadth-first traversal template for graphs of ubi::Nodes.
//
// No GC may occur while an instance of this template is live.
//
// The provided Handler type should have two members:
//
//   typename NodeData;
//
//      The value type of |BreadthFirst<Handler>::visited|, the HashMap of
//      ubi::Nodes that have been visited so far. Since the algorithm needs a
//      hash table like this for its own use anyway, it is simple to let
//      Handler store its own metadata about each node in the same table.
//
//      For example, if you want to find a shortest path to each node from any
//      traversal starting point, your |NodeData| type could record the first
//      edge to reach each node, and the node from which it originates. Then,
//      when the traversal is complete, you can walk backwards from any node
//      to some starting point, and the path recorded will be a shortest path.
//
//      This type must have a default constructor. If this type owns any other
//      resources, move constructors and assignment operators are probably a
//      good idea, too.
//
//   bool operator() (BreadthFirst &traversal,
//                    Node origin, const Edge &edge,
//                    Handler::NodeData *referentData, bool first);
//
//      The visitor function, called to report that we have traversed
//      |edge| from |origin|. This is called once for each edge we traverse.
//      As this is a breadth-first search, any prior calls to the visitor function
//      were for origin nodes not further from the start nodes than |origin|.
//
//      |traversal| is this traversal object, passed along for convenience.
//
//      |referentData| is a pointer to the value of the entry in
//      |traversal.visited| for |edge.referent|; the visitor function can
//      store whatever metadata it likes about |edge.referent| there.
//
//      |first| is true if this is the first time we have visited an edge
//      leading to |edge.referent|. This could be stored in NodeData, but
//      the algorithm knows whether it has just created the entry in
//      |traversal.visited|, so it passes it along for convenience.
//
//      The visitor function may call |traversal.stop()| if it doesn't want
//      to visit any more nodes.
//
//      The visitor function may consult |traversal.visited| for information
//      about other nodes, but it should not add or remove entries.
//
//      The visitor function should return true on success, or false if an
//      error occurs. A false return value terminates the traversal
//      immediately, and causes BreadthFirst<Handler>::traverse to return
//      false.
template<typename Handler>
struct BreadthFirst {

    // Construct a breadth-first traversal object that reports the nodes it
    // reaches to |handler|. The traversal object reports OOM on |cx|, and
    // asserts that no GC happens in |cx|'s runtime during its lifetime.
    //
    // We do nothing with noGC, other than require it to exist, with a lifetime
    // that encloses our own.
    BreadthFirst(JSContext *cx, Handler &handler, const JS::AutoCheckCannotGC &noGC)
      : cx(cx), visited(cx), handler(handler), pending(cx),
        traversalBegun(false), stopRequested(false)
    { }

    // Initialize this traversal object. Return false on OOM.
    bool init() { return visited.init(); }

    // Add |node| as a starting point for the traversal. You may add
    // as many starting points as you like. Return false on OOM.
    bool addStart(Node node) { return pending.append(node); }

    // Traverse the graph in breadth-first order, starting at the given
    // start nodes, applying |handler::operator()| for each edge traversed
    // as described above.
    //
    // This should be called only once per instance of this class.
    //
    // Return false on OOM or error return from |handler::operator()|.
    bool traverse()
    {
        MOZ_ASSERT(!traversalBegun);
        traversalBegun = true;

        // While there are pending nodes, visit them, until we've found a path to the target.
        while (!pending.empty()) {
            Node origin = pending.front();
            pending.popFront();

            // Get a range containing all origin's outgoing edges.
            js::ScopedJSDeletePtr<EdgeRange> range(origin.edges(cx));
            if (!range)
                return false;

            // Traverse each edge.
            for (; !range->empty(); range->popFront()) {
                MOZ_ASSERT(!stopRequested);

                const Edge &edge = range->front();
                typename NodeMap::AddPtr a = visited.lookupForAdd(edge.referent);
                bool first = !a;

                if (first) {
                    // This is the first time we've reached |edge.referent|.
                    // Create an entry for it in |visited|, and arrange to
                    // traverse its outgoing edges later.
                    if (!visited.add(a, edge.referent, typename Handler::NodeData()) ||
                        !pending.append(edge.referent)) {
                        return false;
                    }
                }

                MOZ_ASSERT(a);

                // Report this edge to the visitor function.
                if (!handler(*this, origin, edge, &a->value(), first))
                    return false;

                if (stopRequested)
                    return true;
            }
        }

        return true;
    }

    // Stop traversal, and return true from |traverse| without visiting any
    // more nodes. Only |handler::operator()| should call this function; it
    // may do so to stop the traversal early, without returning false and
    // then making |traverse|'s caller disambiguate that result from a real
    // error.
    void stop() { stopRequested = true; }

    // The context with which we were constructed.
    JSContext *cx;

    // A map associating each node N that we have reached with a
    // Handler::NodeData, for |handler|'s use. This is public, so that
    // |handler| can access it to see the traversal thus far.
    typedef js::HashMap<Node, typename Handler::NodeData> NodeMap;
    NodeMap visited;

  private:
    // Our handler object.
    Handler &handler;

    // A queue template. Appending and popping the front are constant time.
    // Wasted space is never more than some recent actual population plus the
    // current population.
    template <typename T>
    class Queue {
        js::Vector<T, 0> head, tail;
        size_t frontIndex;
      public:
        Queue(JSContext *cx) : head(cx), tail(cx), frontIndex(0) { }
        bool empty() { return frontIndex >= head.length(); }
        T &front() {
            MOZ_ASSERT(!empty());
            return head[frontIndex];
        }
        void popFront() {
            MOZ_ASSERT(!empty());
            frontIndex++;
            if (frontIndex >= head.length()) {
                head.clearAndFree();
                head.swap(tail);
                frontIndex = 0;
            }
        }
        bool append(const T &elt) {
            return frontIndex == 0 ? head.append(elt) : tail.append(elt);
        }
    };

    // A queue of nodes that we have reached, but whose outgoing edges we
    // have not yet traversed. Nodes reachable in fewer edges are enqueued
    // earlier.
    Queue<Node> pending;

    // True if our traverse function has been called.
    bool traversalBegun;

    // True if we've been asked to stop the traversal.
    bool stopRequested;
};

} // namespace ubi
} // namespace JS

#endif // js_UbiNodeTraverse.h
