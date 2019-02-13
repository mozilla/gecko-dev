//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/IntermNode.h"

//
// Traverse the intermediate representation tree, and
// call a node type specific function for each node.
// Done recursively through the member function Traverse().
// Node types can be skipped if their function to call is 0,
// but their subtree will still be traversed.
// Nodes with children can have their whole subtree skipped
// if preVisit is turned on and the type specific function
// returns false.
//
// preVisit, postVisit, and rightToLeft control what order
// nodes are visited in.
//

//
// Traversal functions for terminals are straighforward....
//
void TIntermSymbol::traverse(TIntermTraverser *it)
{
    it->visitSymbol(this);
}

void TIntermConstantUnion::traverse(TIntermTraverser *it)
{
    it->visitConstantUnion(this);
}

//
// Traverse a binary node.
//
void TIntermBinary::traverse(TIntermTraverser *it)
{
    bool visit = true;

    //
    // visit the node before children if pre-visiting.
    //
    if (it->preVisit)
        visit = it->visitBinary(PreVisit, this);

    //
    // Visit the children, in the right order.
    //
    if (visit)
    {
        it->incrementDepth(this);

        if (it->rightToLeft)
        {
            if (mRight)
                mRight->traverse(it);

            if (it->inVisit)
                visit = it->visitBinary(InVisit, this);

            if (visit && mLeft)
                mLeft->traverse(it);
        }
        else
        {
            if (mLeft)
                mLeft->traverse(it);

            if (it->inVisit)
                visit = it->visitBinary(InVisit, this);

            if (visit && mRight)
                mRight->traverse(it);
        }

        it->decrementDepth();
    }

    //
    // Visit the node after the children, if requested and the traversal
    // hasn't been cancelled yet.
    //
    if (visit && it->postVisit)
        it->visitBinary(PostVisit, this);
}

//
// Traverse a unary node.  Same comments in binary node apply here.
//
void TIntermUnary::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitUnary(PreVisit, this);

    if (visit) {
        it->incrementDepth(this);
        mOperand->traverse(it);
        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitUnary(PostVisit, this);
}

//
// Traverse an aggregate node.  Same comments in binary node apply here.
//
void TIntermAggregate::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitAggregate(PreVisit, this);

    if (visit)
    {
        it->incrementDepth(this);

        if (it->rightToLeft)
        {
            for (TIntermSequence::reverse_iterator sit = mSequence.rbegin();
                 sit != mSequence.rend(); sit++)
            {
                (*sit)->traverse(it);

                if (visit && it->inVisit)
                {
                    if (*sit != mSequence.front())
                        visit = it->visitAggregate(InVisit, this);
                }
            }
        }
        else
        {
            for (TIntermSequence::iterator sit = mSequence.begin();
                 sit != mSequence.end(); sit++)
            {
                (*sit)->traverse(it);

                if (visit && it->inVisit)
                {
                    if (*sit != mSequence.back())
                        visit = it->visitAggregate(InVisit, this);
                }
            }
        }

        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitAggregate(PostVisit, this);
}

//
// Traverse a selection node.  Same comments in binary node apply here.
//
void TIntermSelection::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitSelection(PreVisit, this);

    if (visit)
    {
        it->incrementDepth(this);
        if (it->rightToLeft)
        {
            if (mFalseBlock)
                mFalseBlock->traverse(it);
            if (mTrueBlock)
                mTrueBlock->traverse(it);
            mCondition->traverse(it);
        }
        else
        {
            mCondition->traverse(it);
            if (mTrueBlock)
                mTrueBlock->traverse(it);
            if (mFalseBlock)
                mFalseBlock->traverse(it);
        }
        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitSelection(PostVisit, this);
}

//
// Traverse a loop node.  Same comments in binary node apply here.
//
void TIntermLoop::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitLoop(PreVisit, this);

    if (visit)
    {
        it->incrementDepth(this);

        if (it->rightToLeft)
        {
            if (mExpr)
                mExpr->traverse(it);

            if (mBody)
                mBody->traverse(it);

            if (mCond)
                mCond->traverse(it);

            if (mInit)
                mInit->traverse(it);
        }
        else
        {
            if (mInit)
                mInit->traverse(it);

            if (mCond)
                mCond->traverse(it);

            if (mBody)
                mBody->traverse(it);

            if (mExpr)
                mExpr->traverse(it);
        }

        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitLoop(PostVisit, this);
}

//
// Traverse a branch node.  Same comments in binary node apply here.
//
void TIntermBranch::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitBranch(PreVisit, this);

    if (visit && mExpression) {
        it->incrementDepth(this);
        mExpression->traverse(it);
        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitBranch(PostVisit, this);
}

void TIntermRaw::traverse(TIntermTraverser *it)
{
    it->visitRaw(this);
}
