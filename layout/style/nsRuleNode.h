/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a node in the lexicographic tree of rules that match an element,
 * responsible for converting the rules' information into computed style
 */

#ifndef nsRuleNode_h___
#define nsRuleNode_h___

#include "mozilla/PodOperations.h"
#include "mozilla/RangedArray.h"
#include "mozilla/RuleNodeCacheConditions.h"
#include "nsPresContext.h"
#include "nsStyleStruct.h"

class nsCSSPropertySet;
class nsCSSValue;
class nsIStyleRule;
class nsStyleContext;
class nsStyleCoord;
struct nsCSSRect;
struct nsCSSValueList;
struct nsCSSValuePairList;
struct nsRuleData;

struct nsInheritedStyleData
{
  mozilla::RangedArray<void*,
                       nsStyleStructID_Inherited_Start,
                       nsStyleStructID_Inherited_Count> mStyleStructs;

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsInheritedStyleData_id, sz);
  }

  void DestroyStructs(uint64_t aBits, nsPresContext* aContext) {
#define STYLE_STRUCT_INHERITED(name, checkdata_cb) \
    void *name##Data = mStyleStructs[eStyleStruct_##name]; \
    if (name##Data && !(aBits & NS_STYLE_INHERIT_BIT(name))) \
      static_cast<nsStyle##name*>(name##Data)->Destroy(aContext);
#define STYLE_STRUCT_RESET(name, checkdata_cb)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET
  }

  void Destroy(uint64_t aBits, nsPresContext* aContext) {
    DestroyStructs(aBits, aContext);
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsInheritedStyleData_id, this);
  }

  nsInheritedStyleData() {
    for (nsStyleStructID i = nsStyleStructID_Inherited_Start;
         i < nsStyleStructID_Inherited_Start + nsStyleStructID_Inherited_Count;
         i = nsStyleStructID(i + 1)) {
      mStyleStructs[i] = nullptr;
    }
  }
};

struct nsResetStyleData
{
  mozilla::RangedArray<void*,
                       nsStyleStructID_Reset_Start,
                       nsStyleStructID_Reset_Count> mStyleStructs;

  nsResetStyleData()
  {
    for (nsStyleStructID i = nsStyleStructID_Reset_Start;
         i < nsStyleStructID_Reset_Start + nsStyleStructID_Reset_Count;
         i = nsStyleStructID(i + 1)) {
      mStyleStructs[i] = nullptr;
    }
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsResetStyleData_id, sz);
  }

  void Destroy(uint64_t aBits, nsPresContext* aContext) {
#define STYLE_STRUCT_RESET(name, checkdata_cb) \
    void *name##Data = mStyleStructs[eStyleStruct_##name]; \
    if (name##Data && !(aBits & NS_STYLE_INHERIT_BIT(name))) \
      static_cast<nsStyle##name*>(name##Data)->Destroy(aContext);
#define STYLE_STRUCT_INHERITED(name, checkdata_cb)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_RESET
#undef STYLE_STRUCT_INHERITED

    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsResetStyleData_id, this);
  }
};

struct nsConditionalResetStyleData
{
  static uint32_t GetBitForSID(const nsStyleStructID aSID) {
    return 1 << aSID;
  }

  struct Entry
  {
    Entry(const mozilla::RuleNodeCacheConditions& aConditions,
          void* aStyleStruct,
          Entry* aNext)
      : mConditions(aConditions), mStyleStruct(aStyleStruct), mNext(aNext) {}

    void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
      return aContext->PresShell()->
        AllocateByObjectID(nsPresArena::nsConditionalResetStyleDataEntry_id, sz);
    }

    const mozilla::RuleNodeCacheConditions mConditions;
    void* const mStyleStruct;
    Entry* const mNext;
  };

  // Each entry is either a pointer to a style struct or a
  // pointer to an Entry object.  A bit in mConditionalBits
  // means that it is an Entry.
  mozilla::RangedArray<void*,
                       nsStyleStructID_Reset_Start,
                       nsStyleStructID_Reset_Count> mEntries;

  uint32_t mConditionalBits;

  nsConditionalResetStyleData()
  {
    for (nsStyleStructID i = nsStyleStructID_Reset_Start;
         i < nsStyleStructID_Reset_Start + nsStyleStructID_Reset_Count;
         i = nsStyleStructID(i + 1)) {
      mEntries[i] = nullptr;
    }
    mConditionalBits = 0;
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsConditionalResetStyleData_id, sz);
  }

  void* GetStyleData(nsStyleStructID aSID) const {
    if (mConditionalBits & GetBitForSID(aSID)) {
      return nullptr;
    }
    return mEntries[aSID];
  }

  void* GetStyleData(nsStyleStructID aSID,
                     nsStyleContext* aStyleContext) const {
    if (!(mConditionalBits & GetBitForSID(aSID))) {
      return mEntries[aSID];
    }
    Entry* e = static_cast<Entry*>(mEntries[aSID]);
    MOZ_ASSERT(e, "if mConditionalBits bit is set, we must have at least one "
                  "conditional style struct");
    do {
      if (e->mConditions.Matches(aStyleContext)) {
        return e->mStyleStruct;
      }
      e = e->mNext;
    } while (e);
    return nullptr;
  }

  void SetStyleData(nsStyleStructID aSID, void* aStyleStruct) {
    MOZ_ASSERT(!(mConditionalBits & GetBitForSID(aSID)),
               "rule node should not have unconditional and conditional style "
               "data for a given struct");
    mEntries[aSID] = aStyleStruct;
  }

  void SetStyleData(nsStyleStructID aSID,
                    nsPresContext* aPresContext,
                    void* aStyleStruct,
                    const mozilla::RuleNodeCacheConditions& aConditions) {
    MOZ_ASSERT((mConditionalBits & GetBitForSID(aSID)) ||
               !mEntries[aSID],
               "rule node should not have unconditional and conditional style "
               "data for a given struct");
    MOZ_ASSERT(aConditions.CacheableWithDependencies(),
               "don't call SetStyleData with a cache key that has no "
               "conditions or is uncacheable");
#ifdef DEBUG
    for (Entry* e = static_cast<Entry*>(mEntries[aSID]); e; e = e->mNext) {
      NS_WARN_IF_FALSE(e->mConditions != aConditions,
                       "wasteful to have duplicate conditional style data");
    }
#endif

    mConditionalBits |= GetBitForSID(aSID);
    mEntries[aSID] =
      new (aPresContext) Entry(aConditions, aStyleStruct,
                               static_cast<Entry*>(mEntries[aSID]));
  }

  void Destroy(uint64_t aBits, nsPresContext* aContext) {
#define STYLE_STRUCT_RESET(name, checkdata_cb)                                 \
    void* name##Ptr = mEntries[eStyleStruct_##name];                           \
    if (name##Ptr) {                                                           \
      if (!(mConditionalBits & NS_STYLE_INHERIT_BIT(name))) {                  \
        if (!(aBits & NS_STYLE_INHERIT_BIT(name))) {                           \
          static_cast<nsStyle##name*>(name##Ptr)->Destroy(aContext);           \
        }                                                                      \
      } else {                                                                 \
        Entry* e = static_cast<Entry*>(name##Ptr);                             \
        MOZ_ASSERT(e, "if mConditionalBits bit is set, we must have at least " \
                      "one conditional style struct");                         \
        do {                                                                   \
          static_cast<nsStyle##name*>(e->mStyleStruct)->Destroy(aContext);     \
          Entry* next = e->mNext;                                              \
          aContext->PresShell()->FreeByObjectID(                               \
              nsPresArena::nsConditionalResetStyleDataEntry_id, e);            \
          e = next;                                                            \
        } while (e);                                                           \
      }                                                                        \
    }
#define STYLE_STRUCT_INHERITED(name, checkdata_cb)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_RESET
#undef STYLE_STRUCT_INHERITED

    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsConditionalResetStyleData_id, this);
  }

};

struct nsCachedStyleData
{
  nsInheritedStyleData* mInheritedData;
  nsConditionalResetStyleData* mResetData;

  static bool IsReset(const nsStyleStructID aSID) {
    MOZ_ASSERT(0 <= aSID && aSID < nsStyleStructID_Length,
               "must be an inherited or reset SID");
    return nsStyleStructID_Reset_Start <= aSID;
  }

  static bool IsInherited(const nsStyleStructID aSID) {
    return !IsReset(aSID);
  }

  static uint32_t GetBitForSID(const nsStyleStructID aSID) {
    return nsConditionalResetStyleData::GetBitForSID(aSID);
  }

  void* NS_FASTCALL GetStyleData(const nsStyleStructID aSID) {
    if (IsReset(aSID)) {
      if (mResetData) {
        return mResetData->GetStyleData(aSID);
      }
    } else {
      if (mInheritedData) {
        return mInheritedData->mStyleStructs[aSID];
      }
    }
    return nullptr;
  }

  void* NS_FASTCALL GetStyleData(const nsStyleStructID aSID,
                                 nsStyleContext* aStyleContext) {
    if (IsReset(aSID)) {
      if (mResetData) {
        return mResetData->GetStyleData(aSID, aStyleContext);
      }
    } else {
      if (mInheritedData) {
        return mInheritedData->mStyleStructs[aSID];
      }
    }
    return nullptr;
  }

  void NS_FASTCALL SetStyleData(const nsStyleStructID aSID,
                                nsPresContext *aPresContext, void *aData) {
    if (IsReset(aSID)) {
      if (!mResetData) {
        mResetData = new (aPresContext) nsConditionalResetStyleData;
      }
      mResetData->SetStyleData(aSID, aData);
    } else {
      if (!mInheritedData) {
        mInheritedData = new (aPresContext) nsInheritedStyleData;
      }
      mInheritedData->mStyleStructs[aSID] = aData;
    }
  }

  // Typesafe and faster versions of the above
  #define STYLE_STRUCT_INHERITED(name_, checkdata_cb_)                         \
    nsStyle##name_ * NS_FASTCALL GetStyle##name_ () {                          \
      return mInheritedData ? static_cast<nsStyle##name_*>(                    \
        mInheritedData->mStyleStructs[eStyleStruct_##name_]) : nullptr;        \
    }
  #define STYLE_STRUCT_RESET(name_, checkdata_cb_)                             \
    nsStyle##name_ * NS_FASTCALL GetStyle##name_ (nsStyleContext* aContext) {  \
      return mResetData ? static_cast<nsStyle##name_*>(                        \
        mResetData->GetStyleData(eStyleStruct_##name_, aContext)) : nullptr;   \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT_RESET
  #undef STYLE_STRUCT_INHERITED

  void Destroy(uint64_t aBits, nsPresContext* aContext) {
    if (mResetData)
      mResetData->Destroy(aBits, aContext);
    if (mInheritedData)
      mInheritedData->Destroy(aBits, aContext);
    mResetData = nullptr;
    mInheritedData = nullptr;
  }

  nsCachedStyleData() :mInheritedData(nullptr), mResetData(nullptr) {}
  ~nsCachedStyleData() {}
};

/**
 * nsRuleNode is a node in a lexicographic tree (the "rule tree")
 * indexed by style rules (implementations of nsIStyleRule).
 *
 * The rule tree is owned by the nsStyleSet and is destroyed when the
 * presentation of the document goes away.  It is garbage-collected
 * (using mark-and-sweep garbage collection) during the lifetime of the
 * document (when dynamic changes cause the destruction of enough style
 * contexts).  Rule nodes are marked if they are pointed to by a style
 * context or one of their descendants is.
 *
 * An nsStyleContext, which represents the computed style data for an
 * element, points to an nsRuleNode.  The path from the root of the rule
 * tree to the nsStyleContext's mRuleNode gives the list of the rules
 * matched, from least important in the cascading order to most
 * important in the cascading order.
 *
 * The reason for using a lexicographic tree is that it allows for
 * sharing of style data, which saves both memory (for storing the
 * computed style data) and time (for computing them).  This sharing
 * depends on the computed style data being stored in structs (nsStyle*)
 * that contain only properties that are inherited by default
 * ("inherited structs") or structs that contain only properties that
 * are not inherited by default ("reset structs").  The optimization
 * depends on the normal case being that style rules specify relatively
 * few properties and even that elements generally have relatively few
 * properties specified.  This allows sharing in the following ways:
 *   1. [mainly reset structs] When a style data struct will contain the
 *      same computed value for any elements that match the same set of
 *      rules (common for reset structs), it can be stored on the
 *      nsRuleNode instead of on the nsStyleContext.
 *   2. [only? reset structs] When (1) occurs, and an nsRuleNode doesn't
 *      have any rules that change the values in the struct, the
 *      nsRuleNode can share that struct with its parent nsRuleNode.
 *   3. [mainly inherited structs] When an element doesn't match any
 *      rules that change the value of a property (or, in the edge case,
 *      when all the values specified are 'inherit'), the nsStyleContext
 *      can use the same nsStyle* struct as its parent nsStyleContext.
 *
 * Since the data represented by an nsIStyleRule are immutable, the data
 * represented by an nsRuleNode are also immutable.
 */

enum nsFontSizeType {
  eFontSize_HTML = 1,
  eFontSize_CSS = 2
};

class nsRuleNode {
public:
  enum RuleDetail {
    eRuleNone, // No props have been specified at all.
    eRulePartialReset, // At least one prop with a non-"inherit" value
                       // has been specified.  No props have been
                       // specified with an "inherit" value.  At least
                       // one prop remains unspecified.
    eRulePartialMixed, // At least one prop with a non-"inherit" value
                       // has been specified.  Some props may also have
                       // been specified with an "inherit" value.  At
                       // least one prop remains unspecified.
    eRulePartialInherited, // Only props with "inherit" values have
                           // have been specified.  At least one prop
                           // remains unspecified.
    eRuleFullReset, // All props have been specified.  None has an
                    // "inherit" value.
    eRuleFullMixed, // All props have been specified.  At least one has
                    // a non-"inherit" value.
    eRuleFullInherited  // All props have been specified with "inherit"
                        // values.
  };

private:
  nsPresContext* const mPresContext; // Our pres context.

  nsRuleNode* const mParent; // A pointer to the parent node in the tree.
                             // This enables us to walk backwards from the
                             // most specific rule matched to the least
                             // specific rule (which is the optimal order to
                             // use for lookups of style properties.
  nsIStyleRule* const mRule; // [STRONG] A pointer to our specific rule.

  nsRuleNode* mNextSibling; // This value should be used only by the
                            // parent, since the parent may store
                            // children in a hash, which means this
                            // pointer is not meaningful.  Order of
                            // siblings is also not meaningful.

  struct Key {
    nsIStyleRule* mRule;
    uint8_t mLevel;
    bool mIsImportantRule;

    Key(nsIStyleRule* aRule, uint8_t aLevel, bool aIsImportantRule)
      : mRule(aRule), mLevel(aLevel), mIsImportantRule(aIsImportantRule)
    {}

    bool operator==(const Key& aOther) const
    {
      return mRule == aOther.mRule &&
             mLevel == aOther.mLevel &&
             mIsImportantRule == aOther.mIsImportantRule;
    }

    bool operator!=(const Key& aOther) const
    {
      return !(*this == aOther);
    }
  };

  static PLDHashNumber
  ChildrenHashHashKey(PLDHashTable *aTable, const void *aKey);

  static bool
  ChildrenHashMatchEntry(PLDHashTable *aTable,
                         const PLDHashEntryHdr *aHdr,
                         const void *aKey);

  static PLDHashOperator
  SweepHashEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                 uint32_t number, void *arg);
  void SweepChildren(nsTArray<nsRuleNode*>& aSweepQueue);
  bool DestroyIfNotMarked();

  static const PLDHashTableOps ChildrenHashOps;

  static PLDHashOperator
  EnqueueRuleNodeChildren(PLDHashTable *table, PLDHashEntryHdr *hdr,
                          uint32_t number, void *arg);

  Key GetKey() const {
    return Key(mRule, GetLevel(), IsImportantRule());
  }

  // The children of this node are stored in either a hashtable or list
  // that maps from rules to our nsRuleNode children.  When matching
  // rules, we use this mapping to transition from node to node
  // (constructing new nodes as needed to flesh out the tree).

  union {
    void* asVoid;
    nsRuleNode* asList;
    PLDHashTable* asHash;
  } mChildren; // Accessed only through the methods below.

  enum {
    kTypeMask = 0x1,
    kListType = 0x0,
    kHashType = 0x1
  };
  enum {
    // Maximum to have in a list before converting to a hashtable.
    // XXX Need to optimize this.
    kMaxChildrenInList = 32
  };

  bool HaveChildren() const {
    return mChildren.asVoid != nullptr;
  }
  bool ChildrenAreHashed() {
    return (intptr_t(mChildren.asVoid) & kTypeMask) == kHashType;
  }
  nsRuleNode* ChildrenList() {
    return mChildren.asList;
  }
  nsRuleNode** ChildrenListPtr() {
    return &mChildren.asList;
  }
  PLDHashTable* ChildrenHash() {
    return (PLDHashTable*) (intptr_t(mChildren.asHash) & ~intptr_t(kTypeMask));
  }
  void SetChildrenList(nsRuleNode *aList) {
    NS_ASSERTION(!(intptr_t(aList) & kTypeMask),
                 "pointer not 2-byte aligned");
    mChildren.asList = aList;
  }
  void SetChildrenHash(PLDHashTable *aHashtable) {
    NS_ASSERTION(!(intptr_t(aHashtable) & kTypeMask),
                 "pointer not 2-byte aligned");
    mChildren.asHash = (PLDHashTable*)(intptr_t(aHashtable) | kHashType);
  }
  void ConvertChildrenToHash(int32_t aNumKids);

  nsCachedStyleData mStyleData;   // Any data we cached on the rule node.

  uint32_t mDependentBits; // Used to cache the fact that we can look up
                           // cached data under a parent rule.

  uint32_t mNoneBits; // Used to cache the fact that the branch to this
                      // node specifies no non-inherited data for a
                      // given struct type.  (This usually implies that
                      // the entire branch specifies no non-inherited
                      // data, although not necessarily, if a
                      // non-inherited value is overridden by an
                      // explicit 'inherit' value.)  For example, if an
                      // entire rule branch specifies no color
                      // information, then a bit will be set along every
                      // rule node on that branch, so that you can break
                      // out of the rule tree early and just inherit
                      // from the parent style context.  The presence of
                      // this bit means we should just get inherited
                      // data from the parent style context, and it is
                      // never used for reset structs since their
                      // Compute*Data functions don't initialize from
                      // inherited data.

  // Reference count.  This just counts the style contexts that reference this
  // rulenode.  And children the rulenode has had.  When this goes to 0 or
  // stops being 0, we notify the style set.
  // Note, in particular, that when a child is removed mRefCnt is NOT
  // decremented.  This is on purpose; the notifications to the style set are
  // only used to determine when it's worth running GC on the ruletree, and
  // this setup makes it so we only count unused ruletree leaves for purposes
  // of deciding when to GC.  We could more accurately count unused rulenodes
  // by releasing/addrefing our parent when our refcount transitions to or from
  // 0, but it doesn't seem worth it to do that.
  uint32_t mRefCnt;

public:
  // Infallible overloaded new operator that allocates from a presShell arena.
  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy() { DestroyInternal(nullptr); }

  // Implemented in nsStyleSet.h, since it needs to know about nsStyleSet.
  inline void AddRef();

  // Implemented in nsStyleSet.h, since it needs to know about nsStyleSet.
  inline void Release();

protected:
  void DestroyInternal(nsRuleNode ***aDestroyQueueTail);
  void PropagateDependentBit(nsStyleStructID aSID, nsRuleNode* aHighestNode,
                             void* aStruct);
  void PropagateNoneBit(uint32_t aBit, nsRuleNode* aHighestNode);
  static void PropagateGrandancestorBit(nsStyleContext* aContext,
                                        nsStyleContext* aContextInheritedFrom);

  const void* SetDefaultOnRoot(const nsStyleStructID aSID,
                               nsStyleContext* aContext);

  /**
   * Resolves any property values in aRuleData for a given style struct that
   * have eCSSUnit_TokenStream values, by resolving them against the computed
   * variable values on the style context and re-parsing the property.
   *
   * @return Whether any properties with eCSSUnit_TokenStream values were
   *   encountered.
   */
  static bool ResolveVariableReferences(const nsStyleStructID aSID,
                                        nsRuleData* aRuleData,
                                        nsStyleContext* aContext);

  const void*
    WalkRuleTree(const nsStyleStructID aSID, nsStyleContext* aContext);

  const void*
    ComputeDisplayData(void* aStartStruct,
                       const nsRuleData* aRuleData,
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       RuleDetail aRuleDetail,
                       const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeVisibilityData(void* aStartStruct,
                          const nsRuleData* aRuleData,
                          nsStyleContext* aContext, nsRuleNode* aHighestNode,
                          RuleDetail aRuleDetail,
                          const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeFontData(void* aStartStruct,
                    const nsRuleData* aRuleData,
                    nsStyleContext* aContext, nsRuleNode* aHighestNode,
                    RuleDetail aRuleDetail,
                    const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeColorData(void* aStartStruct,
                     const nsRuleData* aRuleData,
                     nsStyleContext* aContext, nsRuleNode* aHighestNode,
                     RuleDetail aRuleDetail,
                     const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeBackgroundData(void* aStartStruct,
                          const nsRuleData* aRuleData,
                          nsStyleContext* aContext, nsRuleNode* aHighestNode,
                          RuleDetail aRuleDetail,
                          const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeMarginData(void* aStartStruct,
                      const nsRuleData* aRuleData,
                      nsStyleContext* aContext, nsRuleNode* aHighestNode,
                      RuleDetail aRuleDetail,
                      const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeBorderData(void* aStartStruct,
                      const nsRuleData* aRuleData,
                      nsStyleContext* aContext, nsRuleNode* aHighestNode,
                      RuleDetail aRuleDetail,
                      const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputePaddingData(void* aStartStruct,
                       const nsRuleData* aRuleData,
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       RuleDetail aRuleDetail,
                       const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeOutlineData(void* aStartStruct,
                       const nsRuleData* aRuleData,
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       RuleDetail aRuleDetail,
                       const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeListData(void* aStartStruct,
                    const nsRuleData* aRuleData,
                    nsStyleContext* aContext, nsRuleNode* aHighestNode,
                    RuleDetail aRuleDetail,
                    const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputePositionData(void* aStartStruct,
                        const nsRuleData* aRuleData,
                        nsStyleContext* aContext, nsRuleNode* aHighestNode,
                        RuleDetail aRuleDetail,
                        const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeTableData(void* aStartStruct,
                     const nsRuleData* aRuleData,
                     nsStyleContext* aContext, nsRuleNode* aHighestNode,
                     RuleDetail aRuleDetail,
                     const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeTableBorderData(void* aStartStruct,
                           const nsRuleData* aRuleData,
                           nsStyleContext* aContext, nsRuleNode* aHighestNode,
                           RuleDetail aRuleDetail,
                           const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeContentData(void* aStartStruct,
                       const nsRuleData* aRuleData,
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       RuleDetail aRuleDetail,
                       const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeQuotesData(void* aStartStruct,
                      const nsRuleData* aRuleData,
                      nsStyleContext* aContext, nsRuleNode* aHighestNode,
                      RuleDetail aRuleDetail,
                      const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeTextData(void* aStartStruct,
                    const nsRuleData* aRuleData,
                    nsStyleContext* aContext, nsRuleNode* aHighestNode,
                    RuleDetail aRuleDetail,
                    const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeTextResetData(void* aStartStruct,
                         const nsRuleData* aRuleData,
                         nsStyleContext* aContext, nsRuleNode* aHighestNode,
                         RuleDetail aRuleDetail,
                         const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeUserInterfaceData(void* aStartStruct,
                             const nsRuleData* aRuleData,
                             nsStyleContext* aContext,
                             nsRuleNode* aHighestNode,
                             RuleDetail aRuleDetail,
                             const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeUIResetData(void* aStartStruct,
                       const nsRuleData* aRuleData,
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       RuleDetail aRuleDetail,
                       const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeXULData(void* aStartStruct,
                   const nsRuleData* aRuleData,
                   nsStyleContext* aContext, nsRuleNode* aHighestNode,
                   RuleDetail aRuleDetail,
                   const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeColumnData(void* aStartStruct,
                      const nsRuleData* aRuleData,
                      nsStyleContext* aContext, nsRuleNode* aHighestNode,
                      RuleDetail aRuleDetail,
                      const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeSVGData(void* aStartStruct,
                   const nsRuleData* aRuleData,
                   nsStyleContext* aContext, nsRuleNode* aHighestNode,
                   RuleDetail aRuleDetail,
                   const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeSVGResetData(void* aStartStruct,
                        const nsRuleData* aRuleData,
                        nsStyleContext* aContext, nsRuleNode* aHighestNode,
                        RuleDetail aRuleDetail,
                        const mozilla::RuleNodeCacheConditions aConditions);

  const void*
    ComputeVariablesData(void* aStartStruct,
                         const nsRuleData* aRuleData,
                         nsStyleContext* aContext, nsRuleNode* aHighestNode,
                         RuleDetail aRuleDetail,
                         const mozilla::RuleNodeCacheConditions aConditions);

  // helpers for |ComputeFontData| that need access to |mNoneBits|:
  static void SetFontSize(nsPresContext* aPresContext,
                          const nsRuleData* aRuleData,
                          const nsStyleFont* aFont,
                          const nsStyleFont* aParentFont,
                          nscoord* aSize,
                          const nsFont& aSystemFont,
                          nscoord aParentSize,
                          nscoord aScriptLevelAdjustedParentSize,
                          bool aUsedStartStruct,
                          bool aAtRoot,
                          mozilla::RuleNodeCacheConditions& aConditions);

  static void SetFont(nsPresContext* aPresContext,
                      nsStyleContext* aContext,
                      uint8_t aGenericFontID,
                      const nsRuleData* aRuleData,
                      const nsStyleFont* aParentFont,
                      nsStyleFont* aFont,
                      bool aStartStruct,
                      mozilla::RuleNodeCacheConditions& aConditions);

  static void SetGenericFont(nsPresContext* aPresContext,
                             nsStyleContext* aContext,
                             uint8_t aGenericFontID,
                             nsStyleFont* aFont);

  inline RuleDetail CheckSpecifiedProperties(const nsStyleStructID aSID,
                                             const nsRuleData* aRuleData);

  already_AddRefed<nsCSSShadowArray>
              GetShadowData(const nsCSSValueList* aList,
                            nsStyleContext* aContext,
                            bool aIsBoxShadow,
                            mozilla::RuleNodeCacheConditions& aConditions);
  bool SetStyleFilterToCSSValue(nsStyleFilter* aStyleFilter,
                                const nsCSSValue& aValue,
                                nsStyleContext* aStyleContext,
                                nsPresContext* aPresContext,
                                mozilla::RuleNodeCacheConditions& aConditions);
  void SetStyleClipPathToCSSValue(nsStyleClipPath* aStyleClipPath,
                                  const nsCSSValue* aValue,
                                  nsStyleContext* aStyleContext,
                                  nsPresContext* aPresContext,
                                  mozilla::RuleNodeCacheConditions& aConditions);

private:
  nsRuleNode(nsPresContext* aPresContext, nsRuleNode* aParent,
             nsIStyleRule* aRule, uint8_t aLevel, bool aIsImportant);
  ~nsRuleNode();

public:
  // This is infallible; it will never return nullptr.
  static nsRuleNode* CreateRootNode(nsPresContext* aPresContext);

  static void EnsureBlockDisplay(uint8_t& display,
                                 bool aConvertListItem = false);
  static void EnsureInlineDisplay(uint8_t& display);

  // Transition never returns null; on out of memory it'll just return |this|.
  nsRuleNode* Transition(nsIStyleRule* aRule, uint8_t aLevel,
                         bool aIsImportantRule);
  nsRuleNode* GetParent() const { return mParent; }
  bool IsRoot() const { return mParent == nullptr; }

  // Return the root of the rule tree that this rule node is in.
  nsRuleNode* RuleTree();
  const nsRuleNode* RuleTree() const {
    return const_cast<nsRuleNode*>(this)->RuleTree();
  }

  // These uint8_ts are really nsStyleSet::sheetType values.
  uint8_t GetLevel() const {
    NS_ASSERTION(!IsRoot(), "can't call on root");
    return (mDependentBits & NS_RULE_NODE_LEVEL_MASK) >>
             NS_RULE_NODE_LEVEL_SHIFT;
  }
  bool IsImportantRule() const {
    NS_ASSERTION(!IsRoot(), "can't call on root");
    return (mDependentBits & NS_RULE_NODE_IS_IMPORTANT) != 0;
  }

  /**
   * Has this rule node at some time in its lifetime been the mRuleNode
   * of some style context (as opposed to only being the ancestor of
   * some style context's mRuleNode)?
   */
  void SetUsedDirectly();
  bool IsUsedDirectly() const {
    return (mDependentBits & NS_RULE_NODE_USED_DIRECTLY) != 0;
  }

  /**
   * Is the mRule of this rule node an AnimValuesStyleRule?
   */
  void SetIsAnimationRule() {
    MOZ_ASSERT(!HaveChildren() ||
               (mDependentBits & NS_RULE_NODE_IS_ANIMATION_RULE),
               "SetIsAnimationRule must only set the IS_ANIMATION_RULE bit "
               "before the rule node has children");
    mDependentBits |= NS_RULE_NODE_IS_ANIMATION_RULE;
    mNoneBits |= NS_RULE_NODE_HAS_ANIMATION_DATA;
  }
  bool IsAnimationRule() const {
    return (mDependentBits & NS_RULE_NODE_IS_ANIMATION_RULE) != 0;
  }

  /**
   * Is the mRule of this rule node or any of its ancestors an
   * AnimValuesStyleRule?
   */
  bool HasAnimationData() const {
    return (mNoneBits & NS_RULE_NODE_HAS_ANIMATION_DATA) != 0;
  }

  // NOTE:  Does not |AddRef|.  Null only for the root.
  nsIStyleRule* GetRule() const { return mRule; }
  // NOTE: Does not |AddRef|.  Never null.
  nsPresContext* PresContext() const { return mPresContext; }

  const void* GetStyleData(nsStyleStructID aSID,
                           nsStyleContext* aContext,
                           bool aComputeData);


  // See comments in GetStyleData for an explanation of what the
  // code below does.
  #define STYLE_STRUCT_INHERITED(name_, checkdata_cb_)                        \
  template<bool aComputeData>                                                 \
  const nsStyle##name_*                                                       \
  GetStyle##name_(nsStyleContext* aContext)                                   \
  {                                                                           \
    NS_ASSERTION(IsUsedDirectly(),                                            \
                 "if we ever call this on rule nodes that aren't used "       \
                 "directly, we should adjust handling of mDependentBits "     \
                 "in some way.");                                             \
                                                                              \
    const nsStyle##name_ *data;                                               \
                                                                              \
    /* Never use cached data for animated style inside a pseudo-element; */   \
    /* see comment on cacheability in AnimValuesStyleRule::MapRuleInfoInto */ \
    if (!(HasAnimationData() && ParentHasPseudoElementData(aContext))) {      \
      data = mStyleData.GetStyle##name_();                                    \
      if (MOZ_LIKELY(data != nullptr))                                        \
        return data;                                                          \
    }                                                                         \
                                                                              \
    if (!aComputeData)                                                        \
      return nullptr;                                                         \
                                                                              \
    data = static_cast<const nsStyle##name_ *>                                \
             (WalkRuleTree(eStyleStruct_##name_, aContext));                  \
                                                                              \
    MOZ_ASSERT(data, "should have aborted on out-of-memory");                 \
    return data;                                                              \
  }

  #define STYLE_STRUCT_RESET(name_, checkdata_cb_)                            \
  template<bool aComputeData>                                                 \
  const nsStyle##name_*                                                       \
  GetStyle##name_(nsStyleContext* aContext)                                   \
  {                                                                           \
    NS_ASSERTION(IsUsedDirectly(),                                            \
                 "if we ever call this on rule nodes that aren't used "       \
                 "directly, we should adjust handling of mDependentBits "     \
                 "in some way.");                                             \
                                                                              \
    const nsStyle##name_ *data;                                               \
                                                                              \
    /* Never use cached data for animated style inside a pseudo-element; */   \
    /* see comment on cacheability in AnimValuesStyleRule::MapRuleInfoInto */ \
    if (!(HasAnimationData() && ParentHasPseudoElementData(aContext))) {      \
      data = mStyleData.GetStyle##name_(aContext);                            \
      if (MOZ_LIKELY(data != nullptr))                                        \
        return data;                                                          \
    }                                                                         \
                                                                              \
    if (!aComputeData)                                                        \
      return nullptr;                                                         \
                                                                              \
    data = static_cast<const nsStyle##name_ *>                                \
             (WalkRuleTree(eStyleStruct_##name_, aContext));                  \
                                                                              \
    MOZ_ASSERT(data, "should have aborted on out-of-memory");                 \
    return data;                                                              \
  }

  #include "nsStyleStructList.h"

  #undef STYLE_STRUCT_RESET
  #undef STYLE_STRUCT_INHERITED

  /*
   * Garbage collection.  Mark walks up the tree, marking any unmarked
   * ancestors until it reaches a marked one.  Sweep recursively sweeps
   * the children, destroys any that are unmarked, and clears marks,
   * returning true if the node on which it was called was destroyed.
   * If children are hashed, the mNextSibling field on the children is
   * temporarily used internally by Sweep.
   */
  void Mark();
  bool Sweep();

  static bool
    HasAuthorSpecifiedRules(nsStyleContext* aStyleContext,
                            uint32_t ruleTypeMask,
                            bool aAuthorColorsAllowed);

  /**
   * Fill in to aPropertiesOverridden all of the properties in aProperties
   * that, for this rule node, have a declaration that is higher than the
   * animation level in the CSS Cascade.
   */
  static void
  ComputePropertiesOverridingAnimation(
                              const nsTArray<nsCSSProperty>& aProperties,
                              nsStyleContext* aStyleContext,
                              nsCSSPropertySet& aPropertiesOverridden);

  // Expose this so media queries can use it
  static nscoord CalcLengthWithInitialFont(nsPresContext* aPresContext,
                                           const nsCSSValue& aValue);
  // Expose this so nsTransformFunctions can use it.
  static nscoord CalcLength(const nsCSSValue& aValue,
                            nsStyleContext* aStyleContext,
                            nsPresContext* aPresContext,
                            mozilla::RuleNodeCacheConditions& aConditions);

  struct ComputedCalc {
    nscoord mLength;
    float mPercent;

    ComputedCalc(nscoord aLength, float aPercent)
      : mLength(aLength), mPercent(aPercent) {}
  };
  static ComputedCalc
  SpecifiedCalcToComputedCalc(const nsCSSValue& aValue,
                              nsStyleContext* aStyleContext,
                              nsPresContext* aPresContext,
                              mozilla::RuleNodeCacheConditions& aConditions);

  // Compute the value of an nsStyleCoord that IsCalcUnit().
  // (Values that don't require aPercentageBasis should be handled
  // inside nsRuleNode rather than through this API.)
  static nscoord ComputeComputedCalc(const nsStyleCoord& aCoord,
                                     nscoord aPercentageBasis);

  // Compute the value of an nsStyleCoord that is either a coord, a
  // percent, or a calc expression.
  static nscoord ComputeCoordPercentCalc(const nsStyleCoord& aCoord,
                                         nscoord aPercentageBasis);

  // Return whether the rule tree for which this node is the root has
  // cached data such that we need to do dynamic change handling for
  // changes that change the results of media queries or require
  // rebuilding all style data.
  bool TreeHasCachedData() const {
    NS_ASSERTION(IsRoot(), "should only be called on root of rule tree");
    return HaveChildren() || mStyleData.mInheritedData || mStyleData.mResetData;
  }

  // Note that this will return false if we have cached conditional
  // style structs.
  bool NodeHasCachedUnconditionalData(const nsStyleStructID aSID) {
    return !!mStyleData.GetStyleData(aSID);
  }

  static void ComputeFontFeatures(const nsCSSValuePairList *aFeaturesList,
                                  nsTArray<gfxFontFeature>& aFeatureSettings);

  static nscoord CalcFontPointSize(int32_t aHTMLSize, int32_t aBasePointSize, 
                                   nsPresContext* aPresContext,
                                   nsFontSizeType aFontSizeType = eFontSize_HTML);

  static nscoord FindNextSmallerFontSize(nscoord aFontSize, int32_t aBasePointSize, 
                                         nsPresContext* aPresContext,
                                         nsFontSizeType aFontSizeType = eFontSize_HTML);

  static nscoord FindNextLargerFontSize(nscoord aFontSize, int32_t aBasePointSize, 
                                        nsPresContext* aPresContext,
                                        nsFontSizeType aFontSizeType = eFontSize_HTML);

  /**
   * @param aValue The color value, returned from nsCSSParser::ParseColorString
   * @param aPresContext Presentation context whose preferences are used
   *                     for certain enumerated colors
   * @param aStyleContext Style context whose color is used for 'currentColor'
   *
   * @note aPresContext and aStyleContext may be null, but in that case, fully
   *       opaque black will be returned for the values that rely on these
   *       objects to compute the color. (For example, -moz-hyperlinktext.)
   *
   * @return false if we fail to extract a color; this will not happen if both
   *         aPresContext and aStyleContext are non-null
   */
  static bool ComputeColor(const nsCSSValue& aValue,
                           nsPresContext* aPresContext,
                           nsStyleContext* aStyleContext,
                           nscolor& aResult);

  static bool ParentHasPseudoElementData(nsStyleContext* aContext);
};

#endif
