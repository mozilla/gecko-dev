/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"

#include <gtk/gtk.h>

#include "nsApplicationChooser.h"
#include "WidgetUtils.h"
#include "nsIMIMEInfo.h"
#include "nsIWidget.h"
#include "nsIFile.h"
#include "nsCExternalHandlerService.h"
#include "nsComponentManagerUtils.h"
#include "nsGtkUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIGIOService.h"
#include <gio/gdesktopappinfo.h>
#include "mozilla/GRefPtr.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsApplicationChooser, nsIApplicationChooser)

nsApplicationChooser::nsApplicationChooser() = default;

nsApplicationChooser::~nsApplicationChooser() = default;

NS_IMETHODIMP
nsApplicationChooser::Init(mozIDOMWindowProxy* aParent,
                           const nsACString& aTitle) {
  NS_ENSURE_TRUE(aParent, NS_ERROR_FAILURE);
  auto* parent = nsPIDOMWindowOuter::From(aParent);
  mParentWidget = widget::WidgetUtils::DOMWindowToWidget(parent);
  mWindowTitle.Assign(aTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationChooser::Open(const nsACString& aContentType,
                           nsIApplicationChooserFinishedCallback* aCallback) {
  MOZ_ASSERT(aCallback);
  if (mCallback) {
    NS_WARNING("Chooser is already in progress.");
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mCallback = aCallback;
  NS_ENSURE_TRUE(mParentWidget, NS_ERROR_FAILURE);
  GtkWindow* parent_widget =
      GTK_WINDOW(mParentWidget->GetNativeData(NS_NATIVE_SHELLWIDGET));

  GtkWidget* chooser = gtk_app_chooser_dialog_new_for_content_type(
      parent_widget,
      (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
      PromiseFlatCString(aContentType).get());
  gtk_app_chooser_dialog_set_heading(GTK_APP_CHOOSER_DIALOG(chooser),
                                     mWindowTitle.BeginReading());
  NS_ADDREF_THIS();
  g_signal_connect(chooser, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(chooser, "destroy", G_CALLBACK(OnDestroy), this);
  gtk_widget_show(chooser);
  return NS_OK;
}

/* static */
void nsApplicationChooser::OnResponse(GtkWidget* chooser, gint response_id,
                                      gpointer user_data) {
  static_cast<nsApplicationChooser*>(user_data)->Done(chooser, response_id);
}

/* static */
void nsApplicationChooser::OnDestroy(GtkWidget* chooser, gpointer user_data) {
  static_cast<nsApplicationChooser*>(user_data)->Done(chooser,
                                                      GTK_RESPONSE_CANCEL);
}

void nsApplicationChooser::Done(GtkWidget* chooser, gint response) {
  nsCOMPtr<nsIGIOHandlerApp> gioHandler;
  switch (response) {
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_ACCEPT: {
      RefPtr<GAppInfo> app_info =
          gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(chooser));
      nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
      if (app_info) {
        giovfs->CreateHandlerAppFromAppId(g_app_info_get_id(app_info),
                                          getter_AddRefs(gioHandler));
      } else {
        NS_WARNING(
            "Application chooser dialog accepted but no appinfo received.");
      }
      break;
    }
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_DELETE_EVENT:
      break;
    default:
      NS_WARNING("Unexpected response");
      break;
  }

  // A "response" signal won't be sent again but "destroy" will be.
  g_signal_handlers_disconnect_by_func(chooser, FuncToGpointer(OnDestroy),
                                       this);
  gtk_widget_destroy(chooser);

  if (mCallback) {
    mCallback->Done(gioHandler);
    mCallback = nullptr;
  }
  NS_RELEASE_THIS();
}
