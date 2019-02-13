/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWINDOW_H_
#define NSWINDOW_H_

#include "nsBaseWidget.h"
#include "gfxPoint.h"

#include "nsTArray.h"

@class UIWindow;
@class UIView;
@class ChildView;
class gfxASurface;

class nsWindow :
    public nsBaseWidget
{
    typedef nsBaseWidget Inherited;

public:
    nsWindow();

    NS_DECL_ISUPPORTS_INHERITED

    //
    // nsIWidget
    //

    NS_IMETHOD Create(nsIWidget *aParent,
                      nsNativeWidget aNativeParent,
                      const nsIntRect &aRect,
                      nsWidgetInitData *aInitData = nullptr) override;
    NS_IMETHOD Destroy() override;
    NS_IMETHOD Show(bool aState) override;
    NS_IMETHOD              Enable(bool aState) override {
        return NS_OK;
    }
    virtual bool            IsEnabled() const override {
        return true;
    }
    NS_IMETHOD              SetModal(bool aState) override;
    virtual bool            IsVisible() const override {
        return mVisible;
    }
    NS_IMETHOD              SetFocus(bool aState=false) override;
    virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset() override;

    virtual void SetBackgroundColor(const nscolor &aColor) override;
    virtual void* GetNativeData(uint32_t aDataType) override;

    NS_IMETHOD              ConstrainPosition(bool aAllowSlop,
                                              int32_t *aX, int32_t *aY) override;
    NS_IMETHOD              Move(double aX, double aY) override;
    NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                        nsIWidget *aWidget, bool aActivate) override;
    NS_IMETHOD              SetSizeMode(int32_t aMode) override;
    void                    EnteredFullScreen(bool aFullScreen);
    NS_IMETHOD              Resize(double aWidth, double aHeight, bool aRepaint) override;
    NS_IMETHOD              Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint) override;
    NS_IMETHOD              GetScreenBounds(nsIntRect &aRect) override;
    void                    ReportMoveEvent();
    void                    ReportSizeEvent();
    void                    ReportSizeModeEvent(int32_t aMode);

    CGFloat                 BackingScaleFactor();
    void                    BackingScaleFactorChanged();
    virtual float           GetDPI() override {
        //XXX: terrible
        return 326.0f;
    }
    virtual double          GetDefaultScaleInternal() override {
        return BackingScaleFactor();
    }
    virtual int32_t         RoundsWidgetCoordinatesTo() override;

    NS_IMETHOD              SetTitle(const nsAString& aTitle) override {
        return NS_OK;
    }

    NS_IMETHOD Invalidate(const nsIntRect &aRect) override;
    virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override;
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                             nsEventStatus& aStatus) override;
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener * aListener,
                                   bool aDoCapture) override {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    void WillPaintWindow();
    bool PaintWindow(nsIntRegion aRegion);

    bool HasModalDescendents() { return false; }

    //NS_IMETHOD NotifyIME(const IMENotification& aIMENotification) override;
    NS_IMETHOD_(void) SetInputContext(
                        const InputContext& aContext,
                        const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();
    /*
    NS_IMETHOD_(bool) ExecuteNativeKeyBinding(
                        NativeKeyBindingsType aType,
                        const mozilla::WidgetKeyboardEvent& aEvent,
                        DoCommandCallback aCallback,
                        void* aCallbackData) override;
    */

    NS_IMETHOD         ReparentNativeWidget(nsIWidget* aNewParent) override;

protected:
    virtual ~nsWindow();
    void BringToFront();
    nsWindow *FindTopLevel();
    bool IsTopLevel();
    nsresult GetCurrentOffset(uint32_t &aOffset, uint32_t &aLength);
    nsresult DeleteRange(int aOffset, int aLen);

    void TearDownView();

    ChildView*   mNativeView;
    bool mVisible;
    nsTArray<nsWindow*> mChildren;
    nsWindow* mParent;
    InputContext         mInputContext;

    void OnSizeChanged(const gfxIntSize& aSize);

    static void DumpWindows();
    static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
    static void LogWindow(nsWindow *win, int index, int indent);
};

#endif /* NSWINDOW_H_ */
