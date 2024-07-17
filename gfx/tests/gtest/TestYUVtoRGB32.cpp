#include "gtest/gtest.h"

#include <array>
#include <cmath>
#include <tuple>
#include <unordered_map>

#include "ImageContainer.h"
#include "YCbCrUtils.h"
#include "nsTArray.h"

using Color = std::tuple<uint8_t, uint8_t, uint8_t>;
using namespace mozilla;

const Color BLACK(0, 0, 0);
const Color BLUE(0, 0, 255);
const Color GREEN(0, 255, 0);
const Color CYAN(0, 255, 255);
const Color RED(255, 0, 0);
const Color MAGENTA(255, 0, 255);
const Color YELLOW(255, 255, 0);
const Color WHITE(255, 255, 255);
const Color CHOCOLATE(210, 105, 30);
const std::array<Color, 9> COLOR_LIST = {BLACK,   BLUE,   GREEN, CYAN,     RED,
                                         MAGENTA, YELLOW, WHITE, CHOCOLATE};

Color RGB2YUV(const Color& aRGBColor) {
  const uint8_t& r = std::get<0>(aRGBColor);
  const uint8_t& g = std::get<1>(aRGBColor);
  const uint8_t& b = std::get<2>(aRGBColor);

  const double y = r * 0.299 + g * 0.587 + b * 0.114;
  const double u = r * -0.168736 + g * -0.331264 + b * 0.5 + 128;
  const double v = r * 0.5 + g * -0.418688 + b * -0.081312 + 128;

  return Color(round(y), round(u), round(v));
}

int32_t CeilingOfHalf(int32_t aValue) {
  MOZ_ASSERT(aValue >= 0);
  return aValue / 2 + (aValue % 2);
}

already_AddRefed<layers::PlanarYCbCrImage> CreateI420Image(
    const Color& aRGBColor, const gfx::YUVColorSpace& aColorSpace,
    const gfx::IntSize& aSize, Maybe<uint8_t> aAlphaValue = Nothing()) {
  const int32_t halfWidth = CeilingOfHalf(aSize.width);
  const int32_t halfHeight = CeilingOfHalf(aSize.height);

  const size_t yPlaneSize = aSize.width * aSize.height;
  const size_t uPlaneSize = halfWidth * halfHeight;
  const size_t vPlaneSize = uPlaneSize;
  const size_t aPlaneSize = aAlphaValue.isSome() ? yPlaneSize : 0;
  const size_t imageSize = yPlaneSize + uPlaneSize + vPlaneSize + aPlaneSize;

  const Color yuvColor = RGB2YUV(aRGBColor);
  const uint8_t& yColor = std::get<0>(yuvColor);
  const uint8_t& uColor = std::get<1>(yuvColor);
  const uint8_t& vColor = std::get<2>(yuvColor);

  UniquePtr<uint8_t[]> buffer(new uint8_t[imageSize]);

  layers::PlanarYCbCrData data;
  data.mPictureRect = gfx::IntRect({0, 0}, aSize);

  // Y plane.
  uint8_t* yChannel = buffer.get();
  memset(yChannel, yColor, yPlaneSize);
  data.mYChannel = yChannel;
  data.mYStride = aSize.width;
  data.mYSkip = 0;

  // Cb plane (aka U).
  uint8_t* uChannel = yChannel + yPlaneSize;
  memset(uChannel, uColor, uPlaneSize);
  data.mCbChannel = uChannel;
  data.mCbSkip = 0;

  // Cr plane (aka V).
  uint8_t* vChannel = uChannel + uPlaneSize;
  memset(vChannel, vColor, vPlaneSize);
  data.mCrChannel = vChannel;
  data.mCrSkip = 0;

  // CrCb plane vectors.
  data.mCbCrStride = halfWidth;
  data.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;

  // Alpha plane.
  if (aPlaneSize) {
    uint8_t* aChannel = vChannel + vPlaneSize;
    memset(aChannel, *aAlphaValue, aPlaneSize);
    data.mAlpha.emplace();
    data.mAlpha->mChannel = aChannel;
    data.mAlpha->mSize = aSize;
  }

  data.mYUVColorSpace = aColorSpace;

  RefPtr<layers::PlanarYCbCrImage> image =
      new layers::RecyclingPlanarYCbCrImage(new layers::BufferRecycleBin());
  image->CopyData(data);
  return image.forget();
}

already_AddRefed<layers::PlanarYCbCrImage> CreateI444Image(
    const Color& aRGBColor, const gfx::YUVColorSpace& aColorSpace,
    const gfx::IntSize& aSize, Maybe<uint8_t> aAlphaValue = Nothing()) {
  const size_t yPlaneSize = aSize.width * aSize.height;
  const size_t uPlaneSize = yPlaneSize;
  const size_t vPlaneSize = yPlaneSize;
  const size_t aPlaneSize = aAlphaValue.isSome() ? yPlaneSize : 0;
  const size_t imageSize = yPlaneSize + uPlaneSize + vPlaneSize + aPlaneSize;

  const Color yuvColor = RGB2YUV(aRGBColor);
  const uint8_t& yColor = std::get<0>(yuvColor);
  const uint8_t& uColor = std::get<1>(yuvColor);
  const uint8_t& vColor = std::get<2>(yuvColor);

  UniquePtr<uint8_t[]> buffer(new uint8_t[imageSize]);

  layers::PlanarYCbCrData data;
  data.mPictureRect = gfx::IntRect({0, 0}, aSize);

  // Y plane.
  uint8_t* yChannel = buffer.get();
  memset(yChannel, yColor, yPlaneSize);
  data.mYChannel = yChannel;
  data.mYStride = aSize.width;
  data.mYSkip = 0;

  // Cb plane (aka U).
  uint8_t* uChannel = yChannel + yPlaneSize;
  memset(uChannel, uColor, uPlaneSize);
  data.mCbChannel = uChannel;
  data.mCbSkip = 0;

  // Cr plane (aka V).
  uint8_t* vChannel = uChannel + uPlaneSize;
  memset(vChannel, vColor, vPlaneSize);
  data.mCrChannel = vChannel;
  data.mCrSkip = 0;

  // CrCb plane vectors.
  data.mCbCrStride = data.mYStride;
  data.mChromaSubsampling = gfx::ChromaSubsampling::FULL;

  // Alpha plane.
  if (aPlaneSize) {
    uint8_t* aChannel = vChannel + vPlaneSize;
    memset(aChannel, *aAlphaValue, aPlaneSize);
    data.mAlpha.emplace();
    data.mAlpha->mChannel = aChannel;
    data.mAlpha->mSize = aSize;
  }

  data.mYUVColorSpace = aColorSpace;

  RefPtr<layers::PlanarYCbCrImage> image =
      new layers::RecyclingPlanarYCbCrImage(new layers::BufferRecycleBin());
  image->CopyData(data);
  return image.forget();
}

void IsColorEqual(uint8_t* aBGRX, uint8_t* aRGBX, size_t aSize) {
  ASSERT_EQ(aSize % 4, (size_t)0);
  for (size_t i = 0; i < aSize; i += 4) {
    ASSERT_EQ(aBGRX[i + 2], aRGBX[i]);      // R
    ASSERT_EQ(aBGRX[i + 1], aRGBX[i + 1]);  // G
    ASSERT_EQ(aBGRX[i], aRGBX[i + 2]);      // B
    ASSERT_EQ(aBGRX[i + 3], aRGBX[i + 3]);  // X or A
  }
}

uint32_t Hash(const Color& aColor) {
  const uint8_t& r = std::get<0>(aColor);
  const uint8_t& g = std::get<1>(aColor);
  const uint8_t& b = std::get<2>(aColor);
  return r << 16 | g << 8 | b;
}

std::unordered_map<uint32_t, std::array<Color, 3>> GetExpectedConvertedRGB() {
  static std::unordered_map<uint32_t, std::array<Color, 3>> map;
  map.emplace(Hash(BLACK), std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                                Color(0, 0, 0),
                                                // gfx::YUVColorSpace::BT709
                                                Color(0, 0, 0),
                                                // gfx::YUVColorSpace::BT2020
                                                Color(0, 0, 0)});
  map.emplace(Hash(BLUE), std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                               Color(0, 82, 0),
                                               // gfx::YUVColorSpace::BT709
                                               Color(0, 54, 0),
                                               // gfx::YUVColorSpace::BT2020
                                               Color(0, 53, 0)});
  map.emplace(Hash(GREEN), std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                                Color(0, 255, 0),
                                                // gfx::YUVColorSpace::BT709
                                                Color(0, 231, 0),
                                                // gfx::YUVColorSpace::BT2020
                                                Color(0, 242, 0)});
  map.emplace(Hash(CYAN), std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                               Color(0, 255, 255),
                                               // gfx::YUVColorSpace::BT709
                                               Color(0, 248, 255),
                                               // gfx::YUVColorSpace::BT2020
                                               Color(0, 255, 255)});
  map.emplace(Hash(RED), std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                              Color(0, 191, 0),
                                              // gfx::YUVColorSpace::BT709
                                              Color(0, 147, 0),
                                              // gfx::YUVColorSpace::BT2020
                                              Color(0, 162, 0)});
  map.emplace(Hash(MAGENTA), std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                                  Color(255, 0, 255),
                                                  // gfx::YUVColorSpace::BT709
                                                  Color(255, 28, 255),
                                                  // gfx::YUVColorSpace::BT2020
                                                  Color(255, 18, 255)});
  map.emplace(Hash(YELLOW), std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                                 Color(255, 255, 0),
                                                 // gfx::YUVColorSpace::BT709
                                                 Color(255, 255, 0),
                                                 // gfx::YUVColorSpace::BT2020
                                                 Color(255, 255, 0)});
  map.emplace(Hash(WHITE), std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                                Color(255, 255, 255),
                                                // gfx::YUVColorSpace::BT709
                                                Color(255, 255, 255),
                                                // gfx::YUVColorSpace::BT2020
                                                Color(255, 255, 255)});
  map.emplace(Hash(CHOCOLATE),
              std::array<Color, 3>{// gfx::YUVColorSpace::BT601
                                   Color(224, 104, 20),
                                   // gfx::YUVColorSpace::BT709
                                   Color(236, 111, 20),
                                   // gfx::YUVColorSpace::BT2020
                                   Color(229, 102, 20)});
  return map;
}

void IsColorMatched(const Color& aColor, uint8_t* aRGBX, size_t aSize,
                    Maybe<uint8_t> aAlphaValue = Nothing()) {
  const uint8_t& r = std::get<0>(aColor);
  const uint8_t& g = std::get<1>(aColor);
  const uint8_t& b = std::get<2>(aColor);
  for (size_t i = 0; i < aSize; i += 4) {
    ASSERT_EQ(r, aRGBX[i]);      // R
    ASSERT_EQ(g, aRGBX[i + 1]);  // G
    ASSERT_EQ(b, aRGBX[i + 2]);  // B
    if (aAlphaValue) {
      ASSERT_EQ(*aAlphaValue, aRGBX[i + 3]);  // A
    }
  }
}

TEST(YCbCrUtils, ConvertYCbCrToRGB32)
{
  const gfx::IntSize imgSize(32, 16);
  const int32_t stride =
      imgSize.Width() * gfx::BytesPerPixel(gfx::SurfaceFormat::B8G8R8X8);
  const size_t bufferSize = stride * imgSize.Height();

  const std::array<gfx::YUVColorSpace, 3> colorSpaces{
      gfx::YUVColorSpace::BT601, gfx::YUVColorSpace::BT709,
      gfx::YUVColorSpace::BT2020};

  std::unordered_map<uint32_t, std::array<Color, 3>> expectations =
      GetExpectedConvertedRGB();

  for (const Color& color : COLOR_LIST) {
    const std::array<Color, 3>& expectedColors = expectations[Hash(color)];
    for (const gfx::YUVColorSpace& colorSpace : colorSpaces) {
      RefPtr<layers::PlanarYCbCrImage> img =
          CreateI420Image(color, colorSpace, imgSize);

      UniquePtr<uint8_t[]> BGRX = MakeUnique<uint8_t[]>(bufferSize);
      ConvertYCbCrToRGB32(*img->GetData(), gfx::SurfaceFormat::B8G8R8X8,
                          BGRX.get(), stride, nullptr);

      UniquePtr<uint8_t[]> RGBX = MakeUnique<uint8_t[]>(bufferSize);
      ConvertYCbCrToRGB32(*img->GetData(), gfx::SurfaceFormat::R8G8B8X8,
                          RGBX.get(), stride, nullptr);

      IsColorEqual(BGRX.get(), RGBX.get(), bufferSize);

      Color expectation = expectedColors[static_cast<size_t>(colorSpace)];
      IsColorMatched(expectation, RGBX.get(), bufferSize);
    }
  }
}

TEST(YCbCrUtils, ConvertYCbCrToRGB32WithAlpha)
{
  const gfx::IntSize imgSize(32, 16);
  const int32_t stride =
      imgSize.Width() * gfx::BytesPerPixel(gfx::SurfaceFormat::B8G8R8A8);
  const size_t bufferSize = stride * imgSize.Height();

  const std::array<gfx::YUVColorSpace, 3> colorSpaces{
      gfx::YUVColorSpace::BT601, gfx::YUVColorSpace::BT709,
      gfx::YUVColorSpace::BT2020};

  std::unordered_map<uint32_t, std::array<Color, 3>> expectations =
      GetExpectedConvertedRGB();

  for (const Color& color : COLOR_LIST) {
    const std::array<Color, 3>& expectedColors = expectations[Hash(color)];
    for (const gfx::YUVColorSpace& colorSpace : colorSpaces) {
      Maybe<uint8_t> alpha = Some(128);
      RefPtr<layers::PlanarYCbCrImage> img =
          CreateI420Image(color, colorSpace, imgSize, alpha);

      UniquePtr<uint8_t[]> BGRA = MakeUnique<uint8_t[]>(bufferSize);
      ConvertYCbCrToRGB32(*img->GetData(), gfx::SurfaceFormat::B8G8R8A8,
                          BGRA.get(), stride, nullptr);

      UniquePtr<uint8_t[]> RGBA = MakeUnique<uint8_t[]>(bufferSize);
      ConvertYCbCrToRGB32(*img->GetData(), gfx::SurfaceFormat::R8G8B8A8,
                          RGBA.get(), stride, nullptr);

      IsColorEqual(BGRA.get(), RGBA.get(), bufferSize);

      Color expectation = expectedColors[static_cast<size_t>(colorSpace)];
      IsColorMatched(expectation, RGBA.get(), bufferSize, alpha);
    }
  }
}

TEST(YCbCrUtils, ConvertYCbCrToRGB32WithIdentityColorSpace)
{
  const gfx::IntSize imgSize(32, 16);
  const int32_t stride =
      imgSize.Width() * gfx::BytesPerPixel(gfx::SurfaceFormat::B8G8R8X8);
  const size_t bufferSize = stride * imgSize.Height();

  for (const Color& color : COLOR_LIST) {
    RefPtr<layers::PlanarYCbCrImage> img =
        CreateI444Image(color, gfx::YUVColorSpace::Identity, imgSize);

    UniquePtr<uint8_t[]> BGRX = MakeUnique<uint8_t[]>(bufferSize);
    ConvertYCbCrToRGB32(*img->GetData(), gfx::SurfaceFormat::B8G8R8X8,
                        BGRX.get(), stride, nullptr);

    UniquePtr<uint8_t[]> RGBX = MakeUnique<uint8_t[]>(bufferSize);
    ConvertYCbCrToRGB32(*img->GetData(), gfx::SurfaceFormat::R8G8B8X8,
                        RGBX.get(), stride, nullptr);

    IsColorEqual(BGRX.get(), RGBX.get(), bufferSize);

    const Color yuvColor = RGB2YUV(color);
    const uint8_t& y = std::get<0>(yuvColor);
    const uint8_t& u = std::get<1>(yuvColor);
    const uint8_t& v = std::get<2>(yuvColor);
    const Color expectation(v, y, u);
    IsColorMatched(expectation, RGBX.get(), bufferSize);
  }
}
