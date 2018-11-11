//! `StableGraph` keeps indices stable across removals.
//!
//! Depends on `feature = "stable_graph"`.
//!

use std::cmp;
use std::fmt;
use std::iter;
use std::marker::PhantomData;
use std::mem::replace;
use std::mem::size_of;
use std::ops::{Index, IndexMut};
use std::slice;

use {
    Graph,
    EdgeType,
    Directed,
    Undirected,
    Direction,
    Incoming,
    Outgoing,
};

use iter_format::{
    IterFormatExt,
    NoPretty,
    DebugMap,
};
use iter_utils::IterUtilsExt;

use super::{
    Edge,
    index_twice,
    Node,
    DIRECTIONS,
    Pair,
    Frozen,
};
use IntoWeightedEdge;
use visit::{
    EdgeRef,
    IntoNodeReferences,
    IntoEdges,
    IntoEdgesDirected,
    IntoEdgeReferences,
    NodeIndexable,
};

// reexport those things that are shared with Graph
#[doc(no_inline)]
pub use graph::{
    NodeIndex,
    EdgeIndex,
    GraphIndex,
    IndexType,
    DefaultIx,
    node_index,
    edge_index,
};

use util::enumerate;

#[cfg(feature = "serde-1")]
mod serialization;

/// `StableGraph<N, E, Ty, Ix>` is a graph datastructure using an adjacency
/// list representation.
///
/// The graph **does not invalidate** any unrelated node or edge indices when
/// items are removed.
///
/// `StableGraph` is parameterized over:
///
/// - Associated data `N` for nodes and `E` for edges, also called *weights*.
///   The associated data can be of arbitrary type.
/// - Edge type `Ty` that determines whether the graph edges are directed or undirected.
/// - Index type `Ix`, which determines the maximum size of the graph.
///
/// The graph uses **O(|V| + |E|)** space, and allows fast node and edge insert
/// and efficient graph search.
///
/// It implements **O(e')** edge lookup and edge and node removals, where **e'**
/// is some local measure of edge count.
///
/// - Nodes and edges are each numbered in an interval from *0* to some number
/// *m*, but *not all* indices in the range are valid, since gaps are formed
/// by deletions.
///
/// - You can select graph index integer type after the size of the graph. A smaller
/// size may have better performance.
///
/// - Using indices allows mutation while traversing the graph, see `Dfs`.
///
/// - The `StableGraph` is a regular rust collection and is `Send` and `Sync`
/// (as long as associated data `N` and `E` are).
///
/// - Indices don't allow as much compile time checking as references.
///
/// Depends on crate feature `stable_graph` (default). *Stable Graph is still
/// missing a few methods compared to Graph. You can contribute to help it
/// achieve parity.*
pub struct StableGraph<N, E, Ty = Directed, Ix = DefaultIx>
{
    g: Graph<Option<N>, Option<E>, Ty, Ix>,
    node_count: usize,
    edge_count: usize,

    // node and edge free lists (both work the same way)
    //
    // free_node, if not NodeIndex::end(), points to a node index
    // that is vacant (after a deletion).  The next item in the list is kept in
    // that Node's Node.next[0] field. For Node, it's a node index stored
    // in an EdgeIndex location, and the _into_edge()/_into_node() methods
    // convert.
    free_node: NodeIndex<Ix>,
    free_edge: EdgeIndex<Ix>,
}

/// A `StableGraph` with directed edges.
///
/// For example, an edge from *1* to *2* is distinct from an edge from *2* to
/// *1*.
pub type StableDiGraph<N, E, Ix = DefaultIx> = StableGraph<N, E, Directed, Ix>;

/// A `StableGraph` with undirected edges.
///
/// For example, an edge between *1* and *2* is equivalent to an edge between
/// *2* and *1*.
pub type StableUnGraph<N, E, Ix = DefaultIx> = StableGraph<N, E, Undirected, Ix>;

impl<N, E, Ty, Ix> fmt::Debug for StableGraph<N, E, Ty, Ix>
    where N: fmt::Debug,
          E: fmt::Debug,
          Ty: EdgeType,
          Ix: IndexType,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let etype = if self.is_directed() { "Directed" } else { "Undirected" };
        let mut fmt_struct = f.debug_struct("StableGraph");
        fmt_struct.field("Ty", &etype);
        fmt_struct.field("node_count", &self.node_count);
        fmt_struct.field("edge_count", &self.edge_count);
        if self.g.edges.iter().any(|e| e.weight.is_some()) {
            fmt_struct.field("edges", &self.g.edges.iter()
                .filter(|e| e.weight.is_some())
                .map(|e| NoPretty((e.source().index(), e.target().index())))
                .format(", "));
        }
        // skip weights if they are ZST!
        if size_of::<N>() != 0 {
            fmt_struct.field("node weights",
            &DebugMap(||
                self.g.nodes.iter()
                    .map(|n| n.weight.as_ref())
                    .enumerate()
                    .filter_map(|(i, wo)| wo.map(move |w| (i, w)))
                   ));
        }
        if size_of::<E>() != 0 {
            fmt_struct.field("edge weights",
            &DebugMap(||
                self.g.edges.iter()
                    .map(|n| n.weight.as_ref())
                    .enumerate()
                    .filter_map(|(i, wo)| wo.map(move |w| (i, w)))
                   ));
        }
        fmt_struct.field("free_node", &self.free_node);
        fmt_struct.field("free_edge", &self.free_edge);
        fmt_struct.finish()
    }
}


impl<N, E> StableGraph<N, E, Directed> {
    /// Create a new `StableGraph` with directed edges.
    ///
    /// This is a convenience method. See `StableGraph::with_capacity`
    /// or `StableGraph::default` for a constructor that is generic in all the
    /// type parameters of `StableGraph`.
    pub fn new() -> Self {
        Self::with_capacity(0, 0)
    }
}

impl<N, E, Ty, Ix> StableGraph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    /// Create a new `StableGraph` with estimated capacity.
    pub fn with_capacity(nodes: usize, edges: usize) -> Self {
        StableGraph {
            g: Graph::with_capacity(nodes, edges),
            node_count: 0,
            edge_count: 0,
            free_node: NodeIndex::end(),
            free_edge: EdgeIndex::end(),
        }
    }

    /// Return the current node and edge capacity of the graph.
    pub fn capacity(&self) -> (usize, usize) {
        self.g.capacity()
    }

    /// Remove all nodes and edges
    pub fn clear(&mut self) {
        self.node_count = 0;
        self.edge_count = 0;
        self.free_node = NodeIndex::end();
        self.free_edge = EdgeIndex::end();
        self.g.clear();
    }

    /// Remove all edges
    pub fn clear_edges(&mut self) {
        self.edge_count = 0;
        self.free_edge = EdgeIndex::end();
        self.g.edges.clear();
        // clear edges without touching the free list
        for node in &mut self.g.nodes {
            if let Some(_) = node.weight {
                node.next = [EdgeIndex::end(), EdgeIndex::end()];
            }
        }
    }

    /// Return the number of nodes (vertices) in the graph.
    ///
    /// Computes in **O(1)** time.
    pub fn node_count(&self) -> usize {
        self.node_count
    }

    /// Return the number of edges in the graph.
    ///
    /// Computes in **O(1)** time.
    pub fn edge_count(&self) -> usize {
        self.edge_count
    }

    /// Whether the graph has directed edges or not.
    #[inline]
    pub fn is_directed(&self) -> bool {
        Ty::is_directed()
    }

    /// Add a node (also called vertex) with associated data `weight` to the graph.
    ///
    /// Computes in **O(1)** time.
    ///
    /// Return the index of the new node.
    ///
    /// **Panics** if the `StableGraph` is at the maximum number of nodes for
    /// its index type.
    pub fn add_node(&mut self, weight: N) -> NodeIndex<Ix> {
        let index = if self.free_node != NodeIndex::end() {
            let node_idx = self.free_node;
            let node_slot = &mut self.g.nodes[node_idx.index()];
            let _old = replace(&mut node_slot.weight, Some(weight));
            debug_assert!(_old.is_none());
            self.free_node = node_slot.next[0]._into_node();
            node_slot.next[0] = EdgeIndex::end();
            node_idx
        } else {
            self.g.add_node(Some(weight))
        };
        self.node_count += 1;
        index
    }

    /// free_node: Which free list to update for the vacancy
    fn add_vacant_node(&mut self, free_node: &mut NodeIndex<Ix>) {
        let node_idx = self.g.add_node(None);
        // link the free list
        let node_slot = &mut self.g.nodes[node_idx.index()];
        node_slot.next[0] = free_node._into_edge();
        *free_node = node_idx;
    }

    /// Remove `a` from the graph if it exists, and return its weight.
    /// If it doesn't exist in the graph, return `None`.
    ///
    /// The node index `a` is invalidated, but none other.
    /// Edge indices are invalidated as they would be following the removal of
    /// each edge with an endpoint in `a`.
    ///
    /// Computes in **O(e')** time, where **e'** is the number of affected
    /// edges, including *n* calls to `.remove_edge()` where *n* is the number
    /// of edges with an endpoint in `a`.
    pub fn remove_node(&mut self, a: NodeIndex<Ix>) -> Option<N> {
        let node_weight = match self.g.nodes.get_mut(a.index()) {
            None => return None,
            Some(n) => n.weight.take(),
        };
        if let None = node_weight {
            return None;
        }
        for d in &DIRECTIONS {
            let k = d.index();

            // Remove all edges from and to this node.
            loop {
                let next = self.g.nodes[a.index()].next[k];
                if next == EdgeIndex::end() {
                    break
                }
                let ret = self.remove_edge(next);
                debug_assert!(ret.is_some());
                let _ = ret;
            }
        }

        let node_slot = &mut self.g.nodes[a.index()];
        //let node_weight = replace(&mut self.g.nodes[a.index()].weight, Entry::Empty(self.free_node));
        //self.g.nodes[a.index()].next = [EdgeIndex::end(), EdgeIndex::end()];
        node_slot.next = [self.free_node._into_edge(), EdgeIndex::end()];
        self.free_node = a;
        self.node_count -= 1;

        node_weight
    }

    pub fn contains_node(&self, a: NodeIndex<Ix>) -> bool {
        self.get_node(a).is_some()
    }

    // Return the Node if it is not vacant (non-None weight)
    fn get_node(&self, a: NodeIndex<Ix>) -> Option<&Node<Option<N>, Ix>> {
        self.g.nodes.get(a.index())
                    .and_then(|node| node.weight.as_ref().map(move |_| node))
    }

    /// Add an edge from `a` to `b` to the graph, with its associated
    /// data `weight`.
    ///
    /// Return the index of the new edge.
    ///
    /// Computes in **O(1)** time.
    ///
    /// **Panics** if any of the nodes don't exist.<br>
    /// **Panics** if the `StableGraph` is at the maximum number of edges for
    /// its index type.
    ///
    /// **Note:** `StableGraph` allows adding parallel (“duplicate”) edges.
    pub fn add_edge(&mut self, a: NodeIndex<Ix>, b: NodeIndex<Ix>, weight: E)
        -> EdgeIndex<Ix>
    {
        let edge_idx;
        let mut new_edge = None::<Edge<_, _>>;
        {
            let edge: &mut Edge<_, _>;

            if self.free_edge != EdgeIndex::end() {
                edge_idx = self.free_edge;
                edge = &mut self.g.edges[edge_idx.index()];
                let _old = replace(&mut edge.weight, Some(weight));
                debug_assert!(_old.is_none());
                self.free_edge = edge.next[0];
                edge.node = [a, b];
            } else {
                edge_idx = EdgeIndex::new(self.g.edges.len());
                assert!(<Ix as IndexType>::max().index() == !0 || EdgeIndex::end() != edge_idx);
                new_edge = Some(Edge {
                    weight: Some(weight),
                    node: [a, b],
                    next: [EdgeIndex::end(); 2],
                });
                edge = new_edge.as_mut().unwrap();
            }

            let wrong_index = match index_twice(&mut self.g.nodes, a.index(), b.index()) {
                Pair::None => Some(cmp::max(a.index(), b.index())),
                Pair::One(an) => {
                    if an.weight.is_none() {
                        Some(a.index())
                    } else {
                        edge.next = an.next;
                        an.next[0] = edge_idx;
                        an.next[1] = edge_idx;
                        None
                    }
                }
                Pair::Both(an, bn) => {
                    // a and b are different indices
                    if an.weight.is_none() {
                        Some(a.index())
                    } else if bn.weight.is_none() {
                        Some(b.index())
                    } else {
                        edge.next = [an.next[0], bn.next[1]];
                        an.next[0] = edge_idx;
                        bn.next[1] = edge_idx;
                        None
                    }
                }
            };
            if let Some(i) = wrong_index {
                panic!("StableGraph::add_edge: node index {} is not a node in the graph", i);
            }
            self.edge_count += 1;
        }
        if let Some(edge) = new_edge {
            self.g.edges.push(edge);
        }
        edge_idx
    }

    /// free_edge: Which free list to update for the vacancy
    fn add_vacant_edge(&mut self, free_edge: &mut EdgeIndex<Ix>) {
        let edge_idx = EdgeIndex::new(self.g.edges.len());
        debug_assert!(edge_idx != EdgeIndex::end());
        let mut edge = Edge {
            weight: None,
            node: [NodeIndex::end(); 2],
            next: [EdgeIndex::end(); 2],
        };
        edge.next[0] = *free_edge;
        *free_edge = edge_idx;
        self.g.edges.push(edge);
    }

    /// Add or update an edge from `a` to `b`.
    /// If the edge already exists, its weight is updated.
    ///
    /// Return the index of the affected edge.
    ///
    /// Computes in **O(e')** time, where **e'** is the number of edges
    /// connected to `a` (and `b`, if the graph edges are undirected).
    ///
    /// **Panics** if any of the nodes don't exist.
    pub fn update_edge(&mut self, a: NodeIndex<Ix>, b: NodeIndex<Ix>, weight: E)
        -> EdgeIndex<Ix>
    {
        if let Some(ix) = self.find_edge(a, b) {
            self[ix] = weight;
            return ix;
        }
        self.add_edge(a, b, weight)
    }

    /// Remove an edge and return its edge weight, or `None` if it didn't exist.
    ///
    /// Invalidates the edge index `e` but no other.
    ///
    /// Computes in **O(e')** time, where **e'** is the number of edges
    /// conneced to the same endpoints as `e`.
    pub fn remove_edge(&mut self, e: EdgeIndex<Ix>) -> Option<E> {
        // every edge is part of two lists,
        // outgoing and incoming edges.
        // Remove it from both
        let (is_edge, edge_node, edge_next) = match self.g.edges.get(e.index()) {
            None => return None,
            Some(x) => (x.weight.is_some(), x.node, x.next),
        };
        if !is_edge {
            return None;
        }

        // Remove the edge from its in and out lists by replacing it with
        // a link to the next in the list.
        self.g.change_edge_links(edge_node, e, edge_next);

        // Clear the edge and put it in the free list
        let edge = &mut self.g.edges[e.index()];
        edge.next = [self.free_edge, EdgeIndex::end()];
        edge.node = [NodeIndex::end(), NodeIndex::end()];
        self.free_edge = e;
        self.edge_count -= 1;
        edge.weight.take()
    }

    /// Access the weight for node `a`.
    ///
    /// Also available with indexing syntax: `&graph[a]`.
    pub fn node_weight(&self, a: NodeIndex<Ix>) -> Option<&N> {
        match self.g.nodes.get(a.index()) {
            Some(no) => no.weight.as_ref(),
            None => None,
        }
    }

    /// Access the weight for node `a`, mutably.
    ///
    /// Also available with indexing syntax: `&mut graph[a]`.
    pub fn node_weight_mut(&mut self, a: NodeIndex<Ix>) -> Option<&mut N> {
        match self.g.nodes.get_mut(a.index()) {
            Some(no) => no.weight.as_mut(),
            None => None,
        }
    }

    /// Return an iterator over the node indices of the graph
    pub fn node_indices(&self) -> NodeIndices<N, Ix> {
        NodeIndices {
            iter: enumerate(self.raw_nodes())
        }
    }

    /// Access the weight for edge `e`.
    ///
    /// Also available with indexing syntax: `&graph[e]`.
    pub fn edge_weight(&self, e: EdgeIndex<Ix>) -> Option<&E> {
        match self.g.edges.get(e.index()) {
            Some(ed) => ed.weight.as_ref(),
            None => None,
        }
    }

    /// Access the weight for edge `e`, mutably
    ///
    /// Also available with indexing syntax: `&mut graph[e]`.
    pub fn edge_weight_mut(&mut self, e: EdgeIndex<Ix>) -> Option<&mut E> {
        match self.g.edges.get_mut(e.index()) {
            Some(ed) => ed.weight.as_mut(),
            None => None,
        }
    }

    /// Access the source and target nodes for `e`.
    pub fn edge_endpoints(&self, e: EdgeIndex<Ix>)
        -> Option<(NodeIndex<Ix>, NodeIndex<Ix>)>
    {
        match self.g.edges.get(e.index()) {
            Some(ed) if ed.weight.is_some() => Some((ed.source(), ed.target())),
            _otherwise => None,
        }
    }

    /// Return an iterator over the node indices of the graph
    pub fn edge_indices(&self) -> EdgeIndices<E, Ix> {
        EdgeIndices {
            iter: enumerate(self.raw_edges())
        }
    }

    /// Lookup an edge from `a` to `b`.
    ///
    /// Computes in **O(e')** time, where **e'** is the number of edges
    /// connected to `a` (and `b`, if the graph edges are undirected).
    pub fn find_edge(&self, a: NodeIndex<Ix>, b: NodeIndex<Ix>) -> Option<EdgeIndex<Ix>>
    {
        if !self.is_directed() {
            self.find_edge_undirected(a, b).map(|(ix, _)| ix)
        } else {
            match self.get_node(a) {
                None => None,
                Some(node) => self.g.find_edge_directed_from_node(node, b)
            }
        }
    }

    /// Lookup an edge between `a` and `b`, in either direction.
    ///
    /// If the graph is undirected, then this is equivalent to `.find_edge()`.
    ///
    /// Return the edge index and its directionality, with `Outgoing` meaning
    /// from `a` to `b` and `Incoming` the reverse,
    /// or `None` if the edge does not exist.
    pub fn find_edge_undirected(&self, a: NodeIndex<Ix>, b: NodeIndex<Ix>) -> Option<(EdgeIndex<Ix>, Direction)>
    {
        match self.get_node(a) {
            None => None,
            Some(node) => self.g.find_edge_undirected_from_node(node, b),
        }
    }


    /// Return an iterator of all nodes with an edge starting from `a`.
    ///
    /// - `Directed`: Outgoing edges from `a`.
    /// - `Undirected`: All edges connected to `a`.
    ///
    /// Produces an empty iterator if the node doesn't exist.<br>
    /// Iterator element type is `NodeIndex<Ix>`.
    ///
    /// Use [`.neighbors(a).detach()`][1] to get a neighbor walker that does
    /// not borrow from the graph.
    ///
    /// [1]: struct.Neighbors.html#method.detach
    pub fn neighbors(&self, a: NodeIndex<Ix>) -> Neighbors<E, Ix> {
        self.neighbors_directed(a, Outgoing)
    }

    /// Return an iterator of all neighbors that have an edge between them and `a`,
    /// in the specified direction.
    /// If the graph's edges are undirected, this is equivalent to *.neighbors(a)*.
    ///
    /// - `Directed`, `Outgoing`: All edges from `a`.
    /// - `Directed`, `Incoming`: All edges to `a`.
    /// - `Undirected`: All edges connected to `a`.
    ///
    /// Produces an empty iterator if the node doesn't exist.<br>
    /// Iterator element type is `NodeIndex<Ix>`.
    ///
    /// Use [`.neighbors_directed(a, dir).detach()`][1] to get a neighbor walker that does
    /// not borrow from the graph.
    ///
    /// [1]: struct.Neighbors.html#method.detach
    pub fn neighbors_directed(&self, a: NodeIndex<Ix>, dir: Direction)
        -> Neighbors<E, Ix>
    {
        let mut iter = self.neighbors_undirected(a);
        if self.is_directed() {
            let k = dir.index();
            iter.next[1 - k] = EdgeIndex::end();
            iter.skip_start = NodeIndex::end();
        }
        iter
    }

    /// Return an iterator of all neighbors that have an edge between them and `a`,
    /// in either direction.
    /// If the graph's edges are undirected, this is equivalent to *.neighbors(a)*.
    ///
    /// - `Directed` and `Undirected`: All edges connected to `a`.
    ///
    /// Produces an empty iterator if the node doesn't exist.<br>
    /// Iterator element type is `NodeIndex<Ix>`.
    ///
    /// Use [`.neighbors_undirected(a).detach()`][1] to get a neighbor walker that does
    /// not borrow from the graph.
    ///
    /// [1]: struct.Neighbors.html#method.detach
    pub fn neighbors_undirected(&self, a: NodeIndex<Ix>) -> Neighbors<E, Ix>
    {
        Neighbors {
            skip_start: a,
            edges: &self.g.edges,
            next: match self.get_node(a) {
                None => [EdgeIndex::end(), EdgeIndex::end()],
                Some(n) => n.next,
            }
        }
    }

    /// Return an iterator of all edges of `a`.
    ///
    /// - `Directed`: Outgoing edges from `a`.
    /// - `Undirected`: All edges connected to `a`.
    ///
    /// Produces an empty iterator if the node doesn't exist.<br>
    /// Iterator element type is `EdgeReference<E, Ix>`.
    pub fn edges(&self, a: NodeIndex<Ix>) -> Edges<E, Ty, Ix> {
        self.edges_directed(a, Outgoing)
    }

    /// Return an iterator of all edges of `a`, in the specified direction.
    ///
    /// - `Directed`, `Outgoing`: All edges from `a`.
    /// - `Directed`, `Incoming`: All edges to `a`.
    /// - `Undirected`: All edges connected to `a`.
    ///
    /// Produces an empty iterator if the node `a` doesn't exist.<br>
    /// Iterator element type is `EdgeReference<E, Ix>`.
    pub fn edges_directed(&self, a: NodeIndex<Ix>, dir: Direction) -> Edges<E, Ty, Ix>
    {
        let mut iter = self.edges_undirected(a);
        if self.is_directed() {
            iter.direction = Some(dir);
        }
        if self.is_directed() && dir == Incoming {
            iter.next.swap(0, 1);
        }
        iter
    }

    /// Return an iterator over all edges connected to `a`.
    ///
    /// - `Directed` and `Undirected`: All edges connected to `a`.
    ///
    /// Produces an empty iterator if the node `a` doesn't exist.<br>
    /// Iterator element type is `EdgeReference<E, Ix>`.
    fn edges_undirected(&self, a: NodeIndex<Ix>) -> Edges<E, Ty, Ix> {
        Edges {
            skip_start: a,
            edges: &self.g.edges,
            direction: None,
            next: match self.get_node(a) {
                None => [EdgeIndex::end(), EdgeIndex::end()],
                Some(n) => n.next,
            },
            ty: PhantomData,
        }
    }

    /// Index the `StableGraph` by two indices, any combination of
    /// node or edge indices is fine.
    ///
    /// **Panics** if the indices are equal or if they are out of bounds.
    pub fn index_twice_mut<T, U>(&mut self, i: T, j: U)
        -> (&mut <Self as Index<T>>::Output,
            &mut <Self as Index<U>>::Output)
        where Self: IndexMut<T> + IndexMut<U>,
              T: GraphIndex,
              U: GraphIndex,
    {
        assert!(T::is_node_index() != U::is_node_index() ||
                i.index() != j.index());

        // Allow two mutable indexes here -- they are nonoverlapping
        unsafe {
            let self_mut = self as *mut _;
            (<Self as IndexMut<T>>::index_mut(&mut *self_mut, i),
             <Self as IndexMut<U>>::index_mut(&mut *self_mut, j))
        }
    }

    /// Keep all nodes that return `true` from the `visit` closure,
    /// remove the others.
    ///
    /// `visit` is provided a proxy reference to the graph, so that
    /// the graph can be walked and associated data modified.
    ///
    /// The order nodes are visited is not specified.
    ///
    /// The node indices of the removed nodes are invalidated, but none other.
    /// Edge indices are invalidated as they would be following the removal of
    /// each edge with an endpoint in a removed node.
    ///
    /// Computes in **O(n + e')** time, where **n** is the number of node indices and
    ///  **e'** is the number of affected edges, including *n* calls to `.remove_edge()`
    /// where *n* is the number of edges with an endpoint in a removed node.
    pub fn retain_nodes<F>(&mut self, mut visit: F) where F: FnMut(Frozen<Self>, NodeIndex<Ix>) -> bool {
        for i in 0..self.node_bound() {
            let ix = node_index(i);
            if self.contains_node(ix) && !visit(Frozen(self), ix) {
                self.remove_node(ix);
            }
        }
        self.check_free_lists();
    }

    /// Keep all edges that return `true` from the `visit` closure,
    /// remove the others.
    ///
    /// `visit` is provided a proxy reference to the graph, so that
    /// the graph can be walked and associated data modified.
    ///
    /// The order edges are visited is not specified.
    ///
    /// The edge indices of the removed edes are invalidated, but none other.
    ///
    /// Computes in **O(e'')** time, **e'** is the number of affected edges,
    /// including the calls to `.remove_edge()` for each removed edge.
    pub fn retain_edges<F>(&mut self, mut visit: F)
        where F: FnMut(Frozen<Self>, EdgeIndex<Ix>) -> bool
    {
        for i in 0..self.edge_bound() {
            let ix = edge_index(i);
            if self.edge_weight(ix).is_some() && !visit(Frozen(self), ix) {
                self.remove_edge(ix);
            }
        }
        self.check_free_lists();
    }

    /// Create a new `StableGraph` from an iterable of edges.
    ///
    /// Node weights `N` are set to default values.
    /// Edge weights `E` may either be specified in the list,
    /// or they are filled with default values.
    ///
    /// Nodes are inserted automatically to match the edges.
    ///
    /// ```
    /// use petgraph::stable_graph::StableGraph;
    ///
    /// let gr = StableGraph::<(), i32>::from_edges(&[
    ///     (0, 1), (0, 2), (0, 3),
    ///     (1, 2), (1, 3),
    ///     (2, 3),
    /// ]);
    /// ```
    pub fn from_edges<I>(iterable: I) -> Self
        where I: IntoIterator,
              I::Item: IntoWeightedEdge<E>,
              <I::Item as IntoWeightedEdge<E>>::NodeId: Into<NodeIndex<Ix>>,
              N: Default,
    {
        let mut g = Self::with_capacity(0, 0);
        g.extend_with_edges(iterable);
        g
    }

    /// Create a new `StableGraph` by mapping node and
    /// edge weights to new values.
    ///
    /// The resulting graph has the same structure and the same
    /// graph indices as `self`.
    pub fn map<'a, F, G, N2, E2>(&'a self, mut node_map: F, mut edge_map: G)
        -> StableGraph<N2, E2, Ty, Ix>
        where F: FnMut(NodeIndex<Ix>, &'a N) -> N2,
              G: FnMut(EdgeIndex<Ix>, &'a E) -> E2,
    {
        let g = self.g.map(
            move |i, w| w.as_ref().map(|w| node_map(i, w)),
            move |i, w| w.as_ref().map(|w| edge_map(i, w)));
        StableGraph {
            g: g,
            node_count: self.node_count,
            edge_count: self.edge_count,
            free_node: self.free_node,
            free_edge: self.free_edge,
        }
    }

    /// Create a new `StableGraph` by mapping nodes and edges.
    /// A node or edge may be mapped to `None` to exclude it from
    /// the resulting graph.
    ///
    /// Nodes are mapped first with the `node_map` closure, then
    /// `edge_map` is called for the edges that have not had any endpoint
    /// removed.
    ///
    /// The resulting graph has the structure of a subgraph of the original graph.
    /// Nodes and edges that are not removed maintain their old node or edge
    /// indices.
    pub fn filter_map<'a, F, G, N2, E2>(&'a self, mut node_map: F, mut edge_map: G)
        -> StableGraph<N2, E2, Ty, Ix>
        where F: FnMut(NodeIndex<Ix>, &'a N) -> Option<N2>,
              G: FnMut(EdgeIndex<Ix>, &'a E) -> Option<E2>,
    {
        let node_bound = self.node_bound();
        let edge_bound = self.edge_bound();
        let mut result_g = StableGraph::with_capacity(node_bound, edge_bound);
        // use separate free lists so that
        // add_node / add_edge below do not reuse the tombstones
        let mut free_node = NodeIndex::end();
        let mut free_edge = EdgeIndex::end();

        // the stable graph keeps the node map itself

        for (i, node) in enumerate(self.raw_nodes()) {
            if i >= node_bound { break; }
            if let Some(node_weight) = node.weight.as_ref() {
                if let Some(new_weight) = node_map(NodeIndex::new(i), node_weight) {
                    result_g.add_node(new_weight);
                    continue;
                }
            }
            result_g.add_vacant_node(&mut free_node);
        }
        for (i, edge) in enumerate(self.raw_edges()) {
            if i >= edge_bound { break; }
            let source = edge.source();
            let target = edge.target();
            if let Some(edge_weight) = edge.weight.as_ref() {
                if result_g.contains_node(source) && result_g.contains_node(target) {
                    if let Some(new_weight) = edge_map(EdgeIndex::new(i), edge_weight) {
                        result_g.add_edge(source, target, new_weight);
                        continue;
                    }
                }
            }
            result_g.add_vacant_edge(&mut free_edge);
        }
        result_g.free_node = free_node;
        result_g.free_edge = free_edge;
        result_g.check_free_lists();
        result_g
    }

    /// Extend the graph from an iterable of edges.
    ///
    /// Node weights `N` are set to default values.
    /// Edge weights `E` may either be specified in the list,
    /// or they are filled with default values.
    ///
    /// Nodes are inserted automatically to match the edges.
    pub fn extend_with_edges<I>(&mut self, iterable: I)
        where I: IntoIterator,
              I::Item: IntoWeightedEdge<E>,
              <I::Item as IntoWeightedEdge<E>>::NodeId: Into<NodeIndex<Ix>>,
              N: Default,
    {
        let iter = iterable.into_iter();

        for elt in iter {
            let (source, target, weight) = elt.into_weighted_edge();
            let (source, target) = (source.into(), target.into());
            let nx = cmp::max(source, target);
            while nx.index() >= self.node_count() {
                self.add_node(N::default());
            }
            self.add_edge(source, target, weight);
        }
    }

    //
    // internal methods
    //
    fn raw_nodes(&self) -> &[Node<Option<N>, Ix>] {
        self.g.raw_nodes()
    }

    fn raw_edges(&self) -> &[Edge<Option<E>, Ix>] {
        self.g.raw_edges()
    }

    fn edge_bound(&self) -> usize {
        self.edge_references()
            .next_back()
            .map_or(0, |edge| edge.id().index() + 1)
    }

    #[cfg(feature = "serde-1")]
    /// Fix up node and edge links after deserialization
    fn link_edges(&mut self) -> Result<(), NodeIndex<Ix>> {
        // set up free node list
        self.node_count = 0;
        self.edge_count = 0;
        let mut free_node = NodeIndex::end();
        for (node_index, node) in enumerate(&mut self.g.nodes) {
            if node.weight.is_some() {
                self.node_count += 1;
            } else {
                // free node
                node.next = [free_node._into_edge(), EdgeIndex::end()];
                free_node = NodeIndex::new(node_index);
            }
        }
        self.free_node = free_node;

        let mut free_edge = EdgeIndex::end();
        for (edge_index, edge) in enumerate(&mut self.g.edges) {
            if edge.weight.is_none() {
                // free edge
                edge.next = [free_edge, EdgeIndex::end()];
                free_edge = EdgeIndex::new(edge_index);
                continue;
            }
            let a = edge.source();
            let b = edge.target();
            let edge_idx = EdgeIndex::new(edge_index);
            match index_twice(&mut self.g.nodes, a.index(), b.index()) {
                Pair::None => return Err(if a > b { a } else { b }),
                Pair::One(an) => {
                    edge.next = an.next;
                    an.next[0] = edge_idx;
                    an.next[1] = edge_idx;
                }
                Pair::Both(an, bn) => {
                    // a and b are different indices
                    edge.next = [an.next[0], bn.next[1]];
                    an.next[0] = edge_idx;
                    bn.next[1] = edge_idx;
                }
            }
            self.edge_count += 1;
        }
        self.free_edge = free_edge;
        Ok(())
    }

    #[cfg(not(debug_assertions))]
    fn check_free_lists(&self) { }
    #[cfg(debug_assertions)]
    // internal method to debug check the free lists (linked lists)
    fn check_free_lists(&self) {
        let mut free_node = self.free_node;
        let mut free_node_len = 0;
        while free_node != NodeIndex::end() {
            if let Some(n) = self.g.nodes.get(free_node.index()) {
                if let None = n.weight {
                    free_node = n.next[0]._into_node();
                    free_node_len += 1;
                    continue;
                }
                debug_assert!(false, "Corrupt free list: pointing to existing {:?}",
                              free_node.index());
            }
            debug_assert!(false, "Corrupt free list: missing {:?}", free_node.index());
        }
        debug_assert_eq!(self.node_count(), self.raw_nodes().len() - free_node_len);

        let mut free_edge_len = 0;
        let mut free_edge = self.free_edge;
        while free_edge != EdgeIndex::end() {
            if let Some(n) = self.g.edges.get(free_edge.index()) {
                if let None = n.weight {
                    free_edge = n.next[0];
                    free_edge_len += 1;
                    continue;
                }
                debug_assert!(false, "Corrupt free list: pointing to existing {:?}",
                              free_node.index());
            }
            debug_assert!(false, "Corrupt free list: missing {:?}", free_edge.index());
        }
        debug_assert_eq!(self.edge_count(), self.raw_edges().len() - free_edge_len);
    }
}

/// The resulting cloned graph has the same graph indices as `self`.
impl<N, E, Ty, Ix: IndexType> Clone for StableGraph<N, E, Ty, Ix>
    where N: Clone, E: Clone,
{
    fn clone(&self) -> Self {
        StableGraph {
            g: self.g.clone(),
            node_count: self.node_count,
            edge_count: self.edge_count,
            free_node: self.free_node,
            free_edge: self.free_edge,
        }
    }

    fn clone_from(&mut self, rhs: &Self) {
        self.g.clone_from(&rhs.g);
        self.node_count = rhs.node_count;
        self.edge_count = rhs.edge_count;
        self.free_node = rhs.free_node;
        self.free_edge = rhs.free_edge;
    }
}

/// Index the `StableGraph` by `NodeIndex` to access node weights.
///
/// **Panics** if the node doesn't exist.
impl<N, E, Ty, Ix> Index<NodeIndex<Ix>> for StableGraph<N, E, Ty, Ix> where
    Ty: EdgeType,
    Ix: IndexType,
{
    type Output = N;
    fn index(&self, index: NodeIndex<Ix>) -> &N {
        self.node_weight(index).unwrap()
    }
}

/// Index the `StableGraph` by `NodeIndex` to access node weights.
///
/// **Panics** if the node doesn't exist.
impl<N, E, Ty, Ix> IndexMut<NodeIndex<Ix>> for StableGraph<N, E, Ty, Ix> where
    Ty: EdgeType,
    Ix: IndexType,
{
    fn index_mut(&mut self, index: NodeIndex<Ix>) -> &mut N {
        self.node_weight_mut(index).unwrap()
    }

}

/// Index the `StableGraph` by `EdgeIndex` to access edge weights.
///
/// **Panics** if the edge doesn't exist.
impl<N, E, Ty, Ix> Index<EdgeIndex<Ix>> for StableGraph<N, E, Ty, Ix> where
    Ty: EdgeType,
    Ix: IndexType,
{
    type Output = E;
    fn index(&self, index: EdgeIndex<Ix>) -> &E {
        self.edge_weight(index).unwrap()
    }
}

/// Index the `StableGraph` by `EdgeIndex` to access edge weights.
///
/// **Panics** if the edge doesn't exist.
impl<N, E, Ty, Ix> IndexMut<EdgeIndex<Ix>> for StableGraph<N, E, Ty, Ix> where
    Ty: EdgeType,
    Ix: IndexType,
{
    fn index_mut(&mut self, index: EdgeIndex<Ix>) -> &mut E {
        self.edge_weight_mut(index).unwrap()
    }
}

/// Create a new empty `StableGraph`.
impl<N, E, Ty, Ix> Default for StableGraph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    fn default() -> Self { Self::with_capacity(0, 0) }
}

/// Convert a `Graph` into a `StableGraph`
///
/// Computes in **O(|V| + |E|)** time.
///
/// The resulting graph has the same node and edge indices as
/// the original graph.
impl<N, E, Ty, Ix> From<Graph<N, E, Ty, Ix>> for StableGraph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    fn from(g: Graph<N, E, Ty, Ix>) -> Self {
        let nodes = g.nodes.into_iter().map(|e| Node {
            weight: Some(e.weight),
            next: e.next,
        });
        let edges = g.edges.into_iter().map(|e| Edge {
            weight: Some(e.weight),
            node: e.node,
            next: e.next,
        });
        StableGraph {
            node_count: nodes.len(),
            edge_count: edges.len(),
            g: Graph { edges: edges.collect(), nodes: nodes.collect(), ty: g.ty },
            free_node: NodeIndex::end(),
            free_edge: EdgeIndex::end(),
        }
    }
}

/// Convert a `StableGraph` into a `Graph`
///
/// Computes in **O(|V| + |E|)** time.
///
/// This translates the stable graph into a graph with node and edge indices in
/// a compact interval without holes (like `Graph`s always are).
///
/// Only if the stable graph had no vacancies after deletions (if node bound was
/// equal to node count, and the same for edges), would the resulting graph have
/// the same node and edge indices as the input.
impl<N, E, Ty, Ix> From<StableGraph<N, E, Ty, Ix>> for Graph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    fn from(graph: StableGraph<N, E, Ty, Ix>) -> Self {
        let mut result_g = Graph::with_capacity(graph.node_count(), graph.edge_count());
        // mapping from old node index to new node index
        let mut node_index_map = vec![NodeIndex::end(); graph.node_bound()];

        for (i, node) in enumerate(graph.g.nodes) {
            if let Some(nw) = node.weight {
                node_index_map[i] = result_g.add_node(nw);
            }
        }
        for edge in graph.g.edges {
            let source_index = edge.source().index();
            let target_index = edge.target().index();
            if let Some(ew) = edge.weight {
                let source = node_index_map[source_index];
                let target = node_index_map[target_index];
                debug_assert!(source != NodeIndex::end());
                debug_assert!(target != NodeIndex::end());
                result_g.add_edge(source, target, ew);
            }
        }
        result_g
    }
}

impl<'a, N, E, Ty, Ix> IntoNodeReferences for &'a StableGraph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    type NodeRef = (NodeIndex<Ix>, &'a N);
    type NodeReferences = NodeReferences<'a, N, Ix>;
    fn node_references(self) -> Self::NodeReferences {
        NodeReferences {
            iter: enumerate(self.raw_nodes())
        }
    }
}

/// Iterator over all nodes of a graph.
pub struct NodeReferences<'a, N: 'a, Ix: IndexType = DefaultIx> {
    iter: iter::Enumerate<slice::Iter<'a, Node<Option<N>, Ix>>>,
}

impl<'a, N, Ix> Iterator for NodeReferences<'a, N, Ix>
    where Ix: IndexType
{
    type Item = (NodeIndex<Ix>, &'a N);

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.ex_find_map(|(i, node)| {
            node.weight.as_ref().map(move |w| (node_index(i), w))
        })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let (_, hi) = self.iter.size_hint();
        (0, hi)
    }
}

impl<'a, N, Ix> DoubleEndedIterator for NodeReferences<'a, N, Ix>
    where Ix: IndexType
{
    fn next_back(&mut self) -> Option<Self::Item> {
        self.iter.ex_rfind_map(|(i, node)| {
            node.weight.as_ref().map(move |w| (node_index(i), w))
        })
    }
}

/// Reference to a `StableGraph` edge.
#[derive(Debug)]
pub struct EdgeReference<'a, E: 'a, Ix = DefaultIx> {
    index: EdgeIndex<Ix>,
    node: [NodeIndex<Ix>; 2],
    weight: &'a E,
}

impl<'a, E, Ix: IndexType> Clone for EdgeReference<'a, E, Ix> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<'a, E, Ix: IndexType> Copy for EdgeReference<'a, E, Ix> { }

impl<'a, E, Ix: IndexType> PartialEq for EdgeReference<'a, E, Ix>
    where E: PartialEq,
{
    fn eq(&self, rhs: &Self) -> bool {
        self.index == rhs.index && self.weight == rhs.weight
    }
}

impl<'a, Ix, E> EdgeReference<'a, E, Ix>
    where Ix: IndexType,
{
    /// Access the edge’s weight.
    ///
    /// **NOTE** that this method offers a longer lifetime
    /// than the trait (unfortunately they don't match yet).
    pub fn weight(&self) -> &'a E { self.weight }
}

impl<'a, Ix, E> EdgeRef for EdgeReference<'a, E, Ix>
    where Ix: IndexType,
{
    type NodeId = NodeIndex<Ix>;
    type EdgeId = EdgeIndex<Ix>;
    type Weight = E;

    fn source(&self) -> Self::NodeId { self.node[0] }
    fn target(&self) -> Self::NodeId { self.node[1] }
    fn weight(&self) -> &E { self.weight }
    fn id(&self) -> Self::EdgeId { self.index }
}

impl<'a, N, E, Ty, Ix> IntoEdges for &'a StableGraph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    type Edges = Edges<'a, E, Ty, Ix>;
    fn edges(self, a: Self::NodeId) -> Self::Edges {
        self.edges(a)
    }
}

impl<'a, N, E, Ty, Ix> IntoEdgesDirected for &'a StableGraph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    type EdgesDirected = Edges<'a, E, Ty, Ix>;
    fn edges_directed(self, a: Self::NodeId, dir: Direction) -> Self::EdgesDirected {
        self.edges_directed(a, dir)
    }
}


/// Iterator over the edges of from or to a node
pub struct Edges<'a, E: 'a, Ty, Ix: 'a = DefaultIx>
    where Ty: EdgeType,
          Ix: IndexType,
{
    /// starting node to skip over
    skip_start: NodeIndex<Ix>,
    edges: &'a [Edge<Option<E>, Ix>],

    /// Next edge to visit.
    /// If we are only following one direction, we only use next[0] regardless.
    next: [EdgeIndex<Ix>; 2],

    /// Which direction to follow
    /// None: Both,
    /// Some(d): d if Directed, Both if Undirected
    direction: Option<Direction>,
    ty: PhantomData<Ty>,
}

impl<'a, E, Ty, Ix> Iterator for Edges<'a, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    type Item = EdgeReference<'a, E, Ix>;

    fn next(&mut self) -> Option<Self::Item> {
        // First the outgoing or incoming edges (directionality)
        let k = self.direction.unwrap_or(Outgoing).index();
        let i = self.next[0].index();
        match self.edges.get(i) {
            None => {}
            Some(&Edge { ref node, weight: Some(ref weight), ref next }) => {
                self.next[0] = next[k];
                return Some(EdgeReference {
                    index: edge_index(i),
                    node: *node,
                    weight: weight,
                });
            }
            Some(_otherwise) => unreachable!(),
        }
        // Stop here if we only follow one direction
        if self.direction.is_some() {
            return None;
        }
        // Then incoming edges
        // For an "undirected" iterator (traverse both incoming
        // and outgoing edge lists), make sure we don't double
        // count selfloops by skipping them in the incoming list.

        // We reach here if self.direction was None or Outgoing.
        debug_assert_eq!(k, 0);
        while let Some(edge) = self.edges.get(self.next[1].index()) {
            debug_assert!(edge.weight.is_some());
            let i = self.next[1].index();
            self.next[1] = edge.next[1];
            if edge.node[0] != self.skip_start {
                return Some(EdgeReference {
                    index: edge_index(i),
                    node: swap_pair(edge.node),
                    weight: edge.weight.as_ref().unwrap(),
                });
            }
        }
        None
    }
}

fn swap_pair<T>(mut x: [T; 2]) -> [T; 2] {
    x.swap(0, 1);
    x
}

impl<'a, N: 'a, E: 'a, Ty, Ix> IntoEdgeReferences for &'a StableGraph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    type EdgeRef = EdgeReference<'a, E, Ix>;
    type EdgeReferences = EdgeReferences<'a, E, Ix>;

    /// Create an iterator over all edges in the graph, in indexed order.
    ///
    /// Iterator element type is `EdgeReference<E, Ix>`.
    fn edge_references(self) -> Self::EdgeReferences {
        EdgeReferences {
            iter: self.g.edges.iter().enumerate()
        }
    }

}

/// Iterator over all edges of a graph.
pub struct EdgeReferences<'a, E: 'a, Ix: 'a = DefaultIx> {
    iter: iter::Enumerate<slice::Iter<'a, Edge<Option<E>, Ix>>>,
}

impl<'a, E, Ix> Iterator for EdgeReferences<'a, E, Ix>
    where Ix: IndexType
{
    type Item = EdgeReference<'a, E, Ix>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.ex_find_map(|(i, edge)|
            edge.weight.as_ref().map(move |weight| {
                EdgeReference {
                    index: edge_index(i),
                    node: edge.node,
                    weight: weight,
                }
            }))
    }
}

impl<'a, E, Ix> DoubleEndedIterator for EdgeReferences<'a, E, Ix>
    where Ix: IndexType
{
    fn next_back(&mut self) -> Option<Self::Item> {
        self.iter.ex_rfind_map(|(i, edge)|
            edge.weight.as_ref().map(move |weight| {
                EdgeReference {
                    index: edge_index(i),
                    node: edge.node,
                    weight: weight,
                }
            }))
    }
}


/// Iterator over the neighbors of a node.
///
/// Iterator element type is `NodeIndex`.
pub struct Neighbors<'a, E: 'a, Ix: 'a = DefaultIx>
{
    /// starting node to skip over
    skip_start: NodeIndex<Ix>,
    edges: &'a [Edge<Option<E>, Ix>],
    next: [EdgeIndex<Ix>; 2],
}

impl<'a, E, Ix> Neighbors<'a, E, Ix>
    where Ix: IndexType,
{
    /// Return a “walker” object that can be used to step through the
    /// neighbors and edges from the origin node.
    ///
    /// Note: The walker does not borrow from the graph, this is to allow mixing
    /// edge walking with mutating the graph's weights.
    pub fn detach(&self) -> WalkNeighbors<Ix> {
        WalkNeighbors {
            inner: super::WalkNeighbors {
                skip_start: self.skip_start,
                next: self.next
            },
        }
    }
}

impl<'a, E, Ix> Iterator for Neighbors<'a, E, Ix> where
    Ix: IndexType,
{
    type Item = NodeIndex<Ix>;

    fn next(&mut self) -> Option<NodeIndex<Ix>> {
        // First any outgoing edges
        match self.edges.get(self.next[0].index()) {
            None => {}
            Some(edge) => {
                debug_assert!(edge.weight.is_some());
                self.next[0] = edge.next[0];
                return Some(edge.node[1]);
            }
        }
        // Then incoming edges
        // For an "undirected" iterator (traverse both incoming
        // and outgoing edge lists), make sure we don't double
        // count selfloops by skipping them in the incoming list.
        while let Some(edge) = self.edges.get(self.next[1].index()) {
            debug_assert!(edge.weight.is_some());
            self.next[1] = edge.next[1];
            if edge.node[0] != self.skip_start {
                return Some(edge.node[0]);
            }
        }
        None
    }
}

/// A “walker” object that can be used to step through the edge list of a node.
///
/// See [*.detach()*](struct.Neighbors.html#method.detach) for more information.
///
/// The walker does not borrow from the graph, so it lets you step through
/// neighbors or incident edges while also mutating graph weights, as
/// in the following example:
///
/// ```
/// use petgraph::visit::Dfs;
/// use petgraph::Incoming;
/// use petgraph::stable_graph::StableGraph;
///
/// let mut gr = StableGraph::new();
/// let a = gr.add_node(0.);
/// let b = gr.add_node(0.);
/// let c = gr.add_node(0.);
/// gr.add_edge(a, b, 3.);
/// gr.add_edge(b, c, 2.);
/// gr.add_edge(c, b, 1.);
///
/// // step through the graph and sum incoming edges into the node weight
/// let mut dfs = Dfs::new(&gr, a);
/// while let Some(node) = dfs.next(&gr) {
///     // use a detached neighbors walker
///     let mut edges = gr.neighbors_directed(node, Incoming).detach();
///     while let Some(edge) = edges.next_edge(&gr) {
///         gr[node] += gr[edge];
///     }
/// }
///
/// // check the result
/// assert_eq!(gr[a], 0.);
/// assert_eq!(gr[b], 4.);
/// assert_eq!(gr[c], 2.);
/// ```
pub struct WalkNeighbors<Ix> {
    inner: super::WalkNeighbors<Ix>,
}

impl<Ix: IndexType> Clone for WalkNeighbors<Ix> {
    clone_fields!(WalkNeighbors, inner);
}

impl<Ix: IndexType> WalkNeighbors<Ix> {
    /// Step to the next edge and its endpoint node in the walk for graph `g`.
    ///
    /// The next node indices are always the others than the starting point
    /// where the `WalkNeighbors` value was created.
    /// For an `Outgoing` walk, the target nodes,
    /// for an `Incoming` walk, the source nodes of the edge.
    pub fn next<N, E, Ty: EdgeType>(&mut self, g: &StableGraph<N, E, Ty, Ix>)
        -> Option<(EdgeIndex<Ix>, NodeIndex<Ix>)> {
        self.inner.next(&g.g)
    }

    pub fn next_node<N, E, Ty: EdgeType>(&mut self, g: &StableGraph<N, E, Ty, Ix>)
        -> Option<NodeIndex<Ix>>
    {
        self.next(g).map(|t| t.1)
    }

    pub fn next_edge<N, E, Ty: EdgeType>(&mut self, g: &StableGraph<N, E, Ty, Ix>)
        -> Option<EdgeIndex<Ix>>
    {
        self.next(g).map(|t| t.0)
    }
}

/// Iterator over the node indices of a graph.
pub struct NodeIndices<'a, N: 'a, Ix: 'a = DefaultIx> {
    iter: iter::Enumerate<slice::Iter<'a, Node<Option<N>, Ix>>>,
}

impl<'a, N, Ix: IndexType> Iterator for NodeIndices<'a, N, Ix> {
    type Item = NodeIndex<Ix>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.ex_find_map(|(i, node)| {
            if node.weight.is_some() {
                Some(node_index(i))
            } else { None }
        })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let (_, hi) = self.iter.size_hint();
        (0, hi)
    }
}

impl<'a, N, Ix: IndexType> DoubleEndedIterator for NodeIndices<'a, N, Ix> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.iter.ex_rfind_map(|(i, node)| {
            if node.weight.is_some() {
                Some(node_index(i))
            } else { None }
        })
    }
}

impl<N, E, Ty, Ix> NodeIndexable for StableGraph<N, E, Ty, Ix>
    where Ty: EdgeType,
          Ix: IndexType,
{
    /// Return an upper bound of the node indices in the graph
    fn node_bound(&self) -> usize {
        self.node_indices()
            .next_back()
            .map_or(0, |i| i.index() + 1)
    }
    fn to_index(&self, ix: NodeIndex<Ix>) -> usize { ix.index() }
    fn from_index(&self, ix: usize) -> Self::NodeId { NodeIndex::new(ix) }
}

/// Iterator over the edge indices of a graph.
pub struct EdgeIndices<'a, E: 'a, Ix: 'a = DefaultIx> {
    iter: iter::Enumerate<slice::Iter<'a, Edge<Option<E>, Ix>>>,
}

impl<'a, E, Ix: IndexType> Iterator for EdgeIndices<'a, E, Ix> {
    type Item = EdgeIndex<Ix>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.ex_find_map(|(i, node)| {
            if node.weight.is_some() {
                Some(edge_index(i))
            } else { None }
        })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let (_, hi) = self.iter.size_hint();
        (0, hi)
    }
}

impl<'a, E, Ix: IndexType> DoubleEndedIterator for EdgeIndices<'a, E, Ix> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.iter.ex_rfind_map(|(i, node)| {
            if node.weight.is_some() {
                Some(edge_index(i))
            } else { None }
        })
    }
}


#[test]
fn stable_graph() {
    let mut gr = StableGraph::<_, _>::with_capacity(0, 0);
    let a = gr.add_node(0);
    let b = gr.add_node(1);
    let c = gr.add_node(2);
    let _ed = gr.add_edge(a, b, 1);
    println!("{:?}", gr);
    gr.remove_node(b);
    println!("{:?}", gr);
    let d = gr.add_node(3);
    println!("{:?}", gr);
    gr.check_free_lists();
    gr.remove_node(a);
    gr.check_free_lists();
    gr.remove_node(c);
    gr.check_free_lists();
    println!("{:?}", gr);
    gr.add_edge(d, d, 2);
    println!("{:?}", gr);

    let e = gr.add_node(4);
    gr.add_edge(d, e, 3);
    println!("{:?}", gr);
    for neigh in gr.neighbors(d) {
        println!("edge {:?} -> {:?}", d, neigh);
    }
    gr.check_free_lists();
}

#[test]
fn dfs() {
    use visit::Dfs;

    let mut gr = StableGraph::<_, _>::with_capacity(0, 0);
    let a = gr.add_node("a");
    let b = gr.add_node("b");
    let c = gr.add_node("c");
    let d = gr.add_node("d");
    gr.add_edge(a, b, 1);
    gr.add_edge(a, c, 2);
    gr.add_edge(b, c, 3);
    gr.add_edge(b, d, 4);
    gr.add_edge(c, d, 5);
    gr.add_edge(d, b, 6);
    gr.add_edge(c, b, 7);
    println!("{:?}", gr);

    let mut dfs = Dfs::new(&gr, a);
    while let Some(next) = dfs.next(&gr) {
        println!("dfs visit => {:?}, weight={:?}", next, &gr[next]);
    }
}

#[test]
fn test_retain_nodes() {
    let mut gr = StableGraph::<_, _>::with_capacity(6, 6);
    let a = gr.add_node("a");
    let f = gr.add_node("f");
    let b = gr.add_node("b");
    let c = gr.add_node("c");
    let d = gr.add_node("d");
    let e = gr.add_node("e");
    gr.add_edge(a, b, 1);
    gr.add_edge(a, c, 2);
    gr.add_edge(b, c, 3);
    gr.add_edge(b, d, 4);
    gr.add_edge(c, d, 5);
    gr.add_edge(d, b, 6);
    gr.add_edge(c, b, 7);
    gr.add_edge(d, e, 8);
    gr.remove_node(f);

    assert_eq!(gr.node_count(), 5);
    assert_eq!(gr.edge_count(), 8);
    gr.retain_nodes(|frozen_gr, ix| {frozen_gr[ix] >= "c"});
    assert_eq!(gr.node_count(), 3);
    assert_eq!(gr.edge_count(), 2);

    gr.check_free_lists();
}
