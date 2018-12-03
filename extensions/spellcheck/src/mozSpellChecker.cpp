/* vim: set ts=2 sts=2 sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozSpellChecker.h"
#include "nsIServiceManager.h"
#include "nsIStringEnumerator.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsISimpleEnumerator.h"
#include "mozEnglishWordUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/PRemoteSpellcheckEngineChild.h"
#include "mozilla/TextServicesDocument.h"
#include "nsXULAppAPI.h"
#include "RemoteSpellCheckEngineChild.h"

using mozilla::GenericPromise;
using mozilla::PRemoteSpellcheckEngineChild;
using mozilla::RemoteSpellcheckEngineChild;
using mozilla::TextServicesDocument;
using mozilla::dom::ContentChild;

#define DEFAULT_SPELL_CHECKER "@mozilla.org/spellchecker/engine;1"

NS_IMPL_CYCLE_COLLECTION(mozSpellChecker, mTextServicesDocument,
                         mPersonalDictionary)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(mozSpellChecker, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(mozSpellChecker, Release)

mozSpellChecker::mozSpellChecker() : mEngine(nullptr) {}

mozSpellChecker::~mozSpellChecker() {
  if (mPersonalDictionary) {
    //    mPersonalDictionary->Save();
    mPersonalDictionary->EndSession();
  }
  mSpellCheckingEngine = nullptr;
  mPersonalDictionary = nullptr;

  if (mEngine) {
    MOZ_ASSERT(XRE_IsContentProcess());
    RemoteSpellcheckEngineChild::Send__delete__(mEngine);
    MOZ_ASSERT(!mEngine);
  }
}

nsresult mozSpellChecker::Init() {
  mSpellCheckingEngine = nullptr;
  if (XRE_IsContentProcess()) {
    mozilla::dom::ContentChild *contentChild =
        mozilla::dom::ContentChild::GetSingleton();
    MOZ_ASSERT(contentChild);
    mEngine = new RemoteSpellcheckEngineChild(this);
    contentChild->SendPRemoteSpellcheckEngineConstructor(mEngine);
  } else {
    mPersonalDictionary =
        do_GetService("@mozilla.org/spellchecker/personaldictionary;1");
  }

  return NS_OK;
}

TextServicesDocument *mozSpellChecker::GetTextServicesDocument() {
  return mTextServicesDocument;
}

nsresult mozSpellChecker::SetDocument(
    TextServicesDocument *aTextServicesDocument, bool aFromStartofDoc) {
  mTextServicesDocument = aTextServicesDocument;
  mFromStart = aFromStartofDoc;
  return NS_OK;
}

nsresult mozSpellChecker::NextMisspelledWord(nsAString &aWord,
                                             nsTArray<nsString> *aSuggestions) {
  if (!aSuggestions || !mConverter) return NS_ERROR_NULL_POINTER;

  int32_t selOffset;
  int32_t begin, end;
  nsresult result;
  result = SetupDoc(&selOffset);
  bool isMisspelled, done;
  if (NS_FAILED(result)) return result;

  while (NS_SUCCEEDED(mTextServicesDocument->IsDone(&done)) && !done) {
    nsString str;
    result = mTextServicesDocument->GetCurrentTextBlock(&str);

    if (NS_FAILED(result)) {
      return result;
    }
    do {
      result = mConverter->FindNextWord(str.get(), str.Length(), selOffset,
                                        &begin, &end);
      if (NS_SUCCEEDED(result) && begin != -1) {
        const nsAString &currWord = Substring(str, begin, end - begin);
        result = CheckWord(currWord, &isMisspelled, aSuggestions);
        if (isMisspelled) {
          aWord = currWord;
          mTextServicesDocument->SetSelection(begin, end - begin);
          // After ScrollSelectionIntoView(), the pending notifications might
          // be flushed and PresShell/PresContext/Frames may be dead.
          // See bug 418470.
          mTextServicesDocument->ScrollSelectionIntoView();
          return NS_OK;
        }
      }
      selOffset = end;
    } while (end != -1);
    mTextServicesDocument->NextBlock();
    selOffset = 0;
  }
  return NS_OK;
}

nsresult mozSpellChecker::CheckWord(const nsAString &aWord, bool *aIsMisspelled,
                                    nsTArray<nsString> *aSuggestions) {
  nsresult result;
  bool correct;

  if (XRE_IsContentProcess()) {
    nsString wordwrapped = nsString(aWord);
    bool rv;
    if (aSuggestions) {
      rv = mEngine->SendCheckAndSuggest(wordwrapped, aIsMisspelled,
                                        aSuggestions);
    } else {
      rv = mEngine->SendCheck(wordwrapped, aIsMisspelled);
    }
    return rv ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  if (!mSpellCheckingEngine) {
    return NS_ERROR_NULL_POINTER;
  }
  *aIsMisspelled = false;
  result = mSpellCheckingEngine->Check(aWord, &correct);
  NS_ENSURE_SUCCESS(result, result);
  if (!correct) {
    if (aSuggestions) {
      uint32_t count, i;
      char16_t **words;

      result = mSpellCheckingEngine->Suggest(aWord, &words, &count);
      NS_ENSURE_SUCCESS(result, result);
      nsString *suggestions = aSuggestions->AppendElements(count);
      for (i = 0; i < count; i++) {
        suggestions[i].Assign(words[i]);
      }

      if (count) NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, words);
    }
    *aIsMisspelled = true;
  }
  return NS_OK;
}

nsresult mozSpellChecker::Replace(const nsAString &aOldWord,
                                  const nsAString &aNewWord,
                                  bool aAllOccurrences) {
  if (!mConverter) return NS_ERROR_NULL_POINTER;

  nsAutoString newWord(aNewWord);  // sigh

  if (aAllOccurrences) {
    int32_t selOffset;
    int32_t startBlock, currentBlock, currOffset;
    int32_t begin, end;
    bool done;
    nsresult result;
    nsAutoString str;

    // find out where we are
    result = SetupDoc(&selOffset);
    if (NS_FAILED(result)) return result;
    result = GetCurrentBlockIndex(mTextServicesDocument, &startBlock);
    if (NS_FAILED(result)) return result;

    // start at the beginning
    result = mTextServicesDocument->FirstBlock();
    currOffset = 0;
    currentBlock = 0;
    while (NS_SUCCEEDED(mTextServicesDocument->IsDone(&done)) && !done) {
      result = mTextServicesDocument->GetCurrentTextBlock(&str);
      do {
        result = mConverter->FindNextWord(str.get(), str.Length(), currOffset,
                                          &begin, &end);
        if (NS_SUCCEEDED(result) && (begin != -1)) {
          if (aOldWord.Equals(Substring(str, begin, end - begin))) {
            // if we are before the current selection point but in the same
            // block move the selection point forwards
            if (currentBlock == startBlock && begin < selOffset) {
              selOffset +=
                  int32_t(aNewWord.Length()) - int32_t(aOldWord.Length());
              if (selOffset < begin) {
                selOffset = begin;
              }
            }
            mTextServicesDocument->SetSelection(begin, end - begin);
            mTextServicesDocument->InsertText(&newWord);
            mTextServicesDocument->GetCurrentTextBlock(&str);
            end += (aNewWord.Length() -
                    aOldWord.Length());  // recursion was cute in GEB, not here.
          }
        }
        currOffset = end;
      } while (currOffset != -1);
      mTextServicesDocument->NextBlock();
      currentBlock++;
      currOffset = 0;
    }

    // We are done replacing.  Put the selection point back where we found  it
    // (or equivalent);
    result = mTextServicesDocument->FirstBlock();
    currentBlock = 0;
    while (NS_SUCCEEDED(mTextServicesDocument->IsDone(&done)) && !done &&
           currentBlock < startBlock) {
      mTextServicesDocument->NextBlock();
    }

    // After we have moved to the block where the first occurrence of replace
    // was done, put the selection to the next word following it. In case there
    // is no word following it i.e if it happens to be the last word in that
    // block, then move to the next block and put the selection to the first
    // word in that block, otherwise when the Setupdoc() is called, it queries
    // the LastSelectedBlock() and the selection offset of the last occurrence
    // of the replaced word is taken instead of the first occurrence and things
    // get messed up as reported in the bug 244969

    if (NS_SUCCEEDED(mTextServicesDocument->IsDone(&done)) && !done) {
      nsString str;
      result = mTextServicesDocument->GetCurrentTextBlock(&str);
      result = mConverter->FindNextWord(str.get(), str.Length(), selOffset,
                                        &begin, &end);
      if (end == -1) {
        mTextServicesDocument->NextBlock();
        selOffset = 0;
        result = mTextServicesDocument->GetCurrentTextBlock(&str);
        result = mConverter->FindNextWord(str.get(), str.Length(), selOffset,
                                          &begin, &end);
        mTextServicesDocument->SetSelection(begin, 0);
      } else {
        mTextServicesDocument->SetSelection(begin, 0);
      }
    }
  } else {
    mTextServicesDocument->InsertText(&newWord);
  }
  return NS_OK;
}

nsresult mozSpellChecker::IgnoreAll(const nsAString &aWord) {
  if (mPersonalDictionary) {
    mPersonalDictionary->IgnoreWord(aWord);
  }
  return NS_OK;
}

nsresult mozSpellChecker::AddWordToPersonalDictionary(const nsAString &aWord) {
  nsresult res;
  if (NS_WARN_IF(!mPersonalDictionary)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  res = mPersonalDictionary->AddWord(aWord);
  return res;
}

nsresult mozSpellChecker::RemoveWordFromPersonalDictionary(
    const nsAString &aWord) {
  nsresult res;
  if (NS_WARN_IF(!mPersonalDictionary)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  res = mPersonalDictionary->RemoveWord(aWord);
  return res;
}

nsresult mozSpellChecker::GetPersonalDictionary(nsTArray<nsString> *aWordList) {
  if (!aWordList || !mPersonalDictionary) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIStringEnumerator> words;
  mPersonalDictionary->GetWordList(getter_AddRefs(words));

  bool hasMore;
  nsAutoString word;
  while (NS_SUCCEEDED(words->HasMore(&hasMore)) && hasMore) {
    words->GetNext(word);
    aWordList->AppendElement(word);
  }
  return NS_OK;
}

nsresult mozSpellChecker::GetDictionaryList(
    nsTArray<nsString> *aDictionaryList) {
  if (XRE_IsContentProcess()) {
    ContentChild *child = ContentChild::GetSingleton();
    child->GetAvailableDictionaries(*aDictionaryList);
    return NS_OK;
  }

  nsresult rv;

  // For catching duplicates
  nsTHashtable<nsStringHashKey> dictionaries;

  nsCOMArray<mozISpellCheckingEngine> spellCheckingEngines;
  rv = GetEngineList(&spellCheckingEngines);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int32_t i = 0; i < spellCheckingEngines.Count(); i++) {
    nsCOMPtr<mozISpellCheckingEngine> engine = spellCheckingEngines[i];

    uint32_t count = 0;
    char16_t **words = nullptr;
    engine->GetDictionaryList(&words, &count);
    for (uint32_t k = 0; k < count; k++) {
      nsAutoString dictName;

      dictName.Assign(words[k]);

      // Skip duplicate dictionaries. Only take the first one
      // for each name.
      if (dictionaries.Contains(dictName)) continue;

      dictionaries.PutEntry(dictName);

      if (!aDictionaryList->AppendElement(dictName)) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, words);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, words);
  }

  return NS_OK;
}

nsresult mozSpellChecker::GetCurrentDictionary(nsAString &aDictionary) {
  if (XRE_IsContentProcess()) {
    aDictionary = mCurrentDictionary;
    return NS_OK;
  }

  if (!mSpellCheckingEngine) {
    aDictionary.Truncate();
    return NS_OK;
  }

  return mSpellCheckingEngine->GetDictionary(aDictionary);
}

nsresult mozSpellChecker::SetCurrentDictionary(const nsAString &aDictionary) {
  if (XRE_IsContentProcess()) {
    nsString wrappedDict = nsString(aDictionary);
    bool isSuccess;
    mEngine->SendSetDictionary(wrappedDict, &isSuccess);
    if (!isSuccess) {
      mCurrentDictionary.Truncate();
      return NS_ERROR_NOT_AVAILABLE;
    }

    mCurrentDictionary = wrappedDict;
    return NS_OK;
  }

  // Calls to mozISpellCheckingEngine::SetDictionary might destroy us
  RefPtr<mozSpellChecker> kungFuDeathGrip = this;

  mSpellCheckingEngine = nullptr;

  if (aDictionary.IsEmpty()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMArray<mozISpellCheckingEngine> spellCheckingEngines;
  rv = GetEngineList(&spellCheckingEngines);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int32_t i = 0; i < spellCheckingEngines.Count(); i++) {
    // We must set mSpellCheckingEngine before we call SetDictionary, since
    // SetDictionary calls back to this spell checker to check if the
    // dictionary was set
    mSpellCheckingEngine = spellCheckingEngines[i];

    rv = mSpellCheckingEngine->SetDictionary(aDictionary);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<mozIPersonalDictionary> personalDictionary =
          do_GetService("@mozilla.org/spellchecker/personaldictionary;1");
      mSpellCheckingEngine->SetPersonalDictionary(personalDictionary.get());

      mConverter = new mozEnglishWordUtils;
      return NS_OK;
    }
  }

  mSpellCheckingEngine = nullptr;

  // We could not find any engine with the requested dictionary
  return NS_ERROR_NOT_AVAILABLE;
}

RefPtr<GenericPromise> mozSpellChecker::SetCurrentDictionaryFromList(
    const nsTArray<nsString> &aList) {
  if (aList.IsEmpty()) {
    return GenericPromise::CreateAndReject(NS_ERROR_INVALID_ARG, __func__);
  }

  if (XRE_IsContentProcess()) {
    // mCurrentDictionary will be set by RemoteSpellCheckEngineChild
    return mEngine->SetCurrentDictionaryFromList(aList);
  }

  for (auto &dictionary : aList) {
    nsresult rv = SetCurrentDictionary(dictionary);
    if (NS_SUCCEEDED(rv)) {
      return GenericPromise::CreateAndResolve(true, __func__);
    }
  }
  // We could not find any engine with the requested dictionary
  return GenericPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
}

nsresult mozSpellChecker::SetupDoc(int32_t *outBlockOffset) {
  nsresult rv;

  TextServicesDocument::BlockSelectionStatus blockStatus;
  int32_t selOffset;
  int32_t selLength;
  *outBlockOffset = 0;

  if (!mFromStart) {
    rv = mTextServicesDocument->LastSelectedBlock(&blockStatus, &selOffset,
                                                  &selLength);
    if (NS_SUCCEEDED(rv) &&
        blockStatus !=
            TextServicesDocument::BlockSelectionStatus::eBlockNotFound) {
      switch (blockStatus) {
        // No TB in S, but found one before/after S.
        case TextServicesDocument::BlockSelectionStatus::eBlockOutside:
        // S begins or ends in TB but extends outside of TB.
        case TextServicesDocument::BlockSelectionStatus::eBlockPartial:
          // the TS doc points to the block we want.
          *outBlockOffset = selOffset + selLength;
          break;

        // S extends beyond the start and end of TB.
        case TextServicesDocument::BlockSelectionStatus::eBlockInside:
          // we want the block after this one.
          rv = mTextServicesDocument->NextBlock();
          *outBlockOffset = 0;
          break;

        // TB contains entire S.
        case TextServicesDocument::BlockSelectionStatus::eBlockContains:
          *outBlockOffset = selOffset + selLength;
          break;

        // There is no text block (TB) in or before the selection (S).
        case TextServicesDocument::BlockSelectionStatus::eBlockNotFound:
        default:
          MOZ_ASSERT_UNREACHABLE("Shouldn't ever get this status");
      }
    }
    // Failed to get last sel block. Just start at beginning
    else {
      rv = mTextServicesDocument->FirstBlock();
      *outBlockOffset = 0;
    }

  }
  // We want the first block
  else {
    rv = mTextServicesDocument->FirstBlock();
    mFromStart = false;
  }
  return rv;
}

// utility method to discover which block we're in. The TSDoc interface doesn't
// give us this, because it can't assume a read-only document. shamelessly
// stolen from nsTextServicesDocument
nsresult mozSpellChecker::GetCurrentBlockIndex(
    TextServicesDocument *aTextServicesDocument, int32_t *aOutBlockIndex) {
  int32_t blockIndex = 0;
  bool isDone = false;
  nsresult result = NS_OK;

  do {
    aTextServicesDocument->PrevBlock();
    result = aTextServicesDocument->IsDone(&isDone);
    if (!isDone) {
      blockIndex++;
    }
  } while (NS_SUCCEEDED(result) && !isDone);

  *aOutBlockIndex = blockIndex;

  return result;
}

nsresult mozSpellChecker::GetEngineList(
    nsCOMArray<mozISpellCheckingEngine> *aSpellCheckingEngines) {
  MOZ_ASSERT(!XRE_IsContentProcess());

  nsresult rv;
  bool hasMoreEngines;

  nsCOMPtr<nsICategoryManager> catMgr =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMgr) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsISimpleEnumerator> catEntries;

  // Get contract IDs of registrated external spell-check engines and
  // append one of HunSpell at the end.
  rv = catMgr->EnumerateCategory("spell-check-engine",
                                 getter_AddRefs(catEntries));
  if (NS_FAILED(rv)) return rv;

  while (NS_SUCCEEDED(catEntries->HasMoreElements(&hasMoreEngines)) &&
         hasMoreEngines) {
    nsCOMPtr<nsISupports> elem;
    rv = catEntries->GetNext(getter_AddRefs(elem));

    nsCOMPtr<nsISupportsCString> entry = do_QueryInterface(elem, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCString contractId;
    rv = entry->GetData(contractId);
    if (NS_FAILED(rv)) return rv;

    // Try to load spellchecker engine. Ignore errors silently
    // except for the last one (HunSpell).
    nsCOMPtr<mozISpellCheckingEngine> engine =
        do_GetService(contractId.get(), &rv);
    if (NS_SUCCEEDED(rv)) {
      aSpellCheckingEngines->AppendObject(engine);
    }
  }

  // Try to load HunSpell spellchecker engine.
  nsCOMPtr<mozISpellCheckingEngine> engine =
      do_GetService(DEFAULT_SPELL_CHECKER, &rv);
  if (NS_FAILED(rv)) {
    // Fail if not succeeded to load HunSpell. Ignore errors
    // for external spellcheck engines.
    return rv;
  }
  aSpellCheckingEngines->AppendObject(engine);

  return NS_OK;
}
