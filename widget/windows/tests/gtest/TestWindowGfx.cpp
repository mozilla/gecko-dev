/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "Image.h"
#include "ImageFactory.h"
#include "imgITools.h"
#include "mozilla/Base64.h"
#include "mozilla/Encoding.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/Preferences.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/SystemPrincipal.h"
#include "mozilla/UniquePtr.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsILoadInfo.h"
#include "nsISVGPaintContext.h"
#include "nsMimeTypes.h"
#include "nsStreamUtils.h"
#include "nsWindowGfx.h"
#include "SystemPrincipal.h"

#include "gtest/gtest.h"

using namespace mozilla;
using namespace mozilla::image;

const char* SVG_GREEN_CIRCLE =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \
<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\" version=\"1.1\"> \
  <circle fill=\"#00FF00\" stroke=\"#FF0000\" stroke-width=\"20\" cx=\"50\" cy=\"50\" r=\"40\" /> \
</svg> \
";

const char* SVG_UNSIZED_CIRCLE =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \
<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\" version=\"1.1\"> \
  <circle fill=\"#00FF00\" stroke=\"#FF0000\" stroke-width=\"20\" cx=\"50\" cy=\"50\" r=\"40\" /> \
</svg> \
";

const char* SVG_CONTEXT_CIRCLE =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \
<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\" version=\"1.1\"> \
  <circle fill=\"context-fill\" stroke=\"context-stroke\" stroke-width=\"20\" cx=\"50\" cy=\"50\" r=\"40\" /> \
</svg> \
";

// Circle's radius is 40 but the total radius includes half the stroke width.
#define CIRCLE_TOTAL_AREA (M_PI * 50 * 50)
// The fill area's radius is the circle's radius minus half the stroke width.
#define CIRCLE_FILL_AREA (M_PI * 30 * 30)
#define CIRCLE_STROKE_AREA (CIRCLE_TOTAL_AREA - CIRCLE_FILL_AREA)

// Allow 2% margin of error to allow for blending
#define ASSERT_NEARLY(val1, val2)   \
  {                                 \
    ASSERT_GT(val1, (val2) * 0.98); \
    ASSERT_LT(val1, (val2) * 1.02); \
  }

class SvgPaintContext : public nsISVGPaintContext {
 public:
  NS_DECL_ISUPPORTS

  nsCString mStrokeColor;
  nsCString mFillColor;

  SvgPaintContext(const char* aStroke, const char* aFill)
      : mStrokeColor(aStroke), mFillColor(aFill) {}

  NS_IMETHODIMP GetStrokeColor(nsACString& color) override {
    color = mStrokeColor;
    return NS_OK;
  }

  NS_IMETHODIMP GetStrokeOpacity(float* opacity) override {
    *opacity = 1.0;
    return NS_OK;
  }

  NS_IMETHODIMP GetFillColor(nsACString& color) override {
    color = mFillColor;
    return NS_OK;
  }

  NS_IMETHODIMP GetFillOpacity(float* opacity) override {
    *opacity = 1.0;
    return NS_OK;
  }

 private:
  virtual ~SvgPaintContext() {};
};

NS_IMPL_ISUPPORTS(SvgPaintContext, nsISVGPaintContext);

class ImageLoadListener : public IProgressObserver {
 public:
  NS_INLINE_DECL_REFCOUNTING(ImageLoadListener, override)

  virtual void OnLoadComplete(bool aLastPart) override { mIsLoaded = true; }

  // Other notifications are ignored.
  virtual void Notify(int32_t aType,
                      const nsIntRect* aRect = nullptr) override {}
  virtual void SetHasImage() override {}
  virtual bool NotificationsDeferred() const override { return false; }
  virtual void MarkPendingNotify() override {}
  virtual void ClearPendingNotify() override {}

  boolean mIsLoaded{};

 private:
  virtual ~ImageLoadListener() {};
};

void LoadImage(const char* aData, imgIContainer** aImage) {
  nsCString svgUri;
  nsresult rv = Base64Encode(aData, strlen(aData), svgUri);
  ASSERT_NS_SUCCEEDED(rv);
  svgUri.Insert("data:" IMAGE_SVG_XML ";base64,", 0);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), svgUri, UTF_8_ENCODING, nullptr);
  ASSERT_NS_SUCCEEDED(rv);

  nsCOMPtr<nsIPrincipal> principal = SystemPrincipal::Get();
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri, principal,
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                     nsContentPolicyType::TYPE_IMAGE);

  RefPtr<ImageLoadListener> listener = new ImageLoadListener();
  RefPtr<ProgressTracker> tracker = new ProgressTracker();
  tracker->AddObserver(listener);
  RefPtr<Image> image = ImageFactory::CreateImage(
      channel, tracker, nsCString(IMAGE_SVG_XML), uri, false, 0);
  ASSERT_FALSE(image->HasError());

  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  ASSERT_NS_SUCCEEDED(rv);

  uint64_t size;
  rv = stream->Available(&size);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(size, strlen(aData));

  rv = image->OnImageDataAvailable(channel, stream, 0, size);
  ASSERT_NS_SUCCEEDED(rv);

  // Let the Image know we've sent all the data.
  rv = image->OnImageDataComplete(channel, NS_OK, true);
  ASSERT_NS_SUCCEEDED(rv);

  // The final load event from the SVG document is dispatched asynchronously so
  // wait for that to happen.
  MOZ_ALWAYS_TRUE(
      SpinEventLoopUntil("windows:widget:TEST(TestWindowGfx, CreateIcon)"_ns,
                         [&listener]() { return listener->mIsLoaded; }));

  image.forget(aImage);
}

void ConvertToRaster(imgIContainer* vectorImage, imgIContainer** aImage) {
  // First we encode it as a png image.
  nsCOMPtr<imgITools> imgTools =
      do_CreateInstance("@mozilla.org/image/tools;1");

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = imgTools->EncodeImage(vectorImage, "image/png"_ns, u""_ns,
                                      getter_AddRefs(stream));
  ASSERT_NS_SUCCEEDED(rv);

  uint64_t size;
  rv = stream->Available(&size);

  // And then we load the image again as a raster imgIContainer
  RefPtr<image::Image> image =
      ImageFactory::CreateAnonymousImage("image/png"_ns, size);
  RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
  ASSERT_FALSE(image->HasError());

  rv = image->OnImageDataAvailable(nullptr, stream, 0, size);
  ASSERT_NS_SUCCEEDED(rv);

  // Let the Image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, NS_OK, true);
  tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);
  ASSERT_NS_SUCCEEDED(rv);

  image.forget(aImage);
}

void CountPixels(ICONINFO& ii, BITMAP& bm, double* redCount, double* greenCount,
                 double* blueCount) {
  BITMAPINFOHEADER bi;
  bi.biSize = sizeof(BITMAPINFOHEADER);
  bi.biWidth = bm.bmWidth;
  bi.biHeight = bm.bmHeight;
  bi.biPlanes = 1;
  bi.biBitCount = 32;
  bi.biCompression = BI_RGB;
  bi.biSizeImage = 0;
  bi.biXPelsPerMeter = 0;
  bi.biYPelsPerMeter = 0;
  bi.biClrUsed = 0;
  bi.biClrImportant = 0;

  auto stride = GDI_WIDTHBYTES(bm.bmWidth * 32);
  auto dataLength = stride * bm.bmHeight;

  UniquePtr<uint8_t[]> bitmapData(new uint8_t[dataLength]);

  *redCount = 0;
  *greenCount = 0;
  *blueCount = 0;

  int lines =
      GetDIBits(::GetDC(nullptr), ii.hbmColor, 0, (UINT)bm.bmHeight,
                (void*)bitmapData.get(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
  if (lines != bm.bmHeight) {
    return;
  }

  for (long y = 0; y < bm.bmHeight; y++) {
    auto index = stride * y;

    for (long x = 0; x < bm.bmWidth; x++) {
      // Pixels are in BGRA format.
      double blue = bitmapData[index++] / 255.0;
      double green = bitmapData[index++] / 255.0;
      double red = bitmapData[index++] / 255.0;
      double alpha = bitmapData[index++] / 255.0;

      *redCount += red * alpha;
      *greenCount += green * alpha;
      *blueCount += blue * alpha;
    }
  }
}

// Tests that we can scale down an image
TEST(TestWindowGfx, CreateIcon_ScaledDown)
{
  auto Test = [](imgIContainer* image) {
    HICON icon;
    nsresult rv =
        nsWindowGfx::CreateIcon(image, nullptr, false, LayoutDeviceIntPoint(),
                                LayoutDeviceIntSize(50, 50), &icon);
    ASSERT_NS_SUCCEEDED(rv);

    ICONINFO ii;
    BOOL fResult = ::GetIconInfo(icon, &ii);
    ASSERT_TRUE(fResult);

    BITMAP bm;
    fResult = ::GetObject(ii.hbmColor, sizeof(bm), &bm) == sizeof(bm);
    ASSERT_TRUE(fResult);

    ASSERT_EQ(bm.bmWidth, 50);
    ASSERT_EQ(bm.bmHeight, 50);

    double redCount, greenCount, blueCount;
    CountPixels(ii, bm, &redCount, &greenCount, &blueCount);

    // We've scaled the image down to a quarter of its size.
    double fillArea = CIRCLE_FILL_AREA / 4;
    double strokeArea = CIRCLE_STROKE_AREA / 4;

    ASSERT_NEARLY(redCount, strokeArea);
    ASSERT_NEARLY(greenCount, fillArea);
    ASSERT_EQ(blueCount, 0.0);

    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);

    ::DestroyIcon(icon);
  };

  nsCOMPtr<imgIContainer> vectorImage;
  LoadImage(SVG_GREEN_CIRCLE, getter_AddRefs(vectorImage));

  Test(vectorImage);

  nsCOMPtr<imgIContainer> rasterImage;
  ConvertToRaster(vectorImage, getter_AddRefs(rasterImage));

  Test(rasterImage);
}

// Tests that we can scale up an image
TEST(TestWindowGfx, CreateIcon_ScaledUp)
{
  auto Test = [](imgIContainer* image) {
    HICON icon;
    nsresult rv =
        nsWindowGfx::CreateIcon(image, nullptr, false, LayoutDeviceIntPoint(),
                                LayoutDeviceIntSize(200, 200), &icon);
    ASSERT_NS_SUCCEEDED(rv);

    ICONINFO ii;
    BOOL fResult = ::GetIconInfo(icon, &ii);
    ASSERT_TRUE(fResult);

    BITMAP bm;
    fResult = ::GetObject(ii.hbmColor, sizeof(bm), &bm) == sizeof(bm);
    ASSERT_TRUE(fResult);

    ASSERT_EQ(bm.bmWidth, 200);
    ASSERT_EQ(bm.bmHeight, 200);

    double redCount, greenCount, blueCount;
    CountPixels(ii, bm, &redCount, &greenCount, &blueCount);

    // We've scaled the image up to four times its size.
    double fillArea = CIRCLE_FILL_AREA * 4;
    double strokeArea = CIRCLE_STROKE_AREA * 4;

    ASSERT_NEARLY(redCount, strokeArea);
    ASSERT_NEARLY(greenCount, fillArea);
    ASSERT_EQ(blueCount, 0.0);

    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);

    ::DestroyIcon(icon);
  };

  nsCOMPtr<imgIContainer> vectorImage;
  LoadImage(SVG_GREEN_CIRCLE, getter_AddRefs(vectorImage));

  Test(vectorImage);

  nsCOMPtr<imgIContainer> rasterImage;
  ConvertToRaster(vectorImage, getter_AddRefs(rasterImage));

  Test(rasterImage);
}

// Tests that we can render an image at its intrinsic size
TEST(TestWindowGfx, CreateIcon_Intrinsic)
{
  auto Test = [](imgIContainer* image) {
    HICON icon;
    nsresult rv =
        nsWindowGfx::CreateIcon(image, nullptr, false, LayoutDeviceIntPoint(),
                                LayoutDeviceIntSize(), &icon);
    ASSERT_NS_SUCCEEDED(rv);

    ICONINFO ii;
    BOOL fResult = ::GetIconInfo(icon, &ii);
    ASSERT_TRUE(fResult);

    BITMAP bm;
    fResult = ::GetObject(ii.hbmColor, sizeof(bm), &bm) == sizeof(bm);
    ASSERT_TRUE(fResult);

    ASSERT_EQ(bm.bmWidth, 100);
    ASSERT_EQ(bm.bmHeight, 100);

    double redCount, greenCount, blueCount;
    CountPixels(ii, bm, &redCount, &greenCount, &blueCount);

    ASSERT_NEARLY(redCount, CIRCLE_STROKE_AREA);
    ASSERT_NEARLY(greenCount, CIRCLE_FILL_AREA);
    ASSERT_EQ(blueCount, 0.0);

    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);

    ::DestroyIcon(icon);
  };

  nsCOMPtr<imgIContainer> vectorImage;
  LoadImage(SVG_GREEN_CIRCLE, getter_AddRefs(vectorImage));

  Test(vectorImage);

  nsCOMPtr<imgIContainer> rasterImage;
  ConvertToRaster(vectorImage, getter_AddRefs(rasterImage));

  Test(rasterImage);
}

// If an SVG has no intrinsic size and we don't provide one we fail.
TEST(TestWindowGfx, CreateIcon_SVG_NoSize)
{
  nsCOMPtr<imgIContainer> image;
  LoadImage(SVG_UNSIZED_CIRCLE, getter_AddRefs(image));

  HICON icon;
  nsresult rv =
      nsWindowGfx::CreateIcon(image, nullptr, false, LayoutDeviceIntPoint(),
                              LayoutDeviceIntSize(), &icon);
  ASSERT_EQ(rv, NS_ERROR_FAILURE);
}

// But we can still render an SVG with no intrinsic size as long as we provide
// one.
TEST(TestWindowGfx, CreateIcon_SVG_NoIntrinsic)
{
  nsCOMPtr<imgIContainer> image;
  LoadImage(SVG_UNSIZED_CIRCLE, getter_AddRefs(image));

  HICON icon;
  nsresult rv =
      nsWindowGfx::CreateIcon(image, nullptr, false, LayoutDeviceIntPoint(),
                              LayoutDeviceIntSize(200, 200), &icon);
  ASSERT_NS_SUCCEEDED(rv);

  ICONINFO ii;
  BOOL fResult = ::GetIconInfo(icon, &ii);
  ASSERT_TRUE(fResult);

  BITMAP bm;
  fResult = ::GetObject(ii.hbmColor, sizeof(bm), &bm) == sizeof(bm);
  ASSERT_TRUE(fResult);

  ASSERT_EQ(bm.bmWidth, 200);
  ASSERT_EQ(bm.bmHeight, 200);

  double redCount, greenCount, blueCount;
  CountPixels(ii, bm, &redCount, &greenCount, &blueCount);

  // We've scaled the image up to four times its size.
  double fillArea = CIRCLE_FILL_AREA * 4;
  double strokeArea = CIRCLE_STROKE_AREA * 4;

  ASSERT_NEARLY(redCount, strokeArea);
  ASSERT_NEARLY(greenCount, fillArea);
  ASSERT_EQ(blueCount, 0.0);

  if (ii.hbmMask) DeleteObject(ii.hbmMask);
  if (ii.hbmColor) DeleteObject(ii.hbmColor);

  ::DestroyIcon(icon);
}

// Tests that we can set SVG context-fill and context-stroke
TEST(TestWindowGfx, CreateIcon_SVG_Context)
{
  // Normally the context properties don't work for content documents including
  // data URIs.
  Preferences::SetBool("svg.context-properties.content.enabled", true);
  // This test breaks if color management is enabled and an earlier gtest may
  // have enabled it.
  gfxPlatform::SetCMSModeOverride(CMSMode::Off);

  nsCOMPtr<imgIContainer> image;
  LoadImage(SVG_CONTEXT_CIRCLE, getter_AddRefs(image));

  nsCOMPtr<nsISVGPaintContext> paintContext =
      new SvgPaintContext("#00FF00", "#0000FF");

  HICON icon;
  nsresult rv = nsWindowGfx::CreateIcon(image, paintContext, false,
                                        LayoutDeviceIntPoint(),
                                        LayoutDeviceIntSize(200, 200), &icon);
  ASSERT_NS_SUCCEEDED(rv);

  ICONINFO ii;
  BOOL fResult = ::GetIconInfo(icon, &ii);
  ASSERT_TRUE(fResult);

  BITMAP bm;
  fResult = ::GetObject(ii.hbmColor, sizeof(bm), &bm) == sizeof(bm);
  ASSERT_TRUE(fResult);

  ASSERT_EQ(bm.bmWidth, 200);
  ASSERT_EQ(bm.bmHeight, 200);

  double redCount, greenCount, blueCount;
  CountPixels(ii, bm, &redCount, &greenCount, &blueCount);

  // We've scaled the image up to four times its size.
  double fillArea = CIRCLE_FILL_AREA * 4;
  double strokeArea = CIRCLE_STROKE_AREA * 4;

  ASSERT_NEARLY(greenCount, strokeArea);
  ASSERT_NEARLY(blueCount, fillArea);
  ASSERT_EQ(redCount, 0.0);

  if (ii.hbmMask) DeleteObject(ii.hbmMask);
  if (ii.hbmColor) DeleteObject(ii.hbmColor);

  ::DestroyIcon(icon);
}
