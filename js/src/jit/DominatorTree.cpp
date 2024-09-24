/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/DominatorTree.h"

#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace js::jit;

// A Simple, Fast Dominance Algorithm by Cooper et al.
// Modified to support empty intersections for OSR, and in RPO.
static MBasicBlock* IntersectDominators(MBasicBlock* block1,
                                        MBasicBlock* block2) {
  MBasicBlock* finger1 = block1;
  MBasicBlock* finger2 = block2;

  MOZ_ASSERT(finger1);
  MOZ_ASSERT(finger2);

  // In the original paper, the block ID comparisons are on the postorder index.
  // This implementation iterates in RPO, so the comparisons are reversed.

  // For this function to be called, the block must have multiple predecessors.
  // If a finger is then found to be self-dominating, it must therefore be
  // reachable from multiple roots through non-intersecting control flow.
  // nullptr is returned in this case, to denote an empty intersection.

  while (finger1->id() != finger2->id()) {
    while (finger1->id() > finger2->id()) {
      MBasicBlock* idom = finger1->immediateDominator();
      if (idom == finger1) {
        return nullptr;  // Empty intersection.
      }
      finger1 = idom;
    }

    while (finger2->id() > finger1->id()) {
      MBasicBlock* idom = finger2->immediateDominator();
      if (idom == finger2) {
        return nullptr;  // Empty intersection.
      }
      finger2 = idom;
    }
  }
  return finger1;
}

static void ComputeImmediateDominators(MIRGraph& graph) {
  // The default start block is a root and therefore only self-dominates.
  MBasicBlock* startBlock = graph.entryBlock();
  startBlock->setImmediateDominator(startBlock);

  // Any OSR block is a root and therefore only self-dominates.
  MBasicBlock* osrBlock = graph.osrBlock();
  if (osrBlock) {
    osrBlock->setImmediateDominator(osrBlock);
  }

  bool changed = true;

  while (changed) {
    changed = false;

    ReversePostorderIterator block = graph.rpoBegin();

    // For each block in RPO, intersect all dominators.
    for (; block != graph.rpoEnd(); block++) {
      // If a node has once been found to have no exclusive dominator,
      // it will never have an exclusive dominator, so it may be skipped.
      if (block->immediateDominator() == *block) {
        continue;
      }

      // A block with no predecessors is not reachable from any entry, so
      // it self-dominates.
      if (MOZ_UNLIKELY(block->numPredecessors() == 0)) {
        block->setImmediateDominator(*block);
        continue;
      }

      MBasicBlock* newIdom = block->getPredecessor(0);

      // Find the first common dominator.
      for (size_t i = 1; i < block->numPredecessors(); i++) {
        MBasicBlock* pred = block->getPredecessor(i);
        if (pred->immediateDominator() == nullptr) {
          continue;
        }

        newIdom = IntersectDominators(pred, newIdom);

        // If there is no common dominator, the block self-dominates.
        if (newIdom == nullptr) {
          block->setImmediateDominator(*block);
          changed = true;
          break;
        }
      }

      if (newIdom && block->immediateDominator() != newIdom) {
        block->setImmediateDominator(newIdom);
        changed = true;
      }
    }
  }

#ifdef DEBUG
  // Assert that all blocks have dominator information.
  for (MBasicBlockIterator block(graph.begin()); block != graph.end();
       block++) {
    MOZ_ASSERT(block->immediateDominator() != nullptr);
  }
#endif
}

bool jit::BuildDominatorTree(MIRGraph& graph) {
  MOZ_ASSERT(graph.canBuildDominators());

  ComputeImmediateDominators(graph);

  Vector<MBasicBlock*, 4, JitAllocPolicy> worklist(graph.alloc());

  // Traversing through the graph in post-order means that every non-phi use
  // of a definition is visited before the def itself. Since a def
  // dominates its uses, by the time we reach a particular
  // block, we have processed all of its dominated children, so
  // block->numDominated() is accurate.
  for (PostorderIterator i(graph.poBegin()); i != graph.poEnd(); i++) {
    MBasicBlock* child = *i;
    MBasicBlock* parent = child->immediateDominator();

    // Dominance is defined such that blocks always dominate themselves.
    child->addNumDominated(1);

    // If the block only self-dominates, it has no definite parent.
    // Add it to the worklist as a root for pre-order traversal.
    // This includes all roots. Order does not matter.
    if (child == parent) {
      if (!worklist.append(child)) {
        return false;
      }
      continue;
    }

    if (!parent->addImmediatelyDominatedBlock(child)) {
      return false;
    }

    parent->addNumDominated(child->numDominated());
  }

#ifdef DEBUG
  // If compiling with OSR, many blocks will self-dominate.
  // Without OSR, there is only one root block which dominates all.
  if (!graph.osrBlock()) {
    MOZ_ASSERT(graph.entryBlock()->numDominated() == graph.numBlocks());
  }
#endif
  // Now, iterate through the dominator tree in pre-order and annotate every
  // block with its index in the traversal.
  size_t index = 0;
  while (!worklist.empty()) {
    MBasicBlock* block = worklist.popCopy();
    block->setDomIndex(index);

    if (!worklist.append(block->immediatelyDominatedBlocksBegin(),
                         block->immediatelyDominatedBlocksEnd())) {
      return false;
    }
    index++;
  }

  return true;
}

void jit::ClearDominatorTree(MIRGraph& graph) {
  for (MBasicBlockIterator iter = graph.begin(); iter != graph.end(); iter++) {
    iter->clearDominatorInfo();
  }
}
