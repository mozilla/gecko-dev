/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 1 */

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"

#include "base/basictypes.h"

#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsDocShell.h"
#include "nsIContentViewer.h"
#include "nsPIDOMWindow.h"
#include "nsStyleSet.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIPrintSettings.h"
#include "nsILanguageAtomService.h"
#include "mozilla/LookAndFeel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWeakReferenceUtils.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsFrameManager.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"
#include "RestyleManager.h"
#include "SurfaceCache.h"
#include "nsCSSRuleProcessor.h"
#include "nsRuleNode.h"
#include "gfxPlatform.h"
#include "nsCSSRules.h"
#include "nsFontFaceLoader.h"
#include "mozilla/EventListenerManager.h"
#include "prenv.h"
#include "nsPluginFrame.h"
#include "nsTransitionManager.h"
#include "nsAnimationManager.h"
#include "CounterStyleManager.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "nsIMessageManager.h"
#include "mozilla/dom/MediaQueryList.h"
#include "nsSMILAnimationController.h"
#include "mozilla/css/ImageLoader.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabParent.h"
#include "nsRefreshDriver.h"
#include "Layers.h"
#include "ClientLayerManager.h"
#include "nsIDOMEvent.h"
#include "gfxPrefs.h"
#include "nsIDOMChromeWindow.h"
#include "nsFrameLoader.h"
#include "mozilla/dom/FontFaceSet.h"
#include "nsContentUtils.h"
#include "nsPIWindowRoot.h"
#include "mozilla/Preferences.h"
#include "gfxTextRun.h"
#include "nsFontFaceUtils.h"
#include "nsLayoutStylesheetCache.h"

#if defined(MOZ_WIDGET_GTK)
#include "gfxPlatformGtk.h" // xxx - for UseFcFontList
#endif


// Needed for Start/Stop of Image Animation
#include "imgIContainer.h"
#include "nsIImageLoadingContent.h"

#include "nsCSSParser.h"
#include "nsBidiUtils.h"
#include "nsServiceManagerUtils.h"

#include "URL.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

uint8_t gNotifySubDocInvalidationData;

/**
 * Layer UserData for ContainerLayers that want to be notified
 * of local invalidations of them and their descendant layers.
 * Pass a callback to ComputeDifferences to have these called.
 */
class ContainerLayerPresContext : public LayerUserData {
public:
  nsPresContext* mPresContext;
};

namespace {

class CharSetChangingRunnable : public nsRunnable
{
public:
  CharSetChangingRunnable(nsPresContext* aPresContext,
                          const nsCString& aCharSet)
    : mPresContext(aPresContext),
      mCharSet(aCharSet)
  {
  }

  NS_IMETHOD Run()
  {
    mPresContext->DoChangeCharSet(mCharSet);
    return NS_OK;
  }

private:
  nsRefPtr<nsPresContext> mPresContext;
  nsCString mCharSet;
};

} // anonymous namespace

nscolor
nsPresContext::MakeColorPref(const nsString& aColor)
{
  nsCSSParser parser;
  nsCSSValue value;
  if (!parser.ParseColorString(aColor, nullptr, 0, value)) {
    // Any better choices?
    return NS_RGB(0, 0, 0);
  }

  nscolor color;
  return nsRuleNode::ComputeColor(value, this, nullptr, color)
    ? color
    : NS_RGB(0, 0, 0);
}

bool
nsPresContext::IsDOMPaintEventPending()
{
  if (mFireAfterPaintEvents) {
    return true;
  }
  nsRootPresContext* drpc = GetDisplayRootPresContext();
  if (drpc && drpc->mRefreshDriver->ViewManagerFlushIsPending()) {
    // Since we're promising that there will be a MozAfterPaint event
    // fired, we record an empty invalidation in case display list
    // invalidation doesn't invalidate anything further.
    NotifyInvalidation(nsRect(0, 0, 0, 0), 0);
    NS_ASSERTION(mFireAfterPaintEvents, "Why aren't we planning to fire the event?");
    return true;
  }
  return false;
}

void
nsPresContext::PrefChangedCallback(const char* aPrefName, void* instance_data)
{
  nsRefPtr<nsPresContext>  presContext =
    static_cast<nsPresContext*>(instance_data);

  NS_ASSERTION(nullptr != presContext, "bad instance data");
  if (nullptr != presContext) {
    presContext->PreferenceChanged(aPrefName);
  }
}

void
nsPresContext::PrefChangedUpdateTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsPresContext*  presContext = (nsPresContext*)aClosure;
  NS_ASSERTION(presContext != nullptr, "bad instance data");
  if (presContext)
    presContext->UpdateAfterPreferencesChanged();
}

static bool
IsVisualCharset(const nsCString& aCharset)
{
  if (aCharset.LowerCaseEqualsLiteral("ibm862")             // Hebrew
      || aCharset.LowerCaseEqualsLiteral("iso-8859-8") ) {  // Hebrew
    return true; // visual text type
  }
  else {
    return false; // logical text type
  }
}

  // NOTE! nsPresContext::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsPresContext::nsPresContext(nsIDocument* aDocument, nsPresContextType aType)
  : mType(aType), mDocument(aDocument), mBaseMinFontSize(0),
    mTextZoom(1.0), mFullZoom(1.0),
    mLastFontInflationScreenSize(gfxSize(-1.0, -1.0)),
    mPageSize(-1, -1), mPPScale(1.0f),
    mViewportStyleScrollbar(NS_STYLE_OVERFLOW_AUTO, NS_STYLE_OVERFLOW_AUTO),
    mImageAnimationModePref(imgIContainer::kNormalAnimMode),
    mAllInvalidated(false),
    mPaintFlashing(false), mPaintFlashingInitialized(false)
{
  // NOTE! nsPresContext::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

  mDoScaledTwips = true;

  SetBackgroundImageDraw(true);		// always draw the background
  SetBackgroundColorDraw(true);

  mBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);

  mUseDocumentColors = true;
  mUseDocumentFonts = true;

  // the minimum font-size is unconstrained by default

  mLinkColor = NS_RGB(0x00, 0x00, 0xEE);
  mActiveLinkColor = NS_RGB(0xEE, 0x00, 0x00);
  mVisitedLinkColor = NS_RGB(0x55, 0x1A, 0x8B);
  mUnderlineLinks = true;
  mSendAfterPaintToContent = false;

  mFocusTextColor = mDefaultColor;
  mFocusBackgroundColor = mBackgroundColor;
  mFocusRingWidth = 1;

  mBodyTextColor = mDefaultColor;

  if (aType == eContext_Galley) {
    mMedium = nsGkAtoms::screen;
  } else {
    mMedium = nsGkAtoms::print;
    mPaginated = true;
  }
  mMediaEmulated = mMedium;

  if (!IsDynamic()) {
    mImageAnimationMode = imgIContainer::kDontAnimMode;
    mNeverAnimate = true;
  } else {
    mImageAnimationMode = imgIContainer::kNormalAnimMode;
    mNeverAnimate = false;
  }
  NS_ASSERTION(mDocument, "Null document");

  mCounterStylesDirty = true;

  // if text perf logging enabled, init stats struct
  PRLogModuleInfo *log = gfxPlatform::GetLog(eGfxLog_textperf);
  if (MOZ_LOG_TEST(log, LogLevel::Warning)) {
    mTextPerf = new gfxTextPerfMetrics();
  }

  if (Preferences::GetBool(GFX_MISSING_FONTS_NOTIFY_PREF)) {
    mMissingFonts = new gfxMissingFontRecorder();
  }
}

void
nsPresContext::Destroy()
{
  if (mEventManager) {
    // unclear if these are needed, but can't hurt
    mEventManager->NotifyDestroyPresContext(this);
    mEventManager->SetPresContext(nullptr);
    mEventManager = nullptr;
  }

  if (mPrefChangedTimer)
  {
    mPrefChangedTimer->Cancel();
    mPrefChangedTimer = nullptr;
  }

  // Unregister preference callbacks
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "font.",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.display.",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.underline_anchors",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.anchor_color",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.active_color",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.visited_color",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "image.animation_mode",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "bidi.",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "dom.send_after_paint_to_content",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "gfx.font_rendering.",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "layout.css.dpi",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "layout.css.devPixelsPerPx",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "nglayout.debug.paint_flashing",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "nglayout.debug.paint_flashing_chrome",
                                  this);

  // Disconnect the refresh driver *after* the transition manager, which
  // needs it.
  if (mRefreshDriver && mRefreshDriver->PresContext() == this) {
    mRefreshDriver->Disconnect();
    mRefreshDriver = nullptr;
  }
}

nsPresContext::~nsPresContext()
{
  NS_PRECONDITION(!mShell, "Presshell forgot to clear our mShell pointer");
  SetShell(nullptr);

  Destroy();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPresContext)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPresContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(nsPresContext, LastRelease())

void
nsPresContext::LastRelease()
{
  if (IsRoot()) {
    static_cast<nsRootPresContext*>(this)->CancelDidPaintTimer();
  }
  if (mMissingFonts) {
    mMissingFonts->Clear();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsPresContext)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsPresContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument);
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mDeviceContext); // not xpcom
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventManager);
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mLanguage); // an atom

  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTheme); // a service
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLangService); // a service
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrintSettings);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrefChangedTimer);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsPresContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDeviceContext); // worth bothering?
  // NS_RELEASE(tmp->mLanguage); // an atom
  // NS_IMPL_CYCLE_COLLECTION_UNLINK(mTheme); // a service
  // NS_IMPL_CYCLE_COLLECTION_UNLINK(mLangService); // a service
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrintSettings);

  tmp->Destroy();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


#define MAKE_FONT_PREF_KEY(_pref, _s0, _s1) \
 _pref.Assign(_s0); \
 _pref.Append(_s1);

static const char* const kGenericFont[] = {
  ".variable.",
  ".fixed.",
  ".serif.",
  ".sans-serif.",
  ".monospace.",
  ".cursive.",
  ".fantasy."
};

// whether no native theme service exists;
// if this gets set to true, we'll stop asking for it.
static bool sNoTheme = false;

// Set to true when LookAndFeelChanged needs to be called.  This is used
// because the look and feel is a service, so there's no need to notify it from
// more than one prescontext.
static bool sLookAndFeelChanged;

// Set to true when ThemeChanged needs to be called on mTheme.  This is used
// because mTheme is a service, so there's no need to notify it from more than
// one prescontext.
static bool sThemeChanged;

const nsPresContext::LangGroupFontPrefs*
nsPresContext::GetFontPrefsForLang(nsIAtom *aLanguage) const
{
  // Get language group for aLanguage:

  nsresult rv = NS_OK;
  nsIAtom *langGroupAtom = nullptr;
  if (!aLanguage) {
    aLanguage = mLanguage;
  }
  if (aLanguage && mLangService) {
    langGroupAtom = mLangService->GetLanguageGroup(aLanguage, &rv);
  }
  if (NS_FAILED(rv) || !langGroupAtom) {
    langGroupAtom = nsGkAtoms::x_western; // Assume x-western is safe...
  }

  // Look for cached prefs for this lang group.
  // Most documents will only use one (or very few) language groups. Rather
  // than have the overhead of a hash lookup, we simply look along what will
  // typically be a very short (usually of length 1) linked list. There are 31
  // language groups, so in the worst case scenario we'll need to traverse 31
  // link items.

  LangGroupFontPrefs *prefs =
    const_cast<LangGroupFontPrefs*>(&mLangGroupFontPrefs);
  if (prefs->mLangGroup) { // if initialized
    DebugOnly<uint32_t> count = 0;
    for (;;) {
      NS_ASSERTION(++count < 35, "Lang group count exceeded!!!");
      if (prefs->mLangGroup == langGroupAtom) {
        return prefs;
      }
      if (!prefs->mNext) {
        break;
      }
      prefs = prefs->mNext;
    }

    // nothing cached, so go on and fetch the prefs for this lang group:
    prefs = prefs->mNext = new LangGroupFontPrefs;
  }

  prefs->mLangGroup = langGroupAtom;

  /* Fetch the font prefs to be used -- see bug 61883 for details.
     Not all prefs are needed upfront. Some are fallback prefs intended
     for the GFX font sub-system...

  1) unit : assumed to be the same for all language groups -------------
  font.size.unit = px | pt    XXX could be folded in the size... bug 90440

  2) attributes for generic fonts --------------------------------------
  font.default.[langGroup] = serif | sans-serif - fallback generic font
  font.name.[generic].[langGroup] = current user' selected font on the pref dialog
  font.name-list.[generic].[langGroup] = fontname1, fontname2, ... [factory pre-built list]
  font.size.[generic].[langGroup] = integer - settable by the user
  font.size-adjust.[generic].[langGroup] = "float" - settable by the user
  font.minimum-size.[langGroup] = integer - settable by the user
  */

  nsAutoCString langGroup;
  langGroupAtom->ToUTF8String(langGroup);

  prefs->mDefaultVariableFont.size = CSSPixelsToAppUnits(16);
  prefs->mDefaultFixedFont.size = CSSPixelsToAppUnits(13);

  nsAutoCString pref;

  // get the current applicable font-size unit
  enum {eUnit_unknown = -1, eUnit_px, eUnit_pt};
  int32_t unit = eUnit_px;

  nsAdoptingCString cvalue =
    Preferences::GetCString("font.size.unit");

  if (!cvalue.IsEmpty()) {
    if (cvalue.EqualsLiteral("px")) {
      unit = eUnit_px;
    }
    else if (cvalue.EqualsLiteral("pt")) {
      unit = eUnit_pt;
    }
    else {
      // XXX should really send this warning to the user (Error Console?).
      // And just default to unit = eUnit_px?
      NS_WARNING("unexpected font-size unit -- expected: 'px' or 'pt'");
      unit = eUnit_unknown;
    }
  }

  // get font.minimum-size.[langGroup]

  MAKE_FONT_PREF_KEY(pref, "font.minimum-size.", langGroup);

  int32_t size = Preferences::GetInt(pref.get());
  if (unit == eUnit_px) {
    prefs->mMinimumFontSize = CSSPixelsToAppUnits(size);
  }
  else if (unit == eUnit_pt) {
    prefs->mMinimumFontSize = CSSPointsToAppUnits(size);
  }

  nsFont* fontTypes[] = {
    &prefs->mDefaultVariableFont,
    &prefs->mDefaultFixedFont,
    &prefs->mDefaultSerifFont,
    &prefs->mDefaultSansSerifFont,
    &prefs->mDefaultMonospaceFont,
    &prefs->mDefaultCursiveFont,
    &prefs->mDefaultFantasyFont
  };
  static_assert(MOZ_ARRAY_LENGTH(fontTypes) == eDefaultFont_COUNT,
                "FontTypes array count is not correct");

  // Get attributes specific to each generic font. We do not get the user's
  // generic-font-name-to-specific-family-name preferences because its the
  // generic name that should be fed into the cascade. It is up to the GFX
  // code to look up the font prefs to convert generic names to specific
  // family names as necessary.
  nsAutoCString generic_dot_langGroup;
  for (uint32_t eType = 0; eType < ArrayLength(fontTypes); ++eType) {
    generic_dot_langGroup.Assign(kGenericFont[eType]);
    generic_dot_langGroup.Append(langGroup);

    nsFont* font = fontTypes[eType];

    // set the default variable font (the other fonts are seen as 'generic' fonts
    // in GFX and will be queried there when hunting for alternative fonts)
    if (eType == eDefaultFont_Variable) {
      MAKE_FONT_PREF_KEY(pref, "font.name.variable.", langGroup);

      nsAdoptingString value = Preferences::GetString(pref.get());
      if (!value.IsEmpty()) {
        FontFamilyName defaultVariableName = FontFamilyName::Convert(value);
        FontFamilyType defaultType = defaultVariableName.mType;
        NS_ASSERTION(defaultType == eFamily_serif ||
                     defaultType == eFamily_sans_serif,
                     "default type must be serif or sans-serif");
        prefs->mDefaultVariableFont.fontlist = FontFamilyList(defaultType);
      }
      else {
        MAKE_FONT_PREF_KEY(pref, "font.default.", langGroup);
        value = Preferences::GetString(pref.get());
        if (!value.IsEmpty()) {
          FontFamilyName defaultVariableName = FontFamilyName::Convert(value);
          FontFamilyType defaultType = defaultVariableName.mType;
          NS_ASSERTION(defaultType == eFamily_serif ||
                       defaultType == eFamily_sans_serif,
                       "default type must be serif or sans-serif");
          prefs->mDefaultVariableFont.fontlist = FontFamilyList(defaultType);
        }
      }
    }
    else {
      if (eType == eDefaultFont_Monospace) {
        // This takes care of the confusion whereby people often expect "monospace"
        // to have the same default font-size as "-moz-fixed" (this tentative
        // size may be overwritten with the specific value for "monospace" when
        // "font.size.monospace.[langGroup]" is read -- see below)
        prefs->mDefaultMonospaceFont.size = prefs->mDefaultFixedFont.size;
      }
      else if (eType != eDefaultFont_Fixed) {
        // all the other generic fonts are initialized with the size of the
        // variable font, but their specific size can supersede later -- see below
        font->size = prefs->mDefaultVariableFont.size;
      }
    }

    // Bug 84398: for spec purists, a different font-size only applies to the
    // .variable. and .fixed. fonts and the other fonts should get |font-size-adjust|.
    // The problem is that only GfxWin has the support for |font-size-adjust|. So for
    // parity, we enable the ability to set a different font-size on all platforms.

    // get font.size.[generic].[langGroup]
    // size=0 means 'Auto', i.e., generic fonts retain the size of the variable font
    MAKE_FONT_PREF_KEY(pref, "font.size", generic_dot_langGroup);
    size = Preferences::GetInt(pref.get());
    if (size > 0) {
      if (unit == eUnit_px) {
        font->size = CSSPixelsToAppUnits(size);
      }
      else if (unit == eUnit_pt) {
        font->size = CSSPointsToAppUnits(size);
      }
    }

    // get font.size-adjust.[generic].[langGroup]
    // XXX only applicable on GFX ports that handle |font-size-adjust|
    MAKE_FONT_PREF_KEY(pref, "font.size-adjust", generic_dot_langGroup);
    cvalue = Preferences::GetCString(pref.get());
    if (!cvalue.IsEmpty()) {
      font->sizeAdjust = (float)atof(cvalue.get());
    }

#ifdef DEBUG_rbs
    printf("%s Family-list:%s size:%d sizeAdjust:%.2f\n",
           generic_dot_langGroup.get(),
           NS_ConvertUTF16toUTF8(font->name).get(), font->size,
           font->sizeAdjust);
#endif
  }

  return prefs;
}

void
nsPresContext::GetDocumentColorPreferences()
{
  // Make sure the preferences are initialized.  In the normal run,
  // they would already be, because gfxPlatform would have been created,
  // but in some reference tests, that is not the case.
  gfxPrefs::GetSingleton();

  int32_t useAccessibilityTheme = 0;
  bool usePrefColors = true;
  bool isChromeDocShell = false;
  static int32_t sDocumentColorsSetting;
  static bool sDocumentColorsSettingPrefCached = false;
  if (!sDocumentColorsSettingPrefCached) {
    sDocumentColorsSettingPrefCached = true;
    Preferences::AddIntVarCache(&sDocumentColorsSetting,
                                "browser.display.document_color_use",
                                0);
  }

  nsIDocument* doc = mDocument->GetDisplayDocument();
  if (doc && doc->GetDocShell()) {
    isChromeDocShell = nsIDocShellTreeItem::typeChrome ==
                       doc->GetDocShell()->ItemType();
  } else {
    nsCOMPtr<nsIDocShellTreeItem> docShell(mContainer);
    if (docShell) {
      isChromeDocShell = nsIDocShellTreeItem::typeChrome == docShell->ItemType();
    }
  }

  mIsChromeOriginImage = mDocument->IsBeingUsedAsImage() &&
                         IsChromeURI(mDocument->GetDocumentURI());

  if (isChromeDocShell || mIsChromeOriginImage) {
    usePrefColors = false;
  } else {
    useAccessibilityTheme =
      LookAndFeel::GetInt(LookAndFeel::eIntID_UseAccessibilityTheme, 0);
    usePrefColors = !useAccessibilityTheme;
  }
  if (usePrefColors) {
    usePrefColors =
      !Preferences::GetBool("browser.display.use_system_colors", false);
  }

  if (usePrefColors) {
    nsAdoptingString colorStr =
      Preferences::GetString("browser.display.foreground_color");

    if (!colorStr.IsEmpty()) {
      mDefaultColor = MakeColorPref(colorStr);
    }

    colorStr = Preferences::GetString("browser.display.background_color");

    if (!colorStr.IsEmpty()) {
      mBackgroundColor = MakeColorPref(colorStr);
    }
  }
  else {
    mDefaultColor =
      LookAndFeel::GetColor(LookAndFeel::eColorID_WindowForeground,
                            NS_RGB(0x00, 0x00, 0x00));
    mBackgroundColor =
      LookAndFeel::GetColor(LookAndFeel::eColorID_WindowBackground,
                            NS_RGB(0xFF, 0xFF, 0xFF));
  }

  // Wherever we got the default background color from, ensure it is
  // opaque.
  mBackgroundColor = NS_ComposeColors(NS_RGB(0xFF, 0xFF, 0xFF),
                                      mBackgroundColor);


  // Now deal with the pref:
  // 0 = default: always, except in high contrast mode
  // 1 = always
  // 2 = never
  if (sDocumentColorsSetting == 1) {
    mUseDocumentColors = true;
  } else if (sDocumentColorsSetting == 2) {
    mUseDocumentColors = isChromeDocShell || mIsChromeOriginImage;
  } else {
    MOZ_ASSERT(!useAccessibilityTheme ||
               !(isChromeDocShell || mIsChromeOriginImage),
               "The accessibility theme should only be on for non-chrome");
    mUseDocumentColors = !useAccessibilityTheme;
  }
}

void
nsPresContext::GetUserPreferences()
{
  if (!GetPresShell()) {
    // No presshell means nothing to do here.  We'll do this when we
    // get a presshell.
    return;
  }

  mAutoQualityMinFontSizePixelsPref =
    Preferences::GetInt("browser.display.auto_quality_min_font_size");

  // * document colors
  GetDocumentColorPreferences();

  mSendAfterPaintToContent =
    Preferences::GetBool("dom.send_after_paint_to_content",
                         mSendAfterPaintToContent);

  // * link colors
  mUnderlineLinks =
    Preferences::GetBool("browser.underline_anchors", mUnderlineLinks);

  nsAdoptingString colorStr = Preferences::GetString("browser.anchor_color");

  if (!colorStr.IsEmpty()) {
    mLinkColor = MakeColorPref(colorStr);
  }

  colorStr = Preferences::GetString("browser.active_color");

  if (!colorStr.IsEmpty()) {
    mActiveLinkColor = MakeColorPref(colorStr);
  }

  colorStr = Preferences::GetString("browser.visited_color");

  if (!colorStr.IsEmpty()) {
    mVisitedLinkColor = MakeColorPref(colorStr);
  }

  mUseFocusColors =
    Preferences::GetBool("browser.display.use_focus_colors", mUseFocusColors);

  mFocusTextColor = mDefaultColor;
  mFocusBackgroundColor = mBackgroundColor;

  colorStr = Preferences::GetString("browser.display.focus_text_color");

  if (!colorStr.IsEmpty()) {
    mFocusTextColor = MakeColorPref(colorStr);
  }

  colorStr = Preferences::GetString("browser.display.focus_background_color");

  if (!colorStr.IsEmpty()) {
    mFocusBackgroundColor = MakeColorPref(colorStr);
  }

  mFocusRingWidth =
    Preferences::GetInt("browser.display.focus_ring_width", mFocusRingWidth);

  mFocusRingOnAnything =
    Preferences::GetBool("browser.display.focus_ring_on_anything",
                         mFocusRingOnAnything);

  mFocusRingStyle =
    Preferences::GetInt("browser.display.focus_ring_style", mFocusRingStyle);

  mBodyTextColor = mDefaultColor;

  // * use fonts?
  mUseDocumentFonts =
    Preferences::GetInt("browser.display.use_document_fonts") != 0;

  mPrefScrollbarSide = Preferences::GetInt("layout.scrollbar.side");

  ResetCachedFontPrefs();

  // * image animation
  const nsAdoptingCString& animatePref =
    Preferences::GetCString("image.animation_mode");
  if (animatePref.EqualsLiteral("normal"))
    mImageAnimationModePref = imgIContainer::kNormalAnimMode;
  else if (animatePref.EqualsLiteral("none"))
    mImageAnimationModePref = imgIContainer::kDontAnimMode;
  else if (animatePref.EqualsLiteral("once"))
    mImageAnimationModePref = imgIContainer::kLoopOnceAnimMode;
  else // dynamic change to invalid value should act like it does initially
    mImageAnimationModePref = imgIContainer::kNormalAnimMode;

  uint32_t bidiOptions = GetBidi();

  int32_t prefInt =
    Preferences::GetInt(IBMBIDI_TEXTDIRECTION_STR,
                        GET_BIDI_OPTION_DIRECTION(bidiOptions));
  SET_BIDI_OPTION_DIRECTION(bidiOptions, prefInt);
  mPrefBidiDirection = prefInt;

  prefInt =
    Preferences::GetInt(IBMBIDI_TEXTTYPE_STR,
                        GET_BIDI_OPTION_TEXTTYPE(bidiOptions));
  SET_BIDI_OPTION_TEXTTYPE(bidiOptions, prefInt);

  prefInt =
    Preferences::GetInt(IBMBIDI_NUMERAL_STR,
                        GET_BIDI_OPTION_NUMERAL(bidiOptions));
  SET_BIDI_OPTION_NUMERAL(bidiOptions, prefInt);

  prefInt =
    Preferences::GetInt(IBMBIDI_SUPPORTMODE_STR,
                        GET_BIDI_OPTION_SUPPORT(bidiOptions));
  SET_BIDI_OPTION_SUPPORT(bidiOptions, prefInt);

  // We don't need to force reflow: either we are initializing a new
  // prescontext or we are being called from UpdateAfterPreferencesChanged()
  // which triggers a reflow anyway.
  SetBidi(bidiOptions, false);
}

void
nsPresContext::InvalidatePaintedLayers()
{
  if (!mShell)
    return;
  nsIFrame* rootFrame = mShell->FrameManager()->GetRootFrame();
  if (rootFrame) {
    // FrameLayerBuilder caches invalidation-related values that depend on the
    // appunits-per-dev-pixel ratio, so ensure that all PaintedLayer drawing
    // is completely flushed.
    rootFrame->InvalidateFrameSubtree();
  }
}

void
nsPresContext::AppUnitsPerDevPixelChanged()
{
  InvalidatePaintedLayers();

  if (mDeviceContext) {
    mDeviceContext->FlushFontCache();
  }

  if (HasCachedStyleData()) {
    // All cached style data must be recomputed.
    MediaFeatureValuesChanged(eRestyle_ForceDescendants, NS_STYLE_HINT_REFLOW);
  }

  mCurAppUnitsPerDevPixel = AppUnitsPerDevPixel();
}

void
nsPresContext::PreferenceChanged(const char* aPrefName)
{
  nsDependentCString prefName(aPrefName);
  if (prefName.EqualsLiteral("layout.css.dpi") ||
      prefName.EqualsLiteral("layout.css.devPixelsPerPx")) {
    int32_t oldAppUnitsPerDevPixel = AppUnitsPerDevPixel();
    if (mDeviceContext->CheckDPIChange() && mShell) {
      nsCOMPtr<nsIPresShell> shell = mShell;
      // Re-fetch the view manager's window dimensions in case there's a deferred
      // resize which hasn't affected our mVisibleArea yet
      nscoord oldWidthAppUnits, oldHeightAppUnits;
      nsRefPtr<nsViewManager> vm = shell->GetViewManager();
      if (!vm) {
        return;
      }
      vm->GetWindowDimensions(&oldWidthAppUnits, &oldHeightAppUnits);
      float oldWidthDevPixels = oldWidthAppUnits/oldAppUnitsPerDevPixel;
      float oldHeightDevPixels = oldHeightAppUnits/oldAppUnitsPerDevPixel;

      nscoord width = NSToCoordRound(oldWidthDevPixels*AppUnitsPerDevPixel());
      nscoord height = NSToCoordRound(oldHeightDevPixels*AppUnitsPerDevPixel());
      vm->SetWindowDimensions(width, height);

      AppUnitsPerDevPixelChanged();
    }
    return;
  }
  if (prefName.EqualsLiteral(GFX_MISSING_FONTS_NOTIFY_PREF)) {
    if (Preferences::GetBool(GFX_MISSING_FONTS_NOTIFY_PREF)) {
      if (!mMissingFonts) {
        mMissingFonts = new gfxMissingFontRecorder();
        // trigger reflow to detect missing fonts on the current page
        mPrefChangePendingNeedsReflow = true;
      }
    } else {
      if (mMissingFonts) {
        mMissingFonts->Clear();
      }
      mMissingFonts = nullptr;
    }
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("font."))) {
    // Changes to font family preferences don't change anything in the
    // computed style data, so the style system won't generate a reflow
    // hint for us.  We need to do that manually.

    // FIXME We could probably also handle changes to
    // browser.display.auto_quality_min_font_size here, but that
    // probably also requires clearing the text run cache, so don't
    // bother (yet, anyway).
    mPrefChangePendingNeedsReflow = true;
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("bidi."))) {
    // Changes to bidi prefs need to trigger a reflow (see bug 443629)
    mPrefChangePendingNeedsReflow = true;

    // Changes to bidi.numeral also needs to empty the text run cache.
    // This is handled in gfxTextRunWordCache.cpp.
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("gfx.font_rendering."))) {
    // Changes to font_rendering prefs need to trigger a reflow
    mPrefChangePendingNeedsReflow = true;
  }
  // we use a zero-delay timer to coalesce multiple pref updates
  if (!mPrefChangedTimer)
  {
    mPrefChangedTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mPrefChangedTimer)
      return;
    // We will end up calling InvalidatePreferenceSheets one from each pres
    // context, but all it's doing is clearing its cached sheet pointers,
    // so it won't be wastefully recreating the sheet multiple times.
    // The first pres context that has its mPrefChangedTimer called will
    // be the one to cause the reconstruction of the pref style sheet.
    nsLayoutStylesheetCache::InvalidatePreferenceSheets();
    mPrefChangedTimer->InitWithFuncCallback(nsPresContext::PrefChangedUpdateTimerCallback, (void*)this, 0, nsITimer::TYPE_ONE_SHOT);
  }
  if (prefName.EqualsLiteral("nglayout.debug.paint_flashing") ||
      prefName.EqualsLiteral("nglayout.debug.paint_flashing_chrome")) {
    mPaintFlashingInitialized = false;
    return;
  }
}

void
nsPresContext::UpdateAfterPreferencesChanged()
{
  mPrefChangedTimer = nullptr;

  nsCOMPtr<nsIDocShellTreeItem> docShell(mContainer);
  if (docShell && nsIDocShellTreeItem::typeChrome == docShell->ItemType()) {
    return;
  }

  // Initialize our state from the user preferences
  GetUserPreferences();

  // update the presShell: tell it to set the preference style rules up
  if (mShell) {
    mShell->UpdatePreferenceStyles();
  }

  InvalidatePaintedLayers();
  mDeviceContext->FlushFontCache();

  nsChangeHint hint = nsChangeHint(0);

  if (mPrefChangePendingNeedsReflow) {
    NS_UpdateHint(hint, NS_STYLE_HINT_REFLOW);
  }

  // Preferences require rerunning selector matching because we rebuild
  // the pref style sheet for some preference changes.
  RebuildAllStyleData(hint, eRestyle_Subtree);
}

nsresult
nsPresContext::Init(nsDeviceContext* aDeviceContext)
{
  NS_ASSERTION(!mInitialized, "attempt to reinit pres context");
  NS_ENSURE_ARG(aDeviceContext);

  mDeviceContext = aDeviceContext;

  if (mDeviceContext->SetFullZoom(mFullZoom))
    mDeviceContext->FlushFontCache();
  mCurAppUnitsPerDevPixel = AppUnitsPerDevPixel();

  mEventManager = new mozilla::EventStateManager();

  mTransitionManager = new nsTransitionManager(this);

  mAnimationManager = new nsAnimationManager(this);

  if (mDocument->GetDisplayDocument()) {
    NS_ASSERTION(mDocument->GetDisplayDocument()->GetShell() &&
                 mDocument->GetDisplayDocument()->GetShell()->GetPresContext(),
                 "Why are we being initialized?");
    mRefreshDriver = mDocument->GetDisplayDocument()->GetShell()->
      GetPresContext()->RefreshDriver();
  } else {
    nsIDocument* parent = mDocument->GetParentDocument();
    // Unfortunately, sometimes |parent| here has no presshell because
    // printing screws up things.  Assert that in other cases it does,
    // but whenever the shell is null just fall back on using our own
    // refresh driver.
    NS_ASSERTION(!parent || mDocument->IsStaticDocument() || parent->GetShell(),
                 "How did we end up with a presshell if our parent doesn't "
                 "have one?");
    if (parent && parent->GetShell()) {
      NS_ASSERTION(parent->GetShell()->GetPresContext(),
                   "How did we get a presshell?");

      // We don't have our container set yet at this point
      nsCOMPtr<nsIDocShellTreeItem> ourItem = mDocument->GetDocShell();
      if (ourItem) {
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        ourItem->GetSameTypeParent(getter_AddRefs(parentItem));
        if (parentItem) {
          Element* containingElement =
            parent->FindContentForSubDocument(mDocument);
          if (!containingElement->IsXULElement() ||
              !containingElement->
                HasAttr(kNameSpaceID_None,
                        nsGkAtoms::forceOwnRefreshDriver)) {
            mRefreshDriver = parent->GetShell()->GetPresContext()->RefreshDriver();
          }
        }
      }
    }

    if (!mRefreshDriver) {
      mRefreshDriver = new nsRefreshDriver(this);
    }
  }

  // Initialise refresh tick counters for OMTA
  mLastStyleUpdateForAllAnimations = mRefreshDriver->MostRecentRefresh();

  // Initialize restyle manager after initializing the refresh driver.
  // Since RestyleManager is also the name of a method of nsPresContext,
  // it is necessary to prefix the class with the mozilla namespace
  // here.
  mRestyleManager = new mozilla::RestyleManager(this);

  mLangService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);

  // Register callbacks so we're notified when the preferences change
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "font.",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.display.",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.underline_anchors",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.anchor_color",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.active_color",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.visited_color",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "image.animation_mode",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "bidi.",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "dom.send_after_paint_to_content",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "gfx.font_rendering.",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "layout.css.dpi",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "layout.css.devPixelsPerPx",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "nglayout.debug.paint_flashing",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "nglayout.debug.paint_flashing_chrome",
                                this);

  nsresult rv = mEventManager->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mEventManager->SetPresContext(this);

#ifdef RESTYLE_LOGGING
  mRestyleLoggingEnabled = RestyleManager::RestyleLoggingInitiallyEnabled();
#endif

#ifdef DEBUG
  mInitialized = true;
#endif

  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_THIN] = CSSPixelsToAppUnits(1);
  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_MEDIUM] = CSSPixelsToAppUnits(3);
  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_THICK] = CSSPixelsToAppUnits(5);

  return NS_OK;
}

// Note: We don't hold a reference on the shell; it has a reference to
// us
void
nsPresContext::SetShell(nsIPresShell* aShell)
{
  if (mCounterStyleManager) {
    mCounterStyleManager->Disconnect();
    mCounterStyleManager = nullptr;
  }

  if (mShell) {
    // Remove ourselves as the charset observer from the shell's doc, because
    // this shell may be going away for good.
    nsIDocument *doc = mShell->GetDocument();
    if (doc) {
      doc->RemoveCharSetObserver(this);
    }
  }

  mShell = aShell;

  if (mShell) {
    // Since CounterStyleManager is also the name of a method of
    // nsPresContext, it is necessary to prefix the class with the mozilla
    // namespace here.
    mCounterStyleManager = new mozilla::CounterStyleManager(this);

    nsIDocument *doc = mShell->GetDocument();
    NS_ASSERTION(doc, "expect document here");
    if (doc) {
      // Have to update PresContext's mDocument before calling any other methods.
      mDocument = doc;
    }
    // Initialize our state from the user preferences, now that we
    // have a presshell, and hence a document.
    GetUserPreferences();

    if (doc) {
      nsIURI *docURI = doc->GetDocumentURI();

      if (IsDynamic() && docURI) {
        bool isChrome = false;
        bool isRes = false;
        docURI->SchemeIs("chrome", &isChrome);
        docURI->SchemeIs("resource", &isRes);

        if (!isChrome && !isRes)
          mImageAnimationMode = mImageAnimationModePref;
        else
          mImageAnimationMode = imgIContainer::kNormalAnimMode;
      }

      if (mLangService) {
        doc->AddCharSetObserver(this);
        UpdateCharSet(doc->GetDocumentCharacterSet());
      }
    }
  } else {
    if (mTransitionManager) {
      mTransitionManager->Disconnect();
      mTransitionManager = nullptr;
    }
    if (mAnimationManager) {
      mAnimationManager->Disconnect();
      mAnimationManager = nullptr;
    }
    if (mRestyleManager) {
      mRestyleManager->Disconnect();
      mRestyleManager = nullptr;
    }

    if (IsRoot()) {
      // Have to cancel our plugin geometry timer, because the
      // callback for that depends on a non-null presshell.
      static_cast<nsRootPresContext*>(this)->CancelApplyPluginGeometryTimer();
    }
  }
}

void
nsPresContext::DoChangeCharSet(const nsCString& aCharSet)
{
  UpdateCharSet(aCharSet);
  mDeviceContext->FlushFontCache();
  RebuildAllStyleData(NS_STYLE_HINT_REFLOW, nsRestyleHint(0));
}

void
nsPresContext::UpdateCharSet(const nsCString& aCharSet)
{
  if (mLangService) {
    mLanguage = mLangService->LookupCharSet(aCharSet);
    // this will be a language group (or script) code rather than a true language code

    // bug 39570: moved from nsLanguageAtomService::LookupCharSet()
    if (mLanguage == nsGkAtoms::Unicode) {
      mLanguage = mLangService->GetLocaleLanguage();
    }
    ResetCachedFontPrefs();
  }

  switch (GET_BIDI_OPTION_TEXTTYPE(GetBidi())) {

    case IBMBIDI_TEXTTYPE_LOGICAL:
      SetVisualMode(false);
      break;

    case IBMBIDI_TEXTTYPE_VISUAL:
      SetVisualMode(true);
      break;

    case IBMBIDI_TEXTTYPE_CHARSET:
    default:
      SetVisualMode(IsVisualCharset(aCharSet));
  }
}

NS_IMETHODIMP
nsPresContext::Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const char16_t* aData)
{
  if (!nsCRT::strcmp(aTopic, "charset")) {
    nsRefPtr<CharSetChangingRunnable> runnable =
      new CharSetChangingRunnable(this, NS_LossyConvertUTF16toASCII(aData));
    return NS_DispatchToCurrentThread(runnable);
  }

  NS_WARNING("unrecognized topic in nsPresContext::Observe");
  return NS_ERROR_FAILURE;
}

nsPresContext*
nsPresContext::GetParentPresContext()
{
  nsIPresShell* shell = GetPresShell();
  if (shell) {
    nsViewManager* viewManager = shell->GetViewManager();
    if (viewManager) {
      nsView* view = viewManager->GetRootView();
      if (view) {
        view = view->GetParent(); // anonymous inner view
        if (view) {
          view = view->GetParent(); // subdocumentframe's view
          if (view) {
            nsIFrame* f = view->GetFrame();
            if (f) {
              return f->PresContext();
            }
          }
        }
      }
    }
  }
  return nullptr;
}

nsPresContext*
nsPresContext::GetToplevelContentDocumentPresContext()
{
  if (IsChrome())
    return nullptr;
  nsPresContext* pc = this;
  for (;;) {
    nsPresContext* parent = pc->GetParentPresContext();
    if (!parent || parent->IsChrome())
      return pc;
    pc = parent;
  }
}

nsIWidget*
nsPresContext::GetNearestWidget(nsPoint* aOffset)
{
  NS_ENSURE_TRUE(mShell, nullptr);
  nsIFrame* frame = mShell->GetRootFrame();
  NS_ENSURE_TRUE(frame, nullptr);
  return frame->GetView()->GetNearestWidget(aOffset);
}

nsIWidget*
nsPresContext::GetRootWidget()
{
  NS_ENSURE_TRUE(mShell, nullptr);
  nsViewManager* vm = mShell->GetViewManager();
  if (!vm) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget;
  vm->GetRootWidget(getter_AddRefs(widget));
  return widget.get();
}

// We may want to replace this with something faster, maybe caching the root prescontext
nsRootPresContext*
nsPresContext::GetRootPresContext()
{
  nsPresContext* pc = this;
  for (;;) {
    nsPresContext* parent = pc->GetParentPresContext();
    if (!parent)
      break;
    pc = parent;
  }
  return pc->IsRoot() ? static_cast<nsRootPresContext*>(pc) : nullptr;
}

nsRootPresContext*
nsPresContext::GetDisplayRootPresContext()
{
  nsPresContext* pc = this;
  for (;;) {
    nsPresContext* parent = pc->GetParentPresContext();
    if (!parent) {
      // Not sure if this is always strictly the parent, but it works for GetRootPresContext
      // where the current pres context has no frames.
      nsIDocument *doc = pc->Document();
      if (doc) {
        doc = doc->GetParentDocument();
        if (doc) {
          nsIPresShell* shell = doc->GetShell();
          if (shell) {
            parent = shell->GetPresContext();
          }
        }
      }
    }
    if (!parent || parent == pc)
      break;
    pc = parent;
  }
  return pc->IsRoot() ? static_cast<nsRootPresContext*>(pc) : nullptr;
}

void
nsPresContext::CompatibilityModeChanged()
{
  if (!mShell) {
    return;
  }

  nsIDocument* doc = mShell->GetDocument();
  if (!doc) {
    return;
  }

  if (doc->IsSVGDocument()) {
    // SVG documents never load quirk.css.
    return;
  }

  bool needsQuirkSheet = CompatibilityMode() == eCompatibility_NavQuirks;
  if (mQuirkSheetAdded == needsQuirkSheet) {
    return;
  }

  nsStyleSet* styleSet = mShell->StyleSet();
  CSSStyleSheet* sheet = nsLayoutStylesheetCache::QuirkSheet();

  if (needsQuirkSheet) {
    // quirk.css needs to come after html.css; we just keep it at the end.
    DebugOnly<nsresult> rv =
      styleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, sheet);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "failed to insert quirk.css");
  } else {
    DebugOnly<nsresult> rv =
      styleSet->RemoveStyleSheet(nsStyleSet::eAgentSheet, sheet);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "failed to remove quirk.css");
  }

  mQuirkSheetAdded = needsQuirkSheet;
}

// Helper function for setting Anim Mode on image
static void SetImgAnimModeOnImgReq(imgIRequest* aImgReq, uint16_t aMode)
{
  if (aImgReq) {
    nsCOMPtr<imgIContainer> imgCon;
    aImgReq->GetImage(getter_AddRefs(imgCon));
    if (imgCon) {
      imgCon->SetAnimationMode(aMode);
    }
  }
}

// IMPORTANT: Assumption is that all images for a Presentation
// have the same Animation Mode (pavlov said this was OK)
//
// Walks content and set the animation mode
// this is a way to turn on/off image animations
void nsPresContext::SetImgAnimations(nsIContent *aParent, uint16_t aMode)
{
  nsCOMPtr<nsIImageLoadingContent> imgContent(do_QueryInterface(aParent));
  if (imgContent) {
    nsCOMPtr<imgIRequest> imgReq;
    imgContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                           getter_AddRefs(imgReq));
    SetImgAnimModeOnImgReq(imgReq, aMode);
  }

  uint32_t count = aParent->GetChildCount();
  for (uint32_t i = 0; i < count; ++i) {
    SetImgAnimations(aParent->GetChildAt(i), aMode);
  }
}

void
nsPresContext::SetSMILAnimations(nsIDocument *aDoc, uint16_t aNewMode,
                                 uint16_t aOldMode)
{
  if (aDoc->HasAnimationController()) {
    nsSMILAnimationController* controller = aDoc->GetAnimationController();
    switch (aNewMode)
    {
      case imgIContainer::kNormalAnimMode:
      case imgIContainer::kLoopOnceAnimMode:
        if (aOldMode == imgIContainer::kDontAnimMode)
          controller->Resume(nsSMILTimeContainer::PAUSE_USERPREF);
        break;

      case imgIContainer::kDontAnimMode:
        if (aOldMode != imgIContainer::kDontAnimMode)
          controller->Pause(nsSMILTimeContainer::PAUSE_USERPREF);
        break;
    }
  }
}

void
nsPresContext::SetImageAnimationModeInternal(uint16_t aMode)
{
  NS_ASSERTION(aMode == imgIContainer::kNormalAnimMode ||
               aMode == imgIContainer::kDontAnimMode ||
               aMode == imgIContainer::kLoopOnceAnimMode, "Wrong Animation Mode is being set!");

  // Image animation mode cannot be changed when rendering to a printer.
  if (!IsDynamic())
    return;

  // Now walk the content tree and set the animation mode
  // on all the images.
  if (mShell != nullptr) {
    nsIDocument *doc = mShell->GetDocument();
    if (doc) {
      doc->StyleImageLoader()->SetAnimationMode(aMode);

      Element *rootElement = doc->GetRootElement();
      if (rootElement) {
        SetImgAnimations(rootElement, aMode);
      }
      SetSMILAnimations(doc, aMode, mImageAnimationMode);
    }
  }

  mImageAnimationMode = aMode;
}

void
nsPresContext::SetImageAnimationModeExternal(uint16_t aMode)
{
  SetImageAnimationModeInternal(aMode);
}

const nsFont*
nsPresContext::GetDefaultFont(uint8_t aFontID, nsIAtom *aLanguage) const
{
  const LangGroupFontPrefs *prefs = GetFontPrefsForLang(aLanguage);

  const nsFont *font;
  switch (aFontID) {
    // Special (our default variable width font and fixed width font)
    case kPresContext_DefaultVariableFont_ID:
      font = &prefs->mDefaultVariableFont;
      break;
    case kPresContext_DefaultFixedFont_ID:
      font = &prefs->mDefaultFixedFont;
      break;
    // CSS
    case kGenericFont_serif:
      font = &prefs->mDefaultSerifFont;
      break;
    case kGenericFont_sans_serif:
      font = &prefs->mDefaultSansSerifFont;
      break;
    case kGenericFont_monospace:
      font = &prefs->mDefaultMonospaceFont;
      break;
    case kGenericFont_cursive:
      font = &prefs->mDefaultCursiveFont;
      break;
    case kGenericFont_fantasy:
      font = &prefs->mDefaultFantasyFont;
      break;
    default:
      font = nullptr;
      NS_ERROR("invalid arg");
      break;
  }
  return font;
}

void
nsPresContext::SetFullZoom(float aZoom)
{
  if (!mShell || mFullZoom == aZoom) {
    return;
  }

  // Re-fetch the view manager's window dimensions in case there's a deferred
  // resize which hasn't affected our mVisibleArea yet
  nscoord oldWidthAppUnits, oldHeightAppUnits;
  mShell->GetViewManager()->GetWindowDimensions(&oldWidthAppUnits, &oldHeightAppUnits);
  float oldWidthDevPixels = oldWidthAppUnits / float(mCurAppUnitsPerDevPixel);
  float oldHeightDevPixels = oldHeightAppUnits / float(mCurAppUnitsPerDevPixel);
  mDeviceContext->SetFullZoom(aZoom);

  NS_ASSERTION(!mSuppressResizeReflow, "two zooms happening at the same time? impossible!");
  mSuppressResizeReflow = true;

  mFullZoom = aZoom;
  mShell->GetViewManager()->
    SetWindowDimensions(NSToCoordRound(oldWidthDevPixels * AppUnitsPerDevPixel()),
                        NSToCoordRound(oldHeightDevPixels * AppUnitsPerDevPixel()));

  AppUnitsPerDevPixelChanged();

  mSuppressResizeReflow = false;
}

gfxSize
nsPresContext::ScreenSizeInchesForFontInflation(bool* aChanged)
{
  if (aChanged) {
    *aChanged = false;
  }

  nsDeviceContext *dx = DeviceContext();
  nsRect clientRect;
  dx->GetClientRect(clientRect); // FIXME: GetClientRect looks expensive
  float unitsPerInch = dx->AppUnitsPerPhysicalInch();
  gfxSize deviceSizeInches(float(clientRect.width) / unitsPerInch,
                           float(clientRect.height) / unitsPerInch);

  if (mLastFontInflationScreenSize == gfxSize(-1.0, -1.0)) {
    mLastFontInflationScreenSize = deviceSizeInches;
  }

  if (deviceSizeInches != mLastFontInflationScreenSize && aChanged) {
    *aChanged = true;
    mLastFontInflationScreenSize = deviceSizeInches;
  }

  return deviceSizeInches;
}

void
nsPresContext::SetContainer(nsIDocShell* aDocShell)
{
  if (aDocShell) {
    mContainer = static_cast<nsDocShell*>(aDocShell);
  } else {
    mContainer = WeakPtr<nsDocShell>();
  }
  UpdateIsChrome();
  if (mContainer) {
    GetDocumentColorPreferences();
  }
}

nsISupports*
nsPresContext::GetContainerWeakInternal() const
{
  return static_cast<nsIDocShell*>(mContainer);
}

nsISupports*
nsPresContext::GetContainerWeakExternal() const
{
  return GetContainerWeakInternal();
}

nsIDocShell*
nsPresContext::GetDocShell() const
{
  return mContainer;
}

/* virtual */ void
nsPresContext::Detach()
{
  SetContainer(nullptr);
  SetLinkHandler(nullptr);
  if (mShell) {
    mShell->CancelInvalidatePresShellIfHidden();
  }
}

bool
nsPresContext::StyleUpdateForAllAnimationsIsUpToDate() const
{
  return mLastStyleUpdateForAllAnimations == mRefreshDriver->MostRecentRefresh();
}

void
nsPresContext::TickLastStyleUpdateForAllAnimations()
{
  mLastStyleUpdateForAllAnimations = mRefreshDriver->MostRecentRefresh();
}

void
nsPresContext::ClearLastStyleUpdateForAllAnimations()
{
  mLastStyleUpdateForAllAnimations = TimeStamp();
}

bool
nsPresContext::BidiEnabledExternal() const
{
  return BidiEnabledInternal();
}

bool
nsPresContext::BidiEnabledInternal() const
{
  return Document()->GetBidiEnabled();
}

void
nsPresContext::SetBidiEnabled() const
{
  if (mShell) {
    nsIDocument *doc = mShell->GetDocument();
    if (doc) {
      doc->SetBidiEnabled();
    }
  }
}

void
nsPresContext::SetBidi(uint32_t aSource, bool aForceRestyle)
{
  // Don't do all this stuff unless the options have changed.
  if (aSource == GetBidi()) {
    return;
  }

  NS_ASSERTION(!(aForceRestyle && (GetBidi() == 0)),
               "ForceReflow on new prescontext");

  Document()->SetBidiOptions(aSource);
  if (IBMBIDI_TEXTDIRECTION_RTL == GET_BIDI_OPTION_DIRECTION(aSource)
      || IBMBIDI_NUMERAL_HINDI == GET_BIDI_OPTION_NUMERAL(aSource)) {
    SetBidiEnabled();
  }
  if (IBMBIDI_TEXTTYPE_VISUAL == GET_BIDI_OPTION_TEXTTYPE(aSource)) {
    SetVisualMode(true);
  }
  else if (IBMBIDI_TEXTTYPE_LOGICAL == GET_BIDI_OPTION_TEXTTYPE(aSource)) {
    SetVisualMode(false);
  }
  else {
    nsIDocument* doc = mShell->GetDocument();
    if (doc) {
      SetVisualMode(IsVisualCharset(doc->GetDocumentCharacterSet()));
    }
  }
  if (aForceRestyle && mShell) {
    // Reconstruct the root document element's frame and its children,
    // because we need to trigger frame reconstruction for direction change.
    mDocument->RebuildUserFontSet();
    mShell->ReconstructFrames();
  }
}

uint32_t
nsPresContext::GetBidi() const
{
  return Document()->GetBidiOptions();
}

bool
nsPresContext::IsTopLevelWindowInactive()
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem(mContainer);
  if (!treeItem)
    return false;

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  if (!rootItem) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindow> domWindow = rootItem->GetWindow();

  return domWindow && !domWindow->IsActive();
}

nsITheme*
nsPresContext::GetTheme()
{
  if (!sNoTheme && !mTheme) {
    mTheme = do_GetService("@mozilla.org/chrome/chrome-native-theme;1");
    if (!mTheme)
      sNoTheme = true;
  }

  return mTheme;
}

void
nsPresContext::ThemeChanged()
{
  if (!mPendingThemeChanged) {
    sLookAndFeelChanged = true;
    sThemeChanged = true;

    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::ThemeChangedInternal);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingThemeChanged = true;
    }
  }
}

static void
NotifyThemeChanged(TabParent* aTabParent, void* aArg)
{
  aTabParent->ThemeChanged();
}

void
nsPresContext::ThemeChangedInternal()
{
  mPendingThemeChanged = false;

  // Tell the theme that it changed, so it can flush any handles to stale theme
  // data.
  if (mTheme && sThemeChanged) {
    mTheme->ThemeChanged();
    sThemeChanged = false;
  }

  if (sLookAndFeelChanged) {
    // Clear all cached LookAndFeel colors.
    LookAndFeel::Refresh();
    sLookAndFeelChanged = false;

    // Vector images (SVG) may be using theme colors so we discard all cached
    // surfaces. (We could add a vector image only version of DiscardAll, but
    // in bug 940625 we decided theme changes are rare enough not to bother.)
    mozilla::image::SurfaceCache::DiscardAll();
  }

  // This will force the system metrics to be generated the next time they're used
  nsCSSRuleProcessor::FreeSystemMetrics();

  // Changes to system metrics can change media queries on them, or
  // :-moz-system-metric selectors (which requires eRestyle_Subtree).
  // Changes in theme can change system colors (whose changes are
  // properly reflected in computed style data), system fonts (whose
  // changes are not), and -moz-appearance (whose changes likewise are
  // not), so we need to reflow.
  MediaFeatureValuesChanged(eRestyle_Subtree, NS_STYLE_HINT_REFLOW);

  // Recursively notify all remote leaf descendants that the
  // system theme has changed.
  nsContentUtils::CallOnAllRemoteChildren(mDocument->GetWindow(),
                                          NotifyThemeChanged, nullptr);
}

void
nsPresContext::SysColorChanged()
{
  if (!mPendingSysColorChanged) {
    sLookAndFeelChanged = true;
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::SysColorChangedInternal);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingSysColorChanged = true;
    }
  }
}

void
nsPresContext::SysColorChangedInternal()
{
  mPendingSysColorChanged = false;

  if (sLookAndFeelChanged) {
     // Don't use the cached values for the system colors
    LookAndFeel::Refresh();
    sLookAndFeelChanged = false;
  }

  // Reset default background and foreground colors for the document since
  // they may be using system colors
  GetDocumentColorPreferences();

  // The system color values are computed to colors in the style data,
  // so normal style data comparison is sufficient here.
  RebuildAllStyleData(nsChangeHint(0), nsRestyleHint(0));
}

void
nsPresContext::UIResolutionChanged()
{
  if (!mPendingUIResolutionChanged) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::UIResolutionChangedInternal);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingUIResolutionChanged = true;
    }
  }
}

void
nsPresContext::UIResolutionChangedSync()
{
  if (!mPendingUIResolutionChanged) {
    mPendingUIResolutionChanged = true;
    UIResolutionChangedInternal();
  }
}

/*static*/ bool
nsPresContext::UIResolutionChangedSubdocumentCallback(nsIDocument* aDocument,
                                                      void* aData)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* pc = shell->GetPresContext();
    if (pc) {
      pc->UIResolutionChangedInternal();
    }
  }
  return true;
}

static void
NotifyTabUIResolutionChanged(TabParent* aTab, void *aArg)
{
  aTab->UIResolutionChanged();
}

static void
NotifyChildrenUIResolutionChanged(nsIDOMWindow* aWindow)
{
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
  if (!piWin) {
    return;
  }
  nsCOMPtr<nsIDocument> doc = piWin->GetExtantDoc();
  nsRefPtr<nsPIWindowRoot> topLevelWin = nsContentUtils::GetWindowRoot(doc);
  if (!topLevelWin) {
    return;
  }
  topLevelWin->EnumerateBrowsers(NotifyTabUIResolutionChanged, nullptr);
}

void
nsPresContext::UIResolutionChangedInternal()
{
  mPendingUIResolutionChanged = false;

  mDeviceContext->CheckDPIChange();
  if (mCurAppUnitsPerDevPixel != AppUnitsPerDevPixel()) {
    AppUnitsPerDevPixelChanged();
  }

  // Recursively notify all remote leaf descendants of the change.
  NotifyChildrenUIResolutionChanged(mDocument->GetWindow());

  mDocument->EnumerateSubDocuments(UIResolutionChangedSubdocumentCallback,
                                   nullptr);
}

void
nsPresContext::EmulateMedium(const nsAString& aMediaType)
{
  nsIAtom* previousMedium = Medium();
  mIsEmulatingMedia = true;

  nsAutoString mediaType;
  nsContentUtils::ASCIIToLower(aMediaType, mediaType);

  mMediaEmulated = do_GetAtom(mediaType);
  if (mMediaEmulated != previousMedium && mShell) {
    MediaFeatureValuesChanged(nsRestyleHint(0), nsChangeHint(0));
  }
}

void nsPresContext::StopEmulatingMedium()
{
  nsIAtom* previousMedium = Medium();
  mIsEmulatingMedia = false;
  if (Medium() != previousMedium) {
    MediaFeatureValuesChanged(nsRestyleHint(0), nsChangeHint(0));
  }
}

void
nsPresContext::RebuildAllStyleData(nsChangeHint aExtraHint,
                                   nsRestyleHint aRestyleHint)
{
  if (!mShell) {
    // We must have been torn down. Nothing to do here.
    return;
  }

  mUsesRootEMUnits = false;
  mUsesExChUnits = false;
  mUsesViewportUnits = false;
  mDocument->RebuildUserFontSet();
  RebuildCounterStyles();

  RestyleManager()->RebuildAllStyleData(aExtraHint, aRestyleHint);
}

void
nsPresContext::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                            nsRestyleHint aRestyleHint)
{
  if (!mShell) {
    // We must have been torn down. Nothing to do here.
    return;
  }
  RestyleManager()->PostRebuildAllStyleDataEvent(aExtraHint, aRestyleHint);
}

void
nsPresContext::MediaFeatureValuesChanged(nsRestyleHint aRestyleHint,
                                         nsChangeHint aChangeHint)
{
  mPendingMediaFeatureValuesChanged = false;

  // MediumFeaturesChanged updates the applied rules, so it always gets called.
  if (mShell && mShell->StyleSet()->MediumFeaturesChanged()) {
    aRestyleHint |= eRestyle_Subtree;
  }

  if (mUsesViewportUnits && mPendingViewportChange) {
    // Rebuild all style data without rerunning selector matching.
    aRestyleHint |= eRestyle_ForceDescendants;
  }

  if (aRestyleHint || aChangeHint) {
    RebuildAllStyleData(aChangeHint, aRestyleHint);
  }

  mPendingViewportChange = false;

  if (mDocument->IsBeingUsedAsImage()) {
    MOZ_ASSERT(PR_CLIST_IS_EMPTY(mDocument->MediaQueryLists()));
    return;
  }

  mDocument->NotifyMediaFeatureValuesChanged();

  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  // Media query list listeners should be notified from a queued task
  // (in HTML5 terms), although we also want to notify them on certain
  // flushes.  (We're already running off an event.)
  //
  // Note that we do this after the new style from media queries in
  // style sheets has been computed.

  if (!PR_CLIST_IS_EMPTY(mDocument->MediaQueryLists())) {
    // We build a list of all the notifications we're going to send
    // before we send any of them.  (The spec says the notifications
    // should be a queued task, so any removals that happen during the
    // notifications shouldn't affect what gets notified.)  Furthermore,
    // we hold strong pointers to everything we're going to make
    // notification calls to, since each notification involves calling
    // arbitrary script that might otherwise destroy these objects, or,
    // for that matter, |this|.
    //
    // Note that we intentionally send the notifications to media query
    // list in the order they were created and, for each list, to the
    // listeners in the order added.
    MediaQueryList::NotifyList notifyList;
    for (PRCList *l = PR_LIST_HEAD(mDocument->MediaQueryLists());
         l != mDocument->MediaQueryLists(); l = PR_NEXT_LINK(l)) {
      MediaQueryList *mql = static_cast<MediaQueryList*>(l);
      mql->MediumFeaturesChanged(notifyList);
    }

    if (!notifyList.IsEmpty()) {
      for (uint32_t i = 0, i_end = notifyList.Length(); i != i_end; ++i) {
        nsAutoMicroTask mt;
        MediaQueryList::HandleChangeData &d = notifyList[i];
        ErrorResult result;
        d.callback->Call(*d.mql, result);
      }
    }

    // NOTE:  When |notifyList| goes out of scope, our destructor could run.
  }
}

void
nsPresContext::PostMediaFeatureValuesChangedEvent()
{
  // FIXME: We should probably replace this event with use of
  // nsRefreshDriver::AddStyleFlushObserver (except the pres shell would
  // need to track whether it's been added).
  if (!mPendingMediaFeatureValuesChanged) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::HandleMediaFeatureValuesChangedEvent);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingMediaFeatureValuesChanged = true;
      mDocument->SetNeedStyleFlush();
    }
  }
}

void
nsPresContext::HandleMediaFeatureValuesChangedEvent()
{
  // Null-check mShell in case the shell has been destroyed (and the
  // event is the only thing holding the pres context alive).
  if (mPendingMediaFeatureValuesChanged && mShell) {
    MediaFeatureValuesChanged(nsRestyleHint(0));
  }
}

nsCompatibility
nsPresContext::CompatibilityMode() const
{
  return Document()->GetCompatibilityMode();
}

void
nsPresContext::SetPaginatedScrolling(bool aPaginated)
{
  if (mType == eContext_PrintPreview || mType == eContext_PageLayout)
    mCanPaginatedScroll = aPaginated;
}

void
nsPresContext::SetPrintSettings(nsIPrintSettings *aPrintSettings)
{
  if (mMedium == nsGkAtoms::print)
    mPrintSettings = aPrintSettings;
}

bool
nsPresContext::EnsureVisible()
{
  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (docShell) {
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    // Make sure this is the content viewer we belong with
    if (cv) {
      nsRefPtr<nsPresContext> currentPresContext;
      cv->GetPresContext(getter_AddRefs(currentPresContext));
      if (currentPresContext == this) {
        // OK, this is us.  We want to call Show() on the content viewer.
        nsresult result = cv->Show();
        if (NS_SUCCEEDED(result)) {
          return true;
        }
      }
    }
  }
  return false;
}

#ifdef MOZ_REFLOW_PERF
void
nsPresContext::CountReflows(const char * aName, nsIFrame * aFrame)
{
  if (mShell) {
    mShell->CountReflows(aName, aFrame);
  }
}
#endif

void
nsPresContext::UpdateIsChrome()
{
  mIsChrome = mContainer &&
              nsIDocShellTreeItem::typeChrome == mContainer->ItemType();
}

/* virtual */ bool
nsPresContext::HasAuthorSpecifiedRules(nsIFrame *aFrame, uint32_t ruleTypeMask) const
{
  return
    nsRuleNode::HasAuthorSpecifiedRules(aFrame->StyleContext(),
                                        ruleTypeMask,
                                        UseDocumentColors());
}

gfxUserFontSet*
nsPresContext::GetUserFontSet()
{
  return mDocument->GetUserFontSet();
}

void
nsPresContext::UserFontSetUpdated(gfxUserFontEntry* aUpdatedFont)
{
  if (!mShell)
    return;

  bool usePlatformFontList = true;
#if defined(MOZ_WIDGET_GTK)
    usePlatformFontList = gfxPlatformGtk::UseFcFontList();
#elif defined(MOZ_WIDGET_QT)
    usePlatformFontList = false;
#endif

  // xxx - until the Linux platform font list is always used, use full
  // restyle to force updates with gfxPangoFontGroup usage
  // Note: this method is called without a font when rules in the userfont set
  // are updated, which may occur during reflow as a result of the lazy
  // initialization of the userfont set. It would be better to avoid a full
  // restyle but until this method is only called outside of reflow, schedule a
  // full restyle in these cases.
  if (!usePlatformFontList || !aUpdatedFont) {
    PostRebuildAllStyleDataEvent(NS_STYLE_HINT_REFLOW, eRestyle_ForceDescendants);
    return;
  }

  // Special case - if either the 'ex' or 'ch' units are used, these
  // depend upon font metrics. Updating this information requires
  // rebuilding the rule tree from the top, avoiding the reuse of cached
  // data even when no style rules have changed.
  if (UsesExChUnits()) {
    PostRebuildAllStyleDataEvent(nsChangeHint(0), eRestyle_ForceDescendants);
  }

  // Iterate over the frame tree looking for frames associated with the
  // downloadable font family in question. If a frame's nsStyleFont has
  // the name, check the font group associated with the metrics to see if
  // it contains that specific font (i.e. the one chosen within the family
  // given the weight, width, and slant from the nsStyleFont). If it does,
  // mark that frame dirty and skip inspecting its descendants.
  nsIFrame* root = mShell->GetRootFrame();
  if (root) {
    nsFontFaceUtils::MarkDirtyForFontChange(root, aUpdatedFont);
  }
}

void
nsPresContext::FlushCounterStyles()
{
  if (!mShell) {
    return; // we've been torn down
  }
  if (mCounterStyleManager->IsInitial()) {
    // Still in its initial state, no need to clean.
    return;
  }

  if (mCounterStylesDirty) {
    bool changed = mCounterStyleManager->NotifyRuleChanged();
    if (changed) {
      PresShell()->NotifyCounterStylesAreDirty();
      PostRebuildAllStyleDataEvent(NS_STYLE_HINT_REFLOW,
                                   eRestyle_ForceDescendants);
    }
    mCounterStylesDirty = false;
  }
}

void
nsPresContext::RebuildCounterStyles()
{
  if (mCounterStyleManager->IsInitial()) {
    // Still in its initial state, no need to reset.
    return;
  }

  mCounterStylesDirty = true;
  mDocument->SetNeedStyleFlush();
  if (!mPostedFlushCounterStyles) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::HandleRebuildCounterStyles);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPostedFlushCounterStyles = true;
    }
  }
}

void
nsPresContext::NotifyMissingFonts()
{
  if (mMissingFonts) {
    mMissingFonts->Flush();
  }
}

void
nsPresContext::EnsureSafeToHandOutCSSRules()
{
  if (!mShell->StyleSet()->EnsureUniqueInnerOnCSSSheets()) {
    // Nothing to do.
    return;
  }

  RebuildAllStyleData(nsChangeHint(0), eRestyle_Subtree);
}

void
nsPresContext::FireDOMPaintEvent(nsInvalidateRequestList* aList)
{
  nsPIDOMWindow* ourWindow = mDocument->GetWindow();
  if (!ourWindow)
    return;

  nsCOMPtr<EventTarget> dispatchTarget = do_QueryInterface(ourWindow);
  nsCOMPtr<EventTarget> eventTarget = dispatchTarget;
  if (!IsChrome() && !mSendAfterPaintToContent) {
    // Don't tell the window about this event, it should not know that
    // something happened in a subdocument. Tell only the chrome event handler.
    // (Events sent to the window get propagated to the chrome event handler
    // automatically.)
    dispatchTarget = do_QueryInterface(ourWindow->GetParentTarget());
    if (!dispatchTarget) {
      return;
    }
  }
  // Events sent to the window get propagated to the chrome event handler
  // automatically.
  nsCOMPtr<nsIDOMEvent> event;
  // This will empty our list in case dispatching the event causes more damage
  // (hopefully it won't, or we're likely to get an infinite loop! At least
  // it won't be blocking app execution though).
  NS_NewDOMNotifyPaintEvent(getter_AddRefs(event), eventTarget, this, nullptr,
                            NS_AFTERPAINT, aList);
  if (!event) {
    return;
  }

  // Even if we're not telling the window about the event (so eventTarget is
  // the chrome event handler, not the window), the window is still
  // logically the event target.
  event->SetTarget(eventTarget);
  event->SetTrusted(true);
  EventDispatcher::DispatchDOMEvent(dispatchTarget, nullptr, event, this,
                                    nullptr);
}

static bool
MayHavePaintEventListenerSubdocumentCallback(nsIDocument* aDocument, void* aData)
{
  bool *result = static_cast<bool*>(aData);
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* pc = shell->GetPresContext();
    if (pc) {
      *result = pc->MayHavePaintEventListenerInSubDocument();

      // If we found a paint event listener, then we can stop enumerating
      // sub documents.
      return !*result;
    }
  }
  return true;
}

static bool
MayHavePaintEventListener(nsPIDOMWindow* aInnerWindow)
{
  if (!aInnerWindow)
    return false;
  if (aInnerWindow->HasPaintEventListeners())
    return true;

  EventTarget* parentTarget = aInnerWindow->GetParentTarget();
  if (!parentTarget)
    return false;

  EventListenerManager* manager = nullptr;
  if ((manager = parentTarget->GetExistingListenerManager()) &&
      manager->MayHavePaintEventListener()) {
    return true;
  }

  nsCOMPtr<nsINode> node;
  if (parentTarget != aInnerWindow->GetChromeEventHandler()) {
    nsCOMPtr<nsIInProcessContentFrameMessageManager> mm =
      do_QueryInterface(parentTarget);
    if (mm) {
      node = mm->GetOwnerContent();
    }
  }

  if (!node) {
    node = do_QueryInterface(parentTarget);
  }
  if (node)
    return MayHavePaintEventListener(node->OwnerDoc()->GetInnerWindow());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(parentTarget);
  if (window)
    return MayHavePaintEventListener(window);

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(parentTarget);
  EventTarget* tabChildGlobal;
  return root &&
         (tabChildGlobal = root->GetParentTarget()) &&
         (manager = tabChildGlobal->GetExistingListenerManager()) &&
         manager->MayHavePaintEventListener();
}

bool
nsPresContext::MayHavePaintEventListener()
{
  return ::MayHavePaintEventListener(mDocument->GetInnerWindow());
}

bool
nsPresContext::MayHavePaintEventListenerInSubDocument()
{
  if (MayHavePaintEventListener()) {
    return true;
  }

  bool result = false;
  mDocument->EnumerateSubDocuments(MayHavePaintEventListenerSubdocumentCallback, &result);
  return result;
}

void
nsPresContext::NotifyInvalidation(uint32_t aFlags)
{
  nsIFrame* rootFrame = PresShell()->FrameManager()->GetRootFrame();
  NotifyInvalidation(rootFrame->GetVisualOverflowRect(), aFlags);
  mAllInvalidated = true;
}

void
nsPresContext::NotifyInvalidation(const nsIntRect& aRect, uint32_t aFlags)
{
  nsRect rect(DevPixelsToAppUnits(aRect.x),
              DevPixelsToAppUnits(aRect.y),
              DevPixelsToAppUnits(aRect.width),
              DevPixelsToAppUnits(aRect.height));
  NotifyInvalidation(rect, aFlags);
}

void
nsPresContext::NotifyInvalidation(const nsRect& aRect, uint32_t aFlags)
{
  MOZ_ASSERT(GetContainerWeak(), "Invalidation in detached pres context");

  // If there is no paint event listener, then we don't need to fire
  // the asynchronous event. We don't even need to record invalidation.
  // MayHavePaintEventListener is pretty cheap and we could make it
  // even cheaper by providing a more efficient
  // nsPIDOMWindow::GetListenerManager.

  if (mAllInvalidated) {
    return;
  }

  nsPresContext* pc;
  for (pc = this; pc; pc = pc->GetParentPresContext()) {
    if (pc->mFireAfterPaintEvents)
      break;
    pc->mFireAfterPaintEvents = true;
  }
  if (!pc) {
    nsRootPresContext* rpc = GetRootPresContext();
    if (rpc) {
      rpc->EnsureEventualDidPaintEvent();
    }
  }

  nsInvalidateRequestList::Request* request =
    mInvalidateRequestsSinceLastPaint.mRequests.AppendElement();
  if (!request)
    return;

  request->mRect = aRect;
  request->mFlags = aFlags;
}

/* static */ void
nsPresContext::NotifySubDocInvalidation(ContainerLayer* aContainer,
                                        const nsIntRegion& aRegion)
{
  ContainerLayerPresContext *data =
    static_cast<ContainerLayerPresContext*>(
      aContainer->GetUserData(&gNotifySubDocInvalidationData));
  if (!data) {
    return;
  }

  nsIntPoint topLeft = aContainer->GetVisibleRegion().GetBounds().TopLeft();

  nsIntRegionRectIterator iter(aRegion);
  while (const nsIntRect* r = iter.Next()) {
    nsIntRect rect = *r;
    //PresContext coordinate space is relative to the start of our visible
    // region. Is this really true? This feels like the wrong way to get the right
    // answer.
    rect.MoveBy(-topLeft);
    data->mPresContext->NotifyInvalidation(rect, 0);
  }
}

void
nsPresContext::SetNotifySubDocInvalidationData(ContainerLayer* aContainer)
{
  ContainerLayerPresContext* pres = new ContainerLayerPresContext;
  pres->mPresContext = this;
  aContainer->SetUserData(&gNotifySubDocInvalidationData, pres);
}

/* static */ void
nsPresContext::ClearNotifySubDocInvalidationData(ContainerLayer* aContainer)
{
  aContainer->SetUserData(&gNotifySubDocInvalidationData, nullptr);
}

struct NotifyDidPaintSubdocumentCallbackClosure {
  uint32_t mFlags;
  bool mNeedsAnotherDidPaintNotification;
};
static bool
NotifyDidPaintSubdocumentCallback(nsIDocument* aDocument, void* aData)
{
  NotifyDidPaintSubdocumentCallbackClosure* closure =
    static_cast<NotifyDidPaintSubdocumentCallbackClosure*>(aData);
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* pc = shell->GetPresContext();
    if (pc) {
      pc->NotifyDidPaintForSubtree(closure->mFlags);
      if (pc->IsDOMPaintEventPending()) {
        closure->mNeedsAnotherDidPaintNotification = true;
      }
    }
  }
  return true;
}

class DelayedFireDOMPaintEvent : public nsRunnable {
public:
  DelayedFireDOMPaintEvent(nsPresContext* aPresContext,
                           nsInvalidateRequestList* aList)
    : mPresContext(aPresContext)
  {
    MOZ_ASSERT(mPresContext->GetContainerWeak(),
               "DOMPaintEvent requested for a detached pres context");
    mList.TakeFrom(aList);
  }
  NS_IMETHOD Run() override
  {
    // The pres context might have been detached during the delay -
    // that's fine, just don't fire the event.
    if (mPresContext->GetContainerWeak()) {
      mPresContext->FireDOMPaintEvent(&mList);
    }
    return NS_OK;
  }

  nsRefPtr<nsPresContext> mPresContext;
  nsInvalidateRequestList mList;
};

void
nsPresContext::NotifyDidPaintForSubtree(uint32_t aFlags)
{
  if (IsRoot()) {
    static_cast<nsRootPresContext*>(this)->CancelDidPaintTimer();

    if (!mFireAfterPaintEvents) {
      return;
    }
  }

  if (!PresShell()->IsVisible() && !mFireAfterPaintEvents) {
    return;
  }

  // Non-root prescontexts fire MozAfterPaint to all their descendants
  // unconditionally, even if no invalidations have been collected. This is
  // because we don't want to eat the cost of collecting invalidations for
  // every subdocument (which would require putting every subdocument in its
  // own layer).

  if (aFlags & nsIPresShell::PAINT_LAYERS) {
    mUndeliveredInvalidateRequestsBeforeLastPaint.TakeFrom(
        &mInvalidateRequestsSinceLastPaint);
    mAllInvalidated = false;
  }
  if (aFlags & nsIPresShell::PAINT_COMPOSITE) {
    nsCOMPtr<nsIRunnable> ev =
      new DelayedFireDOMPaintEvent(this, &mUndeliveredInvalidateRequestsBeforeLastPaint);
    nsContentUtils::AddScriptRunner(ev);
  }

  NotifyDidPaintSubdocumentCallbackClosure closure = { aFlags, false };
  mDocument->EnumerateSubDocuments(NotifyDidPaintSubdocumentCallback, &closure);

  if (!closure.mNeedsAnotherDidPaintNotification &&
      mInvalidateRequestsSinceLastPaint.IsEmpty() &&
      mUndeliveredInvalidateRequestsBeforeLastPaint.IsEmpty()) {
    // Nothing more to do for the moment.
    mFireAfterPaintEvents = false;
  } else {
    if (IsRoot()) {
      static_cast<nsRootPresContext*>(this)->EnsureEventualDidPaintEvent();
    }
  }
}

bool
nsPresContext::HasCachedStyleData()
{
  return mShell && mShell->StyleSet()->HasCachedStyleData();
}

static bool sGotInterruptEnv = false;
enum InterruptMode {
  ModeRandom,
  ModeCounter,
  ModeEvent
};
// Controlled by the GECKO_REFLOW_INTERRUPT_MODE env var; allowed values are
// "random" (except on Windows) or "counter".  If neither is used, the mode is
// ModeEvent.
static InterruptMode sInterruptMode = ModeEvent;
#ifndef XP_WIN
// Used for the "random" mode.  Controlled by the GECKO_REFLOW_INTERRUPT_SEED
// env var.
static uint32_t sInterruptSeed = 1;
#endif
// Used for the "counter" mode.  This is the number of unskipped interrupt
// checks that have to happen before we interrupt.  Controlled by the
// GECKO_REFLOW_INTERRUPT_FREQUENCY env var.
static uint32_t sInterruptMaxCounter = 10;
// Used for the "counter" mode.  This counts up to sInterruptMaxCounter and is
// then reset to 0.
static uint32_t sInterruptCounter;
// Number of interrupt checks to skip before really trying to interrupt.
// Controlled by the GECKO_REFLOW_INTERRUPT_CHECKS_TO_SKIP env var.
static uint32_t sInterruptChecksToSkip = 200;
// Number of milliseconds that a reflow should be allowed to run for before we
// actually allow interruption.  Controlled by the
// GECKO_REFLOW_MIN_NOINTERRUPT_DURATION env var.  Can't be initialized here,
// because TimeDuration/TimeStamp is not safe to use in static constructors..
static TimeDuration sInterruptTimeout;

static void GetInterruptEnv()
{
  char *ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_MODE");
  if (ev) {
#ifndef XP_WIN
    if (PL_strcasecmp(ev, "random") == 0) {
      ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_SEED");
      if (ev) {
        sInterruptSeed = atoi(ev);
      }
      srandom(sInterruptSeed);
      sInterruptMode = ModeRandom;
    } else
#endif
      if (PL_strcasecmp(ev, "counter") == 0) {
      ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_FREQUENCY");
      if (ev) {
        sInterruptMaxCounter = atoi(ev);
      }
      sInterruptCounter = 0;
      sInterruptMode = ModeCounter;
    }
  }
  ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_CHECKS_TO_SKIP");
  if (ev) {
    sInterruptChecksToSkip = atoi(ev);
  }

  ev = PR_GetEnv("GECKO_REFLOW_MIN_NOINTERRUPT_DURATION");
  int duration_ms = ev ? atoi(ev) : 100;
  sInterruptTimeout = TimeDuration::FromMilliseconds(duration_ms);
}

bool
nsPresContext::HavePendingInputEvent()
{
  switch (sInterruptMode) {
#ifndef XP_WIN
    case ModeRandom:
      return (random() & 1);
#endif
    case ModeCounter:
      if (sInterruptCounter < sInterruptMaxCounter) {
        ++sInterruptCounter;
        return false;
      }
      sInterruptCounter = 0;
      return true;
    default:
    case ModeEvent: {
      nsIFrame* f = PresShell()->GetRootFrame();
      if (f) {
        nsIWidget* w = f->GetNearestWidget();
        if (w) {
          return w->HasPendingInputEvent();
        }
      }
      return false;
    }
  }
}

void
nsPresContext::NotifyFontFaceSetOnRefresh()
{
  FontFaceSet* set = mDocument->GetFonts();
  if (set) {
    set->DidRefresh();
  }
}

bool
nsPresContext::HasPendingRestyleOrReflow()
{
  return (mRestyleManager && mRestyleManager->HasPendingRestyles()) ||
         PresShell()->HasPendingReflow();
}

void
nsPresContext::ReflowStarted(bool aInterruptible)
{
#ifdef NOISY_INTERRUPTIBLE_REFLOW
  if (!aInterruptible) {
    printf("STARTING NONINTERRUPTIBLE REFLOW\n");
  }
#endif
  // We don't support interrupting in paginated contexts, since page
  // sequences only handle initial reflow
  mInterruptsEnabled = aInterruptible && !IsPaginated() &&
                       nsLayoutUtils::InterruptibleReflowEnabled();

  // Don't set mHasPendingInterrupt based on HavePendingInputEvent() here.  If
  // we ever change that, then we need to update the code in
  // PresShell::DoReflow to only add the just-reflown root to dirty roots if
  // it's actually dirty.  Otherwise we can end up adding a root that has no
  // interruptible descendants, just because we detected an interrupt at reflow
  // start.
  mHasPendingInterrupt = false;

  mInterruptChecksToSkip = sInterruptChecksToSkip;

  if (mInterruptsEnabled) {
    mReflowStartTime = TimeStamp::Now();
  }
}

bool
nsPresContext::CheckForInterrupt(nsIFrame* aFrame)
{
  if (mHasPendingInterrupt) {
    mShell->FrameNeedsToContinueReflow(aFrame);
    return true;
  }

  if (!sGotInterruptEnv) {
    sGotInterruptEnv = true;
    GetInterruptEnv();
  }

  if (!mInterruptsEnabled) {
    return false;
  }

  if (mInterruptChecksToSkip > 0) {
    --mInterruptChecksToSkip;
    return false;
  }
  mInterruptChecksToSkip = sInterruptChecksToSkip;

  // Don't interrupt if it's been less than sInterruptTimeout since we started
  // the reflow.
  mHasPendingInterrupt =
    TimeStamp::Now() - mReflowStartTime > sInterruptTimeout &&
    HavePendingInputEvent() &&
    !IsChrome();
  if (mHasPendingInterrupt) {
#ifdef NOISY_INTERRUPTIBLE_REFLOW
    printf("*** DETECTED pending interrupt (time=%lld)\n", PR_Now());
#endif /* NOISY_INTERRUPTIBLE_REFLOW */
    mShell->FrameNeedsToContinueReflow(aFrame);
  }
  return mHasPendingInterrupt;
}

nsIFrame*
nsPresContext::GetPrimaryFrameFor(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "Don't do that");
  if (GetPresShell() &&
      GetPresShell()->GetDocument() == aContent->GetComposedDoc()) {
    return aContent->GetPrimaryFrame();
  }
  return nullptr;
}


size_t
nsPresContext::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mPropertyTable.SizeOfExcludingThis(aMallocSizeOf);
         mLangGroupFontPrefs.SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of other members may be added later if DMD finds it is
  // worthwhile.
}

bool
nsPresContext::IsRootContentDocument()
{
  // We are a root content document if: we are not a resource doc, we are
  // not chrome, and we either have no parent or our parent is chrome.
  if (mDocument->IsResourceDoc()) {
    return false;
  }
  if (IsChrome()) {
    return false;
  }
  // We may not have a root frame, so use views.
  nsView* view = PresShell()->GetViewManager()->GetRootView();
  if (!view) {
    return false;
  }
  view = view->GetParent(); // anonymous inner view
  if (!view) {
    return true;
  }
  view = view->GetParent(); // subdocumentframe's view
  if (!view) {
    return true;
  }

  nsIFrame* f = view->GetFrame();
  return (f && f->PresContext()->IsChrome());
}

bool
nsPresContext::IsCrossProcessRootContentDocument()
{
  if (!IsRootContentDocument()) {
    return false;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    return true;
  }

  TabChild* tabChild = TabChild::GetFrom(mShell);
  return (tabChild && tabChild->IsRootContentDocument());
}

bool nsPresContext::GetPaintFlashing() const
{
  if (!mPaintFlashingInitialized) {
    bool pref = Preferences::GetBool("nglayout.debug.paint_flashing");
    if (!pref && IsChrome()) {
      pref = Preferences::GetBool("nglayout.debug.paint_flashing_chrome");
    }
    mPaintFlashing = pref;
    mPaintFlashingInitialized = true;
  }
  return mPaintFlashing;
}

int32_t
nsPresContext::AppUnitsPerDevPixel() const
{
  return mDeviceContext->AppUnitsPerDevPixel();
}

nscoord
nsPresContext::GfxUnitsToAppUnits(gfxFloat aGfxUnits) const
{
  return mDeviceContext->GfxUnitsToAppUnits(aGfxUnits);
}

gfxFloat
nsPresContext::AppUnitsToGfxUnits(nscoord aAppUnits) const
{
  return mDeviceContext->AppUnitsToGfxUnits(aAppUnits);
}

bool
nsPresContext::IsDeviceSizePageSize()
{
  bool isDeviceSizePageSize = false;
  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (docShell) {
    isDeviceSizePageSize = docShell->GetDeviceSizeIsPageSize();
  }
  return isDeviceSizePageSize;
}

nsRootPresContext::nsRootPresContext(nsIDocument* aDocument,
                                     nsPresContextType aType)
  : nsPresContext(aDocument, aType),
    mDOMGeneration(0)
{
}

nsRootPresContext::~nsRootPresContext()
{
  NS_ASSERTION(mRegisteredPlugins.Count() == 0,
               "All plugins should have been unregistered");
  CancelDidPaintTimer();
  CancelApplyPluginGeometryTimer();
}

/* virtual */ void
nsRootPresContext::Detach()
{
  CancelDidPaintTimer();
  // XXXmats maybe also CancelApplyPluginGeometryTimer(); ?
  nsPresContext::Detach();
}

void
nsRootPresContext::RegisterPluginForGeometryUpdates(nsIContent* aPlugin)
{
  mRegisteredPlugins.PutEntry(aPlugin);
}

void
nsRootPresContext::UnregisterPluginForGeometryUpdates(nsIContent* aPlugin)
{
  mRegisteredPlugins.RemoveEntry(aPlugin);
}

static PLDHashOperator
SetPluginHidden(nsRefPtrHashKey<nsIContent>* aEntry, void* userArg)
{
  nsIFrame* root = static_cast<nsIFrame*>(userArg);
  nsPluginFrame* f = static_cast<nsPluginFrame*>(aEntry->GetKey()->GetPrimaryFrame());
  if (!f) {
    NS_WARNING("Null frame in SetPluginHidden");
    return PL_DHASH_NEXT;
  }
  if (!nsLayoutUtils::IsAncestorFrameCrossDoc(root, f)) {
    // f is not managed by this frame so we should ignore it.
    return PL_DHASH_NEXT;
  }
  f->SetEmptyWidgetConfiguration();
  return PL_DHASH_NEXT;
}

void
nsRootPresContext::ComputePluginGeometryUpdates(nsIFrame* aFrame,
                                                nsDisplayListBuilder* aBuilder,
                                                nsDisplayList* aList)
{
  if (mRegisteredPlugins.Count() == 0) {
    return;
  }

  // Initially make the next state for each plugin descendant of aFrame be
  // "hidden". Plugins that are visible will have their next state set to
  // unhidden by nsDisplayPlugin::ComputeVisibility.
  mRegisteredPlugins.EnumerateEntries(SetPluginHidden, aFrame);

  nsIFrame* rootFrame = FrameManager()->GetRootFrame();

  if (rootFrame && aBuilder->ContainsPluginItem()) {
    aBuilder->SetForPluginGeometry();
    aBuilder->SetAccurateVisibleRegions();
    // Merging and flattening has already been done and we should not do it
    // again. nsDisplayScroll(Info)Layer doesn't support trying to flatten
    // again.
    aBuilder->SetAllowMergingAndFlattening(false);
    nsRegion region = rootFrame->GetVisualOverflowRectRelativeToSelf();
    // nsDisplayPlugin::ComputeVisibility will automatically set a non-hidden
    // widget configuration for the plugin, if it's visible.
    aList->ComputeVisibilityForRoot(aBuilder, &region);
  }

#ifdef XP_MACOSX
  // We control painting of Mac plugins, so just apply geometry updates now.
  // This is not happening during a paint event.
  ApplyPluginGeometryUpdates();
#else
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    InitApplyPluginGeometryTimer();
  }
#endif
}

static void
ApplyPluginGeometryUpdatesCallback(nsITimer *aTimer, void *aClosure)
{
  static_cast<nsRootPresContext*>(aClosure)->ApplyPluginGeometryUpdates();
}

void
nsRootPresContext::InitApplyPluginGeometryTimer()
{
  if (mApplyPluginGeometryTimer) {
    return;
  }

  // We'll apply the plugin geometry updates during the next compositing paint in this
  // presContext (either from nsPresShell::WillPaintWindow or from
  // nsPresShell::DidPaintWindow, depending on the platform).  But paints might
  // get optimized away if the old plugin geometry covers the invalid region,
  // so set a backup timer to do this too.  We want to make sure this
  // won't fire before our normal paint notifications, if those would
  // update the geometry, so set it for double the refresh driver interval.
  mApplyPluginGeometryTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mApplyPluginGeometryTimer) {
    mApplyPluginGeometryTimer->
      InitWithFuncCallback(ApplyPluginGeometryUpdatesCallback, this,
                           nsRefreshDriver::DefaultInterval() * 2,
                           nsITimer::TYPE_ONE_SHOT);
  }
}

void
nsRootPresContext::CancelApplyPluginGeometryTimer()
{
  if (mApplyPluginGeometryTimer) {
    mApplyPluginGeometryTimer->Cancel();
    mApplyPluginGeometryTimer = nullptr;
  }
}

#ifndef XP_MACOSX

static bool
HasOverlap(const nsIntPoint& aOffset1, const nsTArray<nsIntRect>& aClipRects1,
           const nsIntPoint& aOffset2, const nsTArray<nsIntRect>& aClipRects2)
{
  nsIntPoint offsetDelta = aOffset1 - aOffset2;
  for (uint32_t i = 0; i < aClipRects1.Length(); ++i) {
    for (uint32_t j = 0; j < aClipRects2.Length(); ++j) {
      if ((aClipRects1[i] + offsetDelta).Intersects(aClipRects2[j]))
        return true;
    }
  }
  return false;
}

/**
 * Given a list of plugin windows to move to new locations, sort the list
 * so that for each window move, the window moves to a location that
 * does not intersect other windows. This minimizes flicker and repainting.
 * It's not always possible to do this perfectly, since in general
 * we might have cycles. But we do our best.
 * We need to take into account that windows are clipped to particular
 * regions and the clip regions change as the windows are moved.
 */
static void
SortConfigurations(nsTArray<nsIWidget::Configuration>* aConfigurations)
{
  if (aConfigurations->Length() > 10) {
    // Give up, we don't want to get bogged down here
    return;
  }

  nsTArray<nsIWidget::Configuration> pluginsToMove;
  pluginsToMove.SwapElements(*aConfigurations);

  // Our algorithm is quite naive. At each step we try to identify
  // a window that can be moved to its new location that won't overlap
  // any other windows at the new location. If there is no such
  // window, we just move the last window in the list anyway.
  while (!pluginsToMove.IsEmpty()) {
    // Find a window whose destination does not overlap any other window
    uint32_t i;
    for (i = 0; i + 1 < pluginsToMove.Length(); ++i) {
      nsIWidget::Configuration* config = &pluginsToMove[i];
      bool foundOverlap = false;
      for (uint32_t j = 0; j < pluginsToMove.Length(); ++j) {
        if (i == j)
          continue;
        nsIntRect bounds;
        pluginsToMove[j].mChild->GetBounds(bounds);
        nsAutoTArray<nsIntRect,1> clipRects;
        pluginsToMove[j].mChild->GetWindowClipRegion(&clipRects);
        if (HasOverlap(bounds.TopLeft(), clipRects,
                       config->mBounds.TopLeft(),
                       config->mClipRegion)) {
          foundOverlap = true;
          break;
        }
      }
      if (!foundOverlap)
        break;
    }
    // Note that we always move the last plugin in pluginsToMove, if we
    // can't find any other plugin to move
    aConfigurations->AppendElement(pluginsToMove[i]);
    pluginsToMove.RemoveElementAt(i);
  }
}

struct PluginGetGeometryUpdateClosure {
  nsTArray<nsIWidget::Configuration> mConfigurations;
};
static PLDHashOperator
PluginGetGeometryUpdate(nsRefPtrHashKey<nsIContent>* aEntry, void* userArg)
{
  PluginGetGeometryUpdateClosure* closure =
    static_cast<PluginGetGeometryUpdateClosure*>(userArg);
  nsPluginFrame* f = static_cast<nsPluginFrame*>(aEntry->GetKey()->GetPrimaryFrame());
  if (!f) {
    NS_WARNING("Null frame in GetPluginGeometryUpdate");
    return PL_DHASH_NEXT;
  }
  f->GetWidgetConfiguration(&closure->mConfigurations);
  return PL_DHASH_NEXT;
}

#endif  // #ifndef XP_MACOSX

static PLDHashOperator
PluginDidSetGeometryEnumerator(nsRefPtrHashKey<nsIContent>* aEntry, void* userArg)
{
  nsPluginFrame* f = static_cast<nsPluginFrame*>(aEntry->GetKey()->GetPrimaryFrame());
  if (!f) {
    NS_WARNING("Null frame in PluginDidSetGeometryEnumerator");
    return PL_DHASH_NEXT;
  }
  f->DidSetWidgetGeometry();
  return PL_DHASH_NEXT;
}

void
nsRootPresContext::ApplyPluginGeometryUpdates()
{
#ifndef XP_MACOSX
  CancelApplyPluginGeometryTimer();

  PluginGetGeometryUpdateClosure closure;
  mRegisteredPlugins.EnumerateEntries(PluginGetGeometryUpdate, &closure);
  // Walk mRegisteredPlugins and ask each plugin for its configuration
  if (!closure.mConfigurations.IsEmpty()) {
    nsIWidget* widget = closure.mConfigurations[0].mChild->GetParent();
    NS_ASSERTION(widget, "Plugins must have a parent window");
    SortConfigurations(&closure.mConfigurations);
    widget->ConfigureChildren(closure.mConfigurations);
  }
#endif  // #ifndef XP_MACOSX

  mRegisteredPlugins.EnumerateEntries(PluginDidSetGeometryEnumerator, nullptr);
}

void
nsRootPresContext::CollectPluginGeometryUpdates(LayerManager* aLayerManager)
{
#ifndef XP_MACOSX
  // Collect and pass plugin widget configurations down to the compositor
  // for transmission to the chrome process.
  NS_ASSERTION(aLayerManager, "layer manager is invalid!");
  mozilla::layers::ClientLayerManager* clm = aLayerManager->AsClientLayerManager();
  PluginGetGeometryUpdateClosure closure;
  mRegisteredPlugins.EnumerateEntries(PluginGetGeometryUpdate, &closure);
  if (closure.mConfigurations.IsEmpty()) {
    mRegisteredPlugins.EnumerateEntries(PluginDidSetGeometryEnumerator, nullptr);
    return;
  }
  SortConfigurations(&closure.mConfigurations);
  if (clm) {
    clm->StorePluginWidgetConfigurations(closure.mConfigurations);
  }
  mRegisteredPlugins.EnumerateEntries(PluginDidSetGeometryEnumerator, nullptr);
#endif  // #ifndef XP_MACOSX
}

static void
NotifyDidPaintForSubtreeCallback(nsITimer *aTimer, void *aClosure)
{
  nsPresContext* presContext = (nsPresContext*)aClosure;
  nsAutoScriptBlocker blockScripts;
  // This is a fallback if we don't get paint events for some reason
  // so we'll just pretend both layer painting and compositing happened.
  presContext->NotifyDidPaintForSubtree(
      nsIPresShell::PAINT_LAYERS | nsIPresShell::PAINT_COMPOSITE);
}

void
nsRootPresContext::EnsureEventualDidPaintEvent()
{
  if (mNotifyDidPaintTimer)
    return;
  mNotifyDidPaintTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (!mNotifyDidPaintTimer)
    return;
  mNotifyDidPaintTimer->InitWithFuncCallback(NotifyDidPaintForSubtreeCallback,
                                             (void*)this, 100, nsITimer::TYPE_ONE_SHOT);
}

void
nsRootPresContext::AddWillPaintObserver(nsIRunnable* aRunnable)
{
  if (!mWillPaintFallbackEvent.IsPending()) {
    mWillPaintFallbackEvent = new RunWillPaintObservers(this);
    NS_DispatchToMainThread(mWillPaintFallbackEvent.get());
  }
  mWillPaintObservers.AppendElement(aRunnable);
}

/**
 * Run all runnables that need to get called before the next paint.
 */
void
nsRootPresContext::FlushWillPaintObservers()
{
  mWillPaintFallbackEvent = nullptr;
  nsTArray<nsCOMPtr<nsIRunnable> > observers;
  observers.SwapElements(mWillPaintObservers);
  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->Run();
  }
}

size_t
nsRootPresContext::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return nsPresContext::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mNotifyDidPaintTimer
  // - mRegisteredPlugins
  // - mWillPaintObservers
  // - mWillPaintFallbackEvent
}
