/* vim:set ts=2 sw=2 sts=2 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIconChannel.h"

#include <stdlib.h>
#include <unistd.h>

#include "gdk/gdk.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/ipc/ByteBuf.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/GRefPtr.h"
#include <algorithm>

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <gtk/gtk.h>

#include "nsMimeTypes.h"
#include "nsIMIMEService.h"

#include "nsServiceManagerUtils.h"

#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsIStringStream.h"
#include "nsServiceManagerUtils.h"
#include "nsIURL.h"
#include "nsIPipe.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "prlink.h"
#include "gfxPlatform.h"

using mozilla::CheckedInt32;
using mozilla::ipc::ByteBuf;

NS_IMPL_ISUPPORTS(nsIconChannel, nsIRequest, nsIChannel)

static nsresult MozGdkPixbufToByteBuf(GdkPixbuf* aPixbuf, gint aScale,
                                      ByteBuf* aByteBuf) {
  int width = gdk_pixbuf_get_width(aPixbuf);
  int height = gdk_pixbuf_get_height(aPixbuf);
  NS_ENSURE_TRUE(height < 256 && width < 256 && height > 0 && width > 0 &&
                     gdk_pixbuf_get_colorspace(aPixbuf) == GDK_COLORSPACE_RGB &&
                     gdk_pixbuf_get_bits_per_sample(aPixbuf) == 8 &&
                     gdk_pixbuf_get_has_alpha(aPixbuf) &&
                     gdk_pixbuf_get_n_channels(aPixbuf) == 4,
                 NS_ERROR_UNEXPECTED);

  const int n_channels = 4;
  CheckedInt32 buf_size =
      4 + n_channels * CheckedInt32(height) * CheckedInt32(width);
  if (!buf_size.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  uint8_t* const buf = (uint8_t*)moz_xmalloc(buf_size.value());
  uint8_t* out = buf;

  *(out++) = width;
  *(out++) = height;
  *(out++) = uint8_t(mozilla::gfx::SurfaceFormat::OS_RGBA);

  // Set all bits to ensure in nsIconDecoder we color manage and premultiply.
  *(out++) = 0xFF;

  const guchar* const pixels = gdk_pixbuf_get_pixels(aPixbuf);
  int instride = gdk_pixbuf_get_rowstride(aPixbuf);
  int outstride = width * n_channels;

  // encode the RGB data and the A data and adjust the stride as necessary.
  mozilla::gfx::SwizzleData(pixels, instride,
                            mozilla::gfx::SurfaceFormat::R8G8B8A8, out,
                            outstride, mozilla::gfx::SurfaceFormat::OS_RGBA,
                            mozilla::gfx::IntSize(width, height));

  *aByteBuf = ByteBuf(buf, buf_size.value(), buf_size.value());
  return NS_OK;
}

static nsresult ByteBufToStream(ByteBuf&& aBuf, nsIInputStream** aStream) {
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream =
      do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // stream takes ownership of buf and will free it on destruction.
  // This function cannot fail.
  rv = stream->AdoptData(reinterpret_cast<char*>(aBuf.mData), aBuf.mLen);
  MOZ_ASSERT(CheckedInt32(aBuf.mLen).isValid(),
             "aBuf.mLen should fit in int32_t");
  aBuf.mData = nullptr;

  // If this no longer holds then re-examine buf's lifetime.
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  NS_ENSURE_SUCCESS(rv, rv);

  stream.forget(aStream);
  return NS_OK;
}

static GdkRGBA GetForegroundColor(nsIMozIconURI* aIconURI) {
  auto scheme = [&] {
    bool dark = false;
    if (NS_FAILED(aIconURI->GetImageDark(&dark))) {
      return mozilla::LookAndFeel::SystemColorScheme();
    }
    return dark ? mozilla::ColorScheme::Dark : mozilla::ColorScheme::Light;
  }();
  auto color = mozilla::LookAndFeel::Color(
      mozilla::LookAndFeel::ColorID::Windowtext, scheme,
      mozilla::LookAndFeel::UseStandins::No);
  auto ToGdk = [](uint8_t aGecko) { return aGecko / 255.0; };
  return GdkRGBA{
      .red = ToGdk(NS_GET_R(color)),
      .green = ToGdk(NS_GET_G(color)),
      .blue = ToGdk(NS_GET_B(color)),
      .alpha = ToGdk(NS_GET_A(color)),
  };
}

/* static */
nsresult nsIconChannel::GetIconWithGIO(nsIMozIconURI* aIconURI,
                                       ByteBuf* aDataOut) {
  RefPtr<GIcon> icon;
  nsCOMPtr<nsIURL> fileURI;

  // Read icon content
  aIconURI->GetIconURL(getter_AddRefs(fileURI));

  // Get icon for file specified by URI
  if (fileURI) {
    nsAutoCString spec;
    fileURI->GetAsciiSpec(spec);
    if (fileURI->SchemeIs("file")) {
      GFile* file = g_file_new_for_uri(spec.get());
      GFileInfo* fileInfo =
          g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON,
                            G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
      g_object_unref(file);
      if (fileInfo) {
        icon = g_file_info_get_icon(fileInfo);
        g_object_unref(fileInfo);
      }
    }
  } else {
    // From the moz-icon://appId?size=... get the appId
    nsAutoCString appId;
    aIconURI->GetAsciiSpec(appId);

    if (appId.Find("?size=")) {
      appId.Truncate(appId.Find("?size="));
    }

    appId = Substring(appId, sizeof("moz-icon:/"));

    RefPtr<GDesktopAppInfo> app_info =
        dont_AddRef(g_desktop_app_info_new(appId.get()));
    if (app_info) {
      icon = g_app_info_get_icon((GAppInfo*)app_info.get());
    }
  }

  // Try to get icon by using MIME type
  if (!icon) {
    nsAutoCString type;
    aIconURI->GetContentType(type);
    // Try to get MIME type from file extension by using nsIMIMEService
    if (type.IsEmpty()) {
      nsCOMPtr<nsIMIMEService> ms(do_GetService("@mozilla.org/mime;1"));
      if (ms) {
        nsAutoCString fileExt;
        aIconURI->GetFileExtension(fileExt);
        ms->GetTypeFromExtension(fileExt, type);
      }
    }
    mozilla::GUniquePtr<gchar> ctype;
    if (!type.IsEmpty()) {
      ctype.reset(g_content_type_from_mime_type(type.get()));
    }
    if (ctype) {
      icon = dont_AddRef(g_content_type_get_icon(ctype.get()));
    }
  }

  // Get default icon theme
  GtkIconTheme* iconTheme = gtk_icon_theme_get_default();
  // Get icon size and scale.
  int32_t iconSize = aIconURI->GetImageSize();
  int32_t scale = aIconURI->GetImageScale();

  RefPtr<GtkIconInfo> iconInfo;
  if (icon) {
    iconInfo = dont_AddRef(gtk_icon_theme_lookup_by_gicon_for_scale(
        iconTheme, icon, iconSize, scale, GtkIconLookupFlags(0)));
  }

  if (!iconInfo) {
    // Mozilla's mimetype lookup failed. Try the "unknown" icon.
    iconInfo = dont_AddRef(gtk_icon_theme_lookup_icon_for_scale(
        iconTheme, "unknown", iconSize, scale, GtkIconLookupFlags(0)));
    if (!iconInfo) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  // Create a GdkPixbuf buffer containing icon and scale it
  const auto fg = GetForegroundColor(aIconURI);
  RefPtr<GdkPixbuf> pixbuf = dont_AddRef(gtk_icon_info_load_symbolic(
      iconInfo, &fg, nullptr, nullptr, nullptr, nullptr, nullptr));

  if (!pixbuf) {
    return NS_ERROR_UNEXPECTED;
  }
  return MozGdkPixbufToByteBuf(pixbuf, scale, aDataOut);
}

/* static */
nsresult nsIconChannel::GetIcon(nsIURI* aURI, ByteBuf* aDataOut) {
  nsCOMPtr<nsIMozIconURI> iconURI = do_QueryInterface(aURI);
  NS_ASSERTION(iconURI, "URI is not an nsIMozIconURI");

  if (!iconURI) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (gfxPlatform::IsHeadless()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString stockIcon;
  iconURI->GetStockIcon(stockIcon);
  if (stockIcon.IsEmpty()) {
    return GetIconWithGIO(iconURI, aDataOut);
  }

  GtkIconTheme* theme = gtk_icon_theme_get_default();
  const gint iconSize = iconURI->GetImageSize();
  const gint scale = iconURI->GetImageScale();
  RefPtr<GtkIconInfo> iconInfo =
      dont_AddRef(gtk_icon_theme_lookup_icon_for_scale(
          theme, stockIcon.get(), iconSize, scale, GtkIconLookupFlags(0)));
  if (!iconInfo) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  const auto fg = GetForegroundColor(iconURI);
  RefPtr<GdkPixbuf> pixbuf = dont_AddRef(gtk_icon_info_load_symbolic(
      iconInfo, &fg, nullptr, nullptr, nullptr, nullptr, nullptr));

  // According to documentation, gtk_icon_set_render_icon() never returns
  // nullptr, but it does return nullptr when we have the problem reported
  // here: https://bugzilla.gnome.org/show_bug.cgi?id=629878#c13
  if (!pixbuf) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return MozGdkPixbufToByteBuf(pixbuf, scale, aDataOut);
}

nsresult nsIconChannel::Init(nsIURI* aURI, nsILoadInfo* aLoadInfo) {
  nsCOMPtr<nsIInputStream> stream;

  using ContentChild = mozilla::dom::ContentChild;
  if (auto* contentChild = ContentChild::GetSingleton()) {
    // Get the icon via IPC and translate the promise of a ByteBuf
    // into an actually-existing channel.
    RefPtr<ContentChild::GetSystemIconPromise> icon =
        contentChild->SendGetSystemIcon(aURI);
    if (!icon) {
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIAsyncInputStream> inputStream;
    nsCOMPtr<nsIAsyncOutputStream> outputStream;
    NS_NewPipe2(getter_AddRefs(inputStream), getter_AddRefs(outputStream), true,
                false, 0, UINT32_MAX);

    // FIXME: Bug 1718324
    // The GetSystemIcon() call will end up on the parent doing GetIcon()
    // and by using ByteBuf we might not be immune to some deadlock, at least
    // on paper. From analysis in
    // https://phabricator.services.mozilla.com/D118596#3865440 we should be
    // safe in practice, but it would be nicer to just write that differently.

    icon->Then(
        mozilla::GetCurrentSerialEventTarget(), __func__,
        [outputStream](std::tuple<nsresult, mozilla::Maybe<ByteBuf>>&& aArg) {
          nsresult rv = std::get<0>(aArg);
          mozilla::Maybe<ByteBuf> bytes = std::move(std::get<1>(aArg));

          if (NS_SUCCEEDED(rv)) {
            MOZ_RELEASE_ASSERT(bytes);
            uint32_t written;
            rv = outputStream->Write(reinterpret_cast<char*>(bytes->mData),
                                     static_cast<uint32_t>(bytes->mLen),
                                     &written);
            if (NS_SUCCEEDED(rv)) {
              const bool wroteAll = static_cast<size_t>(written) == bytes->mLen;
              MOZ_ASSERT(wroteAll);
              if (!wroteAll) {
                rv = NS_ERROR_UNEXPECTED;
              }
            }
          } else {
            MOZ_ASSERT(!bytes);
          }

          if (NS_FAILED(rv)) {
            outputStream->CloseWithStatus(rv);
          }
        },
        [outputStream](mozilla::ipc::ResponseRejectReason) {
          outputStream->CloseWithStatus(NS_ERROR_FAILURE);
        });

    stream = inputStream.forget();
  } else {
    // Get the icon directly.
    ByteBuf bytebuf;
    nsresult rv = GetIcon(aURI, &bytebuf);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ByteBufToStream(std::move(bytebuf), getter_AddRefs(stream));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_NewInputStreamChannelInternal(
      getter_AddRefs(mRealChannel), aURI, stream.forget(),
      nsLiteralCString(IMAGE_ICON_MS), /* aContentCharset */ ""_ns, aLoadInfo);
}

void nsIconChannel::Shutdown() {}
