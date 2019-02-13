/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsReadableUtils.h"
#include "nsTreeUtils.h"
#include "ChildIterator.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"

using namespace mozilla;

nsresult
nsTreeUtils::TokenizeProperties(const nsAString& aProperties, AtomArray & aPropertiesArray)
{
  nsAString::const_iterator end;
  aProperties.EndReading(end);

  nsAString::const_iterator iter;
  aProperties.BeginReading(iter);

  do {
    // Skip whitespace
    while (iter != end && nsCRT::IsAsciiSpace(*iter))
      ++iter;

    // If only whitespace, we're done
    if (iter == end)
      break;

    // Note the first non-whitespace character
    nsAString::const_iterator first = iter;

    // Advance to the next whitespace character
    while (iter != end && ! nsCRT::IsAsciiSpace(*iter))
      ++iter;

    // XXX this would be nonsensical
    NS_ASSERTION(iter != first, "eh? something's wrong here");
    if (iter == first)
      break;

    nsCOMPtr<nsIAtom> atom = do_GetAtom(Substring(first, iter));
    aPropertiesArray.AppendElement(atom);
  } while (iter != end);

  return NS_OK;
}

nsIContent*
nsTreeUtils::GetImmediateChild(nsIContent* aContainer, nsIAtom* aTag)
{
  dom::FlattenedChildIterator iter(aContainer);
  for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
    if (child->IsXULElement(aTag)) {
      return child;
    }
  }

  return nullptr;
}

nsIContent*
nsTreeUtils::GetDescendantChild(nsIContent* aContainer, nsIAtom* aTag)
{
  dom::FlattenedChildIterator iter(aContainer);
  for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
    if (child->IsXULElement(aTag)) {
      return child;
    }

    child = GetDescendantChild(child, aTag);
    if (child) {
      return child;
    }
  }

  return nullptr;
}

nsresult
nsTreeUtils::UpdateSortIndicators(nsIContent* aColumn, const nsAString& aDirection)
{
  aColumn->SetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection, aDirection, true);
  aColumn->SetAttr(kNameSpaceID_None, nsGkAtoms::sortActive, NS_LITERAL_STRING("true"), true);

  // Unset sort attribute(s) on the other columns
  nsCOMPtr<nsIContent> parentContent = aColumn->GetParent();
  if (parentContent &&
      parentContent->NodeInfo()->Equals(nsGkAtoms::treecols,
                                        kNameSpaceID_XUL)) {
    uint32_t i, numChildren = parentContent->GetChildCount();
    for (i = 0; i < numChildren; ++i) {
      nsCOMPtr<nsIContent> childContent = parentContent->GetChildAt(i);

      if (childContent &&
          childContent != aColumn &&
          childContent->NodeInfo()->Equals(nsGkAtoms::treecol,
                                           kNameSpaceID_XUL)) {
        childContent->UnsetAttr(kNameSpaceID_None,
                                nsGkAtoms::sortDirection, true);
        childContent->UnsetAttr(kNameSpaceID_None,
                                nsGkAtoms::sortActive, true);
      }
    }
  }

  return NS_OK;
}

nsresult
nsTreeUtils::GetColumnIndex(nsIContent* aColumn, int32_t* aResult)
{
  nsIContent* parentContent = aColumn->GetParent();
  if (parentContent &&
      parentContent->NodeInfo()->Equals(nsGkAtoms::treecols,
                                        kNameSpaceID_XUL)) {
    uint32_t i, numChildren = parentContent->GetChildCount();
    int32_t colIndex = 0;
    for (i = 0; i < numChildren; ++i) {
      nsIContent *childContent = parentContent->GetChildAt(i);
      if (childContent &&
          childContent->NodeInfo()->Equals(nsGkAtoms::treecol,
                                           kNameSpaceID_XUL)) {
        if (childContent == aColumn) {
          *aResult = colIndex;
          return NS_OK;
        }
        ++colIndex;
      }
    }
  }

  *aResult = -1;
  return NS_OK;
}
