//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/RemoveInvariantDeclaration.h"

#include "compiler/translator/IntermNode.h"

namespace sh
{

namespace
{

// An AST traverser that removes invariant declaration for input in fragment shader
// when GLSL >= 4.20 and for output in vertex shader when GLSL < 4.2.
class RemoveInvariantDeclarationTraverser : public TIntermTraverser
{
  public:
    RemoveInvariantDeclarationTraverser() : TIntermTraverser(true, false, false) {}

  private:
    bool visitAggregate(Visit visit, TIntermAggregate *node) override
    {
        if (node->getOp() == EOpInvariantDeclaration)
        {
            TIntermSequence emptyReplacement;
            mMultiReplacements.push_back(NodeReplaceWithMultipleEntry(getParentNode()->getAsBlock(),
                                                                      node, emptyReplacement));
            return false;
        }
        return true;
    }
};

}  // anonymous namespace

void RemoveInvariantDeclaration(TIntermNode *root)
{
    RemoveInvariantDeclarationTraverser traverser;
    root->traverse(&traverser);
    traverser.updateTree();
}

}  // namespace sh
