/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindow_h__
#define __nsWindow_h__

#include "mozilla/ipc/SharedMemorySysV.h"

#include "nsAutoPtr.h"

#include "mozcontainer.h"
#include "nsWeakReference.h"

#include "nsIDragService.h"
#include "nsITimer.h"
#include "nsGkAtoms.h"

#include "nsBaseWidget.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif /* MOZ_X11 */

#ifdef ACCESSIBILITY
#include "mozilla/a11y/Accessible.h"
#endif
#include "mozilla/EventForwards.h"

#include "nsGtkIMModule.h"

#undef LOG
#ifdef MOZ_LOGGING

// make sure that logging is enabled before including prlog.h
#define FORCE_PR_LOG

#include "prlog.h"
#include "nsTArray.h"

extern PRLogModuleInfo *gWidgetLog;
extern PRLogModuleInfo *gWidgetFocusLog;
extern PRLogModuleInfo *gWidgetDragLog;
extern PRLogModuleInfo *gWidgetDrawLog;

#define LOG(args) PR_LOG(gWidgetLog, 4, args)
#define LOGFOCUS(args) PR_LOG(gWidgetFocusLog, 4, args)
#define LOGDRAG(args) PR_LOG(gWidgetDragLog, 4, args)
#define LOGDRAW(args) PR_LOG(gWidgetDrawLog, 4, args)

#else

#define LOG(args)
#define LOGFOCUS(args)
#define LOGDRAG(args)
#define LOGDRAW(args)

#endif /* MOZ_LOGGING */

class gfxASurface;
class gfxPattern;
class nsDragService;
#if defined(MOZ_X11) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
#  define MOZ_HAVE_SHMIMAGE

class nsShmImage;
#endif

class nsWindow : public nsBaseWidget, public nsSupportsWeakReference
{
public:
    nsWindow();
    virtual ~nsWindow();

    static void ReleaseGlobals();

    NS_DECL_ISUPPORTS_INHERITED
    
    void CommonCreate(nsIWidget *aParent, bool aListenForResizes);
    
    virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                   nsEventStatus& aStatus);
    
    // called when we are destroyed
    void OnDestroy(void);

    // called to check and see if a widget's dimensions are sane
    bool AreBoundsSane(void);

    // nsIWidget
    NS_IMETHOD         Create(nsIWidget        *aParent,
                              nsNativeWidget   aNativeParent,
                              const nsIntRect  &aRect,
                              nsDeviceContext *aContext,
                              nsWidgetInitData *aInitData);
    NS_IMETHOD         Destroy(void);
    virtual nsIWidget *GetParent();
    virtual float      GetDPI();
    virtual nsresult   SetParent(nsIWidget* aNewParent);
    NS_IMETHOD         SetModal(bool aModal);
    virtual bool       IsVisible() const;
    NS_IMETHOD         ConstrainPosition(bool aAllowSlop,
                                         int32_t *aX,
                                         int32_t *aY);
    virtual void       SetSizeConstraints(const SizeConstraints& aConstraints);
    NS_IMETHOD         Move(double aX,
                            double aY);
    NS_IMETHOD         Show             (bool aState);
    NS_IMETHOD         Resize           (double aWidth,
                                         double aHeight,
                                         bool   aRepaint);
    NS_IMETHOD         Resize           (double aX,
                                         double aY,
                                         double aWidth,
                                         double aHeight,
                                         bool   aRepaint);
    virtual bool       IsEnabled() const;


    NS_IMETHOD         PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                                   nsIWidget                  *aWidget,
                                   bool                        aActivate);
    void               SetZIndex(int32_t aZIndex);
    NS_IMETHOD         SetSizeMode(int32_t aMode);
    NS_IMETHOD         Enable(bool aState);
    NS_IMETHOD         SetFocus(bool aRaise = false);
    NS_IMETHOD         GetScreenBounds(nsIntRect &aRect);
    NS_IMETHOD         GetClientBounds(nsIntRect &aRect);
    virtual nsIntPoint GetClientOffset();
    NS_IMETHOD         SetCursor(nsCursor aCursor);
    NS_IMETHOD         SetCursor(imgIContainer* aCursor,
                                 uint32_t aHotspotX, uint32_t aHotspotY);
    NS_IMETHOD         Invalidate(const nsIntRect &aRect);
    virtual void*      GetNativeData(uint32_t aDataType);
    NS_IMETHOD         SetTitle(const nsAString& aTitle);
    NS_IMETHOD         SetIcon(const nsAString& aIconSpec);
    NS_IMETHOD         SetWindowClass(const nsAString& xulWinType);
    virtual nsIntPoint WidgetToScreenOffset();
    NS_IMETHOD         EnableDragDrop(bool aEnable);
    NS_IMETHOD         CaptureMouse(bool aCapture);
    NS_IMETHOD         CaptureRollupEvents(nsIRollupListener *aListener,
                                           bool aDoCapture);
    NS_IMETHOD         GetAttention(int32_t aCycleCount);

    virtual bool       HasPendingInputEvent();

    NS_IMETHOD         MakeFullScreen(bool aFullScreen);
    NS_IMETHOD         HideWindowChrome(bool aShouldHide);

    /**
     * GetLastUserInputTime returns a timestamp for the most recent user input
     * event.  This is intended for pointer grab requests (including drags).
     */
    static guint32     GetLastUserInputTime();

    // utility method, -1 if no change should be made, otherwise returns a
    // value that can be passed to gdk_window_set_decorations
    gint               ConvertBorderStyles(nsBorderStyle aStyle);

    // event callbacks
#if (MOZ_WIDGET_GTK == 2)
    gboolean           OnExposeEvent(GdkEventExpose *aEvent);
#else
    gboolean           OnExposeEvent(cairo_t *cr);
#endif
    gboolean           OnConfigureEvent(GtkWidget *aWidget,
                                        GdkEventConfigure *aEvent);
    void               OnContainerUnrealize();
    void               OnSizeAllocate(GtkAllocation *aAllocation);
    void               OnDeleteEvent();
    void               OnEnterNotifyEvent(GdkEventCrossing *aEvent);
    void               OnLeaveNotifyEvent(GdkEventCrossing *aEvent);
    void               OnMotionNotifyEvent(GdkEventMotion *aEvent);
    void               OnButtonPressEvent(GdkEventButton *aEvent);
    void               OnButtonReleaseEvent(GdkEventButton *aEvent);
    void               OnContainerFocusInEvent(GdkEventFocus *aEvent);
    void               OnContainerFocusOutEvent(GdkEventFocus *aEvent);
    gboolean           OnKeyPressEvent(GdkEventKey *aEvent);
    gboolean           OnKeyReleaseEvent(GdkEventKey *aEvent);
    void               OnScrollEvent(GdkEventScroll *aEvent);
    void               OnVisibilityNotifyEvent(GdkEventVisibility *aEvent);
    void               OnWindowStateEvent(GtkWidget *aWidget,
                                          GdkEventWindowState *aEvent);
    void               OnDragDataReceivedEvent(GtkWidget       *aWidget,
                                               GdkDragContext  *aDragContext,
                                               gint             aX,
                                               gint             aY,
                                               GtkSelectionData*aSelectionData,
                                               guint            aInfo,
                                               guint            aTime,
                                               gpointer         aData);

  mozilla::TemporaryRef<mozilla::gfx::DrawTarget> StartRemoteDrawing() MOZ_OVERRIDE;

private:
    void               UpdateAlpha(gfxPattern* aPattern, nsIntRect aBoundsRect);

    void               NativeResize(int32_t aWidth,
                                    int32_t aHeight,
                                    bool    aRepaint);

    void               NativeResize(int32_t aX,
                                    int32_t aY,
                                    int32_t aWidth,
                                    int32_t aHeight,
                                    bool    aRepaint);

    void               NativeShow  (bool    aAction);
    void               SetHasMappedToplevel(bool aState);
    nsIntSize          GetSafeWindowSize(nsIntSize aSize);

    void               EnsureGrabs  (void);
    void               GrabPointer  (guint32 aTime);
    void               ReleaseGrabs (void);

public:
    enum PluginType {
        PluginType_NONE = 0,   /* do not have any plugin */
        PluginType_XEMBED,     /* the plugin support xembed */
        PluginType_NONXEMBED   /* the plugin does not support xembed */
    };

    void               SetPluginType(PluginType aPluginType);
#ifdef MOZ_X11
    void               SetNonXEmbedPluginFocus(void);
    void               LoseNonXEmbedPluginFocus(void);
#endif /* MOZ_X11 */

    void               ThemeChanged(void);

#ifdef MOZ_X11
    Window             mOldFocusWindow;
#endif /* MOZ_X11 */

    static guint32     sLastButtonPressTime;

    NS_IMETHOD         BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                       int32_t aHorizontal,
                                       int32_t aVertical);
    NS_IMETHOD         BeginMoveDrag(mozilla::WidgetMouseEvent* aEvent);

    MozContainer*      GetMozContainer() { return mContainer; }
    // GetMozContainerWidget returns the MozContainer even for undestroyed
    // descendant windows
    GtkWidget*         GetMozContainerWidget();
    GdkWindow*         GetGdkWindow() { return mGdkWindow; }
    bool               IsDestroyed() { return mIsDestroyed; }

    void               DispatchDragEvent(uint32_t aMsg,
                                         const nsIntPoint& aRefPoint,
                                         guint aTime);
    static void        UpdateDragStatus (GdkDragContext *aDragContext,
                                         nsIDragService *aDragService);
    // If this dispatched the keydown event actually, this returns TRUE,
    // otherwise, FALSE.
    bool               DispatchKeyDownEvent(GdkEventKey *aEvent,
                                            bool *aIsCancelled);

    NS_IMETHOD NotifyIME(const IMENotification& aIMENotification) MOZ_OVERRIDE;
    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();
    NS_IMETHOD_(bool) ExecuteNativeKeyBinding(
                        NativeKeyBindingsType aType,
                        const mozilla::WidgetKeyboardEvent& aEvent,
                        DoCommandCallback aCallback,
                        void* aCallbackData) MOZ_OVERRIDE;
    NS_IMETHOD GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState);

    // These methods are for toplevel windows only.
    void               ResizeTransparencyBitmap();
    void               ApplyTransparencyBitmap();
    void               ClearTransparencyBitmap();

   virtual void        SetTransparencyMode(nsTransparencyMode aMode);
   virtual nsTransparencyMode GetTransparencyMode();
   virtual nsresult    ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
   nsresult            UpdateTranslucentWindowAlphaInternal(const nsIntRect& aRect,
                                                            uint8_t* aAlphas, int32_t aStride);
    virtual gfxASurface *GetThebesSurface();

#if (MOZ_WIDGET_GTK == 2)
    static already_AddRefed<gfxASurface> GetSurfaceForGdkDrawable(GdkDrawable* aDrawable,
                                                                  const nsIntSize& aSize);
#endif
    NS_IMETHOD         ReparentNativeWidget(nsIWidget* aNewParent);

    virtual nsresult SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                                uint32_t aNativeMessage,
                                                uint32_t aModifierFlags);

    virtual nsresult SynthesizeNativeMouseMove(nsIntPoint aPoint)
    { return SynthesizeNativeMouseEvent(aPoint, GDK_MOTION_NOTIFY, 0); }

protected:
    // event handling code
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchResized(int32_t aWidth, int32_t aHeight);

    // Helper for SetParent and ReparentNativeWidget.
    void ReparentNativeWidgetInternal(nsIWidget* aNewParent,
                                      GtkWidget* aNewContainer,
                                      GdkWindow* aNewParentWindow,
                                      GtkWidget* aOldContainer);
    nsCOMPtr<nsIWidget> mParent;
    // Is this a toplevel window?
    bool                mIsTopLevel;
    // Has this widget been destroyed yet?
    bool                mIsDestroyed;

    // This is a flag that tracks if we need to resize a widget or
    // window when we show it.
    bool                mNeedsResize;
    // This is a flag that tracks if we need to move a widget or
    // window when we show it.
    bool                mNeedsMove;
    // Should we send resize events on all resizes?
    bool                mListenForResizes;
    // This flag tracks if we're hidden or shown.
    bool                mIsShown;
    bool                mNeedsShow;
    // is this widget enabled?
    bool                mEnabled;
    // has the native window for this been created yet?
    bool                mCreated;

private:
    void               DestroyChildWindows();
    GtkWidget         *GetToplevelWidget();
    nsWindow          *GetContainerWindow();
    void               SetUrgencyHint(GtkWidget *top_window, bool state);
    void              *SetupPluginPort(void);
    void               SetDefaultIcon(void);
    void               InitButtonEvent(mozilla::WidgetMouseEvent& aEvent,
                                       GdkEventButton* aGdkEvent);
    bool               DispatchCommandEvent(nsIAtom* aCommand);
    bool               DispatchContentCommandEvent(int32_t aMsg);
    void               SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                                           bool aIntersectWithExisting);
    bool               CheckForRollup(gdouble aMouseX, gdouble aMouseY,
                                      bool aIsWheel, bool aAlwaysRollup);
    bool               GetDragInfo(mozilla::WidgetMouseEvent* aMouseEvent,
                                   GdkWindow** aWindow, gint* aButton,
                                   gint* aRootX, gint* aRootY);
    void               ClearCachedResources();

    GtkWidget          *mShell;
    MozContainer       *mContainer;
    GdkWindow          *mGdkWindow;

    uint32_t            mHasMappedToplevel : 1,
                        mIsFullyObscured : 1,
                        mRetryPointerGrab : 1;
    nsSizeMode          mSizeState;
    PluginType          mPluginType;

    int32_t             mTransparencyBitmapWidth;
    int32_t             mTransparencyBitmapHeight;

#ifdef MOZ_HAVE_SHMIMAGE
    // If we're using xshm rendering, mThebesSurface wraps mShmImage
    nsRefPtr<nsShmImage>  mShmImage;
#endif
    nsRefPtr<gfxASurface> mThebesSurface;

#ifdef ACCESSIBILITY
    nsRefPtr<mozilla::a11y::Accessible> mRootAccessible;

    /**
     * Request to create the accessible for this window if it is top level.
     */
    void                CreateRootAccessible();

    /**
     * Dispatch accessible event for the top level window accessible.
     *
     * @param  aEventType  [in] the accessible event type to dispatch
     */
    void                DispatchEventToRootAccessible(uint32_t aEventType);

    /**
     * Dispatch accessible window activate event for the top level window
     * accessible.
     */
    void                DispatchActivateEventAccessible();

    /**
     * Dispatch accessible window deactivate event for the top level window
     * accessible.
     */
    void                DispatchDeactivateEventAccessible();

    /**
     * Dispatch accessible window maximize event for the top level window
     * accessible.
     */
    void                DispatchMaximizeEventAccessible();

    /**
     * Dispatch accessible window minize event for the top level window
     * accessible.
     */
    void                DispatchMinimizeEventAccessible();

    /**
     * Dispatch accessible window restore event for the top level window
     * accessible.
     */
    void                DispatchRestoreEventAccessible();
#endif

    // The cursor cache
    static GdkCursor   *gsGtkCursorCache[eCursorCount];

    // Transparency
    bool         mIsTransparent;
    // This bitmap tracks which pixels are transparent. We don't support
    // full translucency at this time; each pixel is either fully opaque
    // or fully transparent.
    gchar*       mTransparencyBitmap;
 
    // all of our DND stuff
    void   InitDragEvent(mozilla::WidgetDragEvent& aEvent);

    float              mLastMotionPressure;

    // Remember the last sizemode so that we can restore it when
    // leaving fullscreen
    nsSizeMode         mLastSizeMode;

    static bool DragInProgress(void);

    void DispatchMissedButtonReleases(GdkEventCrossing *aGdkEvent);

    // nsBaseWidget
    virtual LayerManager* GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                          LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nullptr) MOZ_OVERRIDE;
#if (MOZ_WIDGET_GTK == 3)
    gfxASurface* GetThebesSurface(cairo_t *cr);
#endif

    void CleanLayerManagerRecursive();

    /**
     * |mIMModule| takes all IME related stuff.
     *
     * This is owned by the top-level nsWindow or the topmost child
     * nsWindow embedded in a non-Gecko widget.
     *
     * The instance is created when the top level widget is created.  And when
     * the widget is destroyed, it's released.  All child windows refer its
     * ancestor widget's instance.  So, one set of IM contexts is created for
     * all windows in a hierarchy.  If the children are released after the top
     * level window is released, the children still have a valid pointer,
     * however, IME doesn't work at that time.
     */
    nsRefPtr<nsGtkIMModule> mIMModule;
};

class nsChildWindow : public nsWindow {
public:
    nsChildWindow();
    ~nsChildWindow();
};

#endif /* __nsWindow_h__ */
