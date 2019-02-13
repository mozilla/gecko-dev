/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TILEDLAYERBUFFER_H
#define GFX_TILEDLAYERBUFFER_H

// Debug defines
//#define GFX_TILEDLAYER_DEBUG_OVERLAY
//#define GFX_TILEDLAYER_PREF_WARNINGS
//#define GFX_TILEDLAYER_RETAINING_LOG

#include <stdint.h>                     // for uint16_t, uint32_t
#include <sys/types.h>                  // for int32_t
#include "gfxPlatform.h"                // for GetTileWidth/GetTileHeight
#include "LayersLogging.h"              // for print_stderr
#include "mozilla/gfx/Logging.h"        // for gfxCriticalError
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
#include <ui/Fence.h>
#endif

namespace mozilla {

struct TileUnit {};
template<> struct IsPixel<TileUnit> : mozilla::TrueType {};

namespace layers {

// You can enable all the TILING_LOG print statements by
// changing the 0 to a 1 in the following #define.
#define ENABLE_TILING_LOG 0

#if ENABLE_TILING_LOG
#  define TILING_LOG(...) printf_stderr(__VA_ARGS__);
#else
#  define TILING_LOG(...)
#endif

// Normal integer division truncates towards zero,
// we instead want to floor to hangle negative numbers.
static inline int floor_div(int a, int b)
{
  int rem = a % b;
  int div = a/b;
  if (rem == 0) {
    return div;
  } else {
    // If the signs are different substract 1.
    int sub;
    sub = a ^ b;
    // The results of this shift is either 0 or -1.
    sub >>= 8*sizeof(int)-1;
    return div+sub;
  }
}

// An abstract implementation of a tile buffer. This code covers the logic of
// moving and reusing tiles and leaves the validation up to the implementor. To
// avoid the overhead of virtual dispatch, we employ the curiously recurring
// template pattern.
//
// Tiles are aligned to a grid with one of the grid points at (0,0) and other
// grid points spaced evenly in the x- and y-directions by GetTileSize()
// multiplied by mResolution. GetScaledTileSize() provides convenience for
// accessing these values.
//
// This tile buffer stores a valid region, which defines the areas that have
// up-to-date content. The contents of tiles within this region will be reused
// from paint to paint. It also stores the region that was modified in the last
// paint operation; this is useful when one tiled layer buffer shadows another
// (as in an off-main-thread-compositing scenario), so that the shadow tiled
// layer buffer can correctly reflect the updates of the master layer buffer.
//
// The associated Tile may be of any type as long as the derived class can
// validate and return tiles of that type. Tiles will be frequently copied, so
// the tile type should be a reference or some other type with an efficient
// copy constructor.
//
// It is required that the derived class specify the base class as a friend. It
// must also implement the following public method:
//
//   Tile GetPlaceholderTile() const;
//
//   Returns a temporary placeholder tile used as a marker. This placeholder tile
//   must never be returned by validateTile and must be == to every instance
//   of a placeholder tile.
//
// Additionally, it must implement the following protected methods:
//
//   Tile ValidateTile(Tile aTile, const nsIntPoint& aTileOrigin,
//                     const nsIntRegion& aDirtyRect);
//
//   Validates the dirtyRect. The returned Tile will replace the tile.
//
//   void ReleaseTile(Tile aTile);
//
//   Destroys the given tile.
//
//   void SwapTiles(Tile& aTileA, Tile& aTileB);
//
//   Swaps two tiles.
//
// The contents of the tile buffer will be rendered at the resolution specified
// in mResolution, which can be altered with SetResolution. The resolution
// should always be a factor of the tile length, to avoid tiles covering
// non-integer amounts of pixels.

// Size and Point in number of tiles rather than in pixels
typedef gfx::IntSizeTyped<TileUnit> TileIntSize;
typedef gfx::IntPointTyped<TileUnit> TileIntPoint;

/**
 * Stores the origin and size of a tile buffer and handles switching between
 * tile indices and tile positions.
 *
 * Tile positions in TileIntPoint take the first tile offset into account which
 * means that two TilesPlacement of the same layer and resolution give tile
 * positions in the same coordinate space (useful when changing the offset and/or
 * size of a tile buffer).
 */
struct TilesPlacement {
  // in tiles
  TileIntPoint mFirst;
  TileIntSize mSize;

  TilesPlacement(int aFirstX, int aFirstY,
                 int aRetainedWidth, int aRetainedHeight)
  : mFirst(aFirstX, aFirstY)
  , mSize(aRetainedWidth, aRetainedHeight)
  {}

  int TileIndex(TileIntPoint aPosition) const {
    return (aPosition.x - mFirst.x) * mSize.height + aPosition.y - mFirst.y;
  }

  TileIntPoint TilePosition(size_t aIndex) const {
    return TileIntPoint(
      mFirst.x + aIndex / mSize.height,
      mFirst.y + aIndex % mSize.height
    );
  }

  bool HasTile(TileIntPoint aPosition) {
    return aPosition.x >= mFirst.x && aPosition.x < mFirst.x + mSize.width &&
           aPosition.y >= mFirst.y && aPosition.y < mFirst.y + mSize.height;
  }
};

template<typename Derived, typename Tile>
class TiledLayerBuffer
{
public:
  TiledLayerBuffer()
    : mTiles(0, 0, 0, 0)
    , mResolution(1)
    , mTileSize(gfxPlatform::GetPlatform()->GetTileWidth(),
                gfxPlatform::GetPlatform()->GetTileHeight())
  {}

  ~TiledLayerBuffer() {}

  // Given a tile origin aligned to a multiple of GetScaledTileSize,
  // return the tile that describes that region.
  // NOTE: To get the valid area of that tile you must intersect
  //       (aTileOrigin.x, aTileOrigin.y,
  //        GetScaledTileSize().width, GetScaledTileSize().height)
  //       and GetValidRegion() to get the area of the tile that is valid.
  Tile& GetTile(const gfx::IntPoint& aTileOrigin);
  // Given a tile x, y relative to the top left of the layer, this function
  // will return the tile for
  // (x*GetScaledTileSize().width, y*GetScaledTileSize().height,
  //  GetScaledTileSize().width, GetScaledTileSize().height)
  Tile& GetTile(int x, int y);

  Tile& GetTile(size_t i) { return mRetainedTiles[i]; }

  gfx::IntPoint GetTileOffset(TileIntPoint aPosition) const {
    gfx::IntSize scaledTileSize = GetScaledTileSize();
    return gfx::IntPoint(aPosition.x * scaledTileSize.width,
                         aPosition.y * scaledTileSize.height);
  }

  const TilesPlacement& GetPlacement() const { return mTiles; }

  int TileIndex(const gfx::IntPoint& aTileOrigin) const;
  int TileIndex(int x, int y) const { return x * mTiles.mSize.height + y; }

  bool HasTile(int index) const { return index >= 0 && index < (int)mRetainedTiles.Length(); }
  bool HasTile(const gfx::IntPoint& aTileOrigin) const;
  bool HasTile(int x, int y) const {
    return x >= 0 && x < mTiles.mSize.width && y >= 0 && y < mTiles.mSize.height;
  }

  const gfx::IntSize& GetTileSize() const { return mTileSize; }

  gfx::IntSize GetScaledTileSize() const { return RoundedToInt(gfx::Size(mTileSize) / mResolution); }

  unsigned int GetTileCount() const { return mRetainedTiles.Length(); }

  const nsIntRegion& GetValidRegion() const { return mValidRegion; }
  const nsIntRegion& GetPaintedRegion() const { return mPaintedRegion; }
  void ClearPaintedRegion() { mPaintedRegion.SetEmpty(); }

  void ResetPaintedAndValidState() {
    mPaintedRegion.SetEmpty();
    mValidRegion.SetEmpty();
    mTiles.mSize.width = 0;
    mTiles.mSize.height = 0;
    for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
      if (!mRetainedTiles[i].IsPlaceholderTile()) {
        AsDerived().ReleaseTile(mRetainedTiles[i]);
      }
    }
    mRetainedTiles.Clear();
  }

  // Given a position i, this function returns the position inside the current tile.
  int GetTileStart(int i, int aTileLength) const {
    return (i >= 0) ? (i % aTileLength)
                    : ((aTileLength - (-i % aTileLength)) %
                       aTileLength);
  }

  // Rounds the given coordinate down to the nearest tile boundary.
  int RoundDownToTileEdge(int aX, int aTileLength) const { return aX - GetTileStart(aX, aTileLength); }

  // Get and set draw scaling. mResolution affects the resolution at which the
  // contents of the buffer are drawn. mResolution has no effect on the
  // coordinate space of the valid region, but does affect the size of an
  // individual tile's rect in relation to the valid region.
  // Setting the resolution will invalidate the buffer.
  float GetResolution() const { return mResolution; }
  bool IsLowPrecision() const { return mResolution < 1; }

  typedef Tile* Iterator;
  Iterator TilesBegin() { return mRetainedTiles.Elements(); }
  Iterator TilesEnd() { return mRetainedTiles.Elements() + mRetainedTiles.Length(); }

  void Dump(std::stringstream& aStream, const char* aPrefix, bool aDumpHtml);

protected:
  // The implementor should call Update() to change
  // the new valid region. This implementation will call
  // validateTile on each tile that is dirty, which is left
  // to the implementor.
  void Update(const nsIntRegion& aNewValidRegion, const nsIntRegion& aPaintRegion);

  // Return a reference to this tile in GetTile when the requested tile offset
  // does not exist.
  Tile mPlaceHolderTile;

  nsIntRegion     mValidRegion;
  nsIntRegion     mPaintedRegion;

  /**
   * mRetainedTiles is a rectangular buffer of mTiles.mSize.width x mTiles.mSize.height
   * stored as column major with the same origin as mValidRegion.GetBounds().
   * Any tile that does not intersect mValidRegion is a PlaceholderTile.
   * Only the region intersecting with mValidRegion should be read from a tile,
   * another other region is assumed to be uninitialized. The contents of the
   * tiles is scaled by mResolution.
   */
  nsTArray<Tile>  mRetainedTiles;
  TilesPlacement  mTiles;
  float           mResolution;
  gfx::IntSize    mTileSize;

private:
  const Derived& AsDerived() const { return *static_cast<const Derived*>(this); }
  Derived& AsDerived() { return *static_cast<Derived*>(this); }
};

class ClientTiledLayerBuffer;
class SurfaceDescriptorTiles;
class ISurfaceAllocator;

// Shadow layers may implement this interface in order to be notified when a
// tiled layer buffer is updated.
class TiledLayerComposer
{
public:
  /**
   * Update the current retained layer with the updated layer data.
   * It is expected that the tiles described by aTiledDescriptor are all in the
   * ReadLock state, so that the locks can be adopted when recreating a
   * ClientTiledLayerBuffer locally. This lock will be retained until the buffer
   * has completed uploading.
   *
   * Returns false if a deserialization error happened, in which case we will
   * have to kill the child process.
   */
  virtual bool UseTiledLayerBuffer(ISurfaceAllocator* aAllocator,
                                   const SurfaceDescriptorTiles& aTiledDescriptor) = 0;

  /**
   * If some part of the buffer is being rendered at a lower precision, this
   * returns that region. If it is not, an empty region will be returned.
   */
  virtual const nsIntRegion& GetValidLowPrecisionRegion() const = 0;

  virtual const nsIntRegion& GetValidRegion() const = 0;
};

template<typename Derived, typename Tile> bool
TiledLayerBuffer<Derived, Tile>::HasTile(const gfx::IntPoint& aTileOrigin) const {
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  return HasTile(floor_div(aTileOrigin.x, scaledTileSize.width) - mTiles.mFirst.x,
                 floor_div(aTileOrigin.y, scaledTileSize.height) - mTiles.mFirst.y);
}

template<typename Derived, typename Tile> Tile&
TiledLayerBuffer<Derived, Tile>::GetTile(const nsIntPoint& aTileOrigin)
{
  if (HasTile(aTileOrigin)) {
    return mRetainedTiles[TileIndex(aTileOrigin)];
  }
  return mPlaceHolderTile;
}

template<typename Derived, typename Tile> int
TiledLayerBuffer<Derived, Tile>::TileIndex(const gfx::IntPoint& aTileOrigin) const
{
  // Find the tile x/y of the first tile and the target tile relative to the (0, 0)
  // origin, the difference is the tile x/y relative to the start of the tile buffer.
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  return TileIndex(floor_div(aTileOrigin.x, scaledTileSize.width) - mTiles.mFirst.x,
                   floor_div(aTileOrigin.y, scaledTileSize.height) - mTiles.mFirst.y);
}

template<typename Derived, typename Tile> Tile&
TiledLayerBuffer<Derived, Tile>::GetTile(int x, int y)
{
  if (HasTile(x, y)) {
    return mRetainedTiles[TileIndex(x, y)];
  }
  return mPlaceHolderTile;
}

template<typename Derived, typename Tile> void
TiledLayerBuffer<Derived, Tile>::Dump(std::stringstream& aStream,
                                      const char* aPrefix,
                                      bool aDumpHtml)
{
  gfx::IntRect visibleRect = GetValidRegion().GetBounds();
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  for (int32_t x = visibleRect.x; x < visibleRect.x + visibleRect.width;) {
    int32_t tileStartX = GetTileStart(x, scaledTileSize.width);
    int32_t w = scaledTileSize.width - tileStartX;

    for (int32_t y = visibleRect.y; y < visibleRect.y + visibleRect.height;) {
      int32_t tileStartY = GetTileStart(y, scaledTileSize.height);
      nsIntPoint tileOrigin = nsIntPoint(RoundDownToTileEdge(x, scaledTileSize.width),
                                         RoundDownToTileEdge(y, scaledTileSize.height));
      Tile& tileTexture = GetTile(tileOrigin);
      int32_t h = scaledTileSize.height - tileStartY;

      aStream << "\n" << aPrefix << "Tile (x=" <<
        RoundDownToTileEdge(x, scaledTileSize.width) << ", y=" <<
        RoundDownToTileEdge(y, scaledTileSize.height) << "): ";
      if (!tileTexture.IsPlaceholderTile()) {
        tileTexture.DumpTexture(aStream);
      } else {
        aStream << "empty tile";
      }
      y += h;
    }
    x += w;
  }
}

template<typename Derived, typename Tile> void
TiledLayerBuffer<Derived, Tile>::Update(const nsIntRegion& newValidRegion,
                                        const nsIntRegion& aPaintRegion)
{
  gfx::IntSize scaledTileSize = GetScaledTileSize();

  nsTArray<Tile>  newRetainedTiles;
  nsTArray<Tile>& oldRetainedTiles = mRetainedTiles;
  const gfx::IntRect oldBound = mValidRegion.GetBounds();
  const gfx::IntRect newBound = newValidRegion.GetBounds();
  const nsIntPoint oldBufferOrigin(RoundDownToTileEdge(oldBound.x, scaledTileSize.width),
                                   RoundDownToTileEdge(oldBound.y, scaledTileSize.height));
  const nsIntPoint newBufferOrigin(RoundDownToTileEdge(newBound.x, scaledTileSize.width),
                                   RoundDownToTileEdge(newBound.y, scaledTileSize.height));

  // This is the reason we break the style guide with newValidRegion instead
  // of aNewValidRegion - so that the names match better and code easier to read
  const nsIntRegion& oldValidRegion = mValidRegion;
  const int oldRetainedHeight = mTiles.mSize.height;

#ifdef GFX_TILEDLAYER_RETAINING_LOG
  { // scope ss
    std::stringstream ss;
    ss << "TiledLayerBuffer " << this << " starting update"
       << " on bounds ";
    AppendToString(ss, newBound);
    ss << " with mResolution=" << mResolution << "\n";
    for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
      ss << "mRetainedTiles[" << i << "] = ";
      mRetainedTiles[i].Dump(ss);
      ss << "\n";
    }
    print_stderr(ss);
  }
#endif

  // Pass 1: Recycle valid content from the old buffer
  // Recycle tiles from the old buffer that contain valid regions.
  // Insert placeholders tiles if we have no valid area for that tile
  // which we will allocate in pass 2.
  // TODO: Add a tile pool to reduce new allocation
  int tileX = 0;
  int tileY = 0;
  int tilesMissing = 0;
  // Iterate over the new drawing bounds in steps of tiles.
  for (int32_t x = newBound.x; x < newBound.XMost(); tileX++) {
    // Compute tileRect(x,y,width,height) in layer space coordinate
    // giving us the rect of the tile that hits the newBounds.
    int width = scaledTileSize.width - GetTileStart(x, scaledTileSize.width);
    if (x + width > newBound.XMost()) {
      width = newBound.x + newBound.width - x;
    }

    tileY = 0;
    for (int32_t y = newBound.y; y < newBound.YMost(); tileY++) {
      int height = scaledTileSize.height - GetTileStart(y, scaledTileSize.height);
      if (y + height > newBound.y + newBound.height) {
        height = newBound.y + newBound.height - y;
      }

      const gfx::IntRect tileRect(x,y,width,height);
      if (oldValidRegion.Intersects(tileRect) && newValidRegion.Intersects(tileRect)) {
        // This old tiles contains some valid area so move it to the new tile
        // buffer. Replace the tile in the old buffer with a placeholder
        // to leave the old buffer index unaffected.
        int tileX = floor_div(x - oldBufferOrigin.x, scaledTileSize.width);
        int tileY = floor_div(y - oldBufferOrigin.y, scaledTileSize.height);
        int index = tileX * oldRetainedHeight + tileY;

        // The tile may have been removed, skip over it in this case.
        if (oldRetainedTiles.
                          SafeElementAt(index, AsDerived().GetPlaceholderTile()).IsPlaceholderTile()) {
          newRetainedTiles.AppendElement(AsDerived().GetPlaceholderTile());
        } else {
          Tile tileWithPartialValidContent = oldRetainedTiles[index];
          newRetainedTiles.AppendElement(tileWithPartialValidContent);
          oldRetainedTiles[index] = AsDerived().GetPlaceholderTile();
        }

      } else {
        // This tile is either:
        // 1) Outside the new valid region and will simply be an empty
        // placeholder forever.
        // 2) The old buffer didn't have any data for this tile. We postpone
        // the allocation of this tile after we've reused any tile with
        // valid content because then we know we can safely recycle
        // with taking from a tile that has recyclable content.
        newRetainedTiles.AppendElement(AsDerived().GetPlaceholderTile());

        if (aPaintRegion.Intersects(tileRect)) {
          tilesMissing++;
        }
      }

      y += height;
    }

    x += width;
  }

  // Keep track of the number of horizontal/vertical tiles
  // in the buffer so that we can easily look up a tile.
  mTiles.mSize.width = tileX;
  mTiles.mSize.height = tileY;

#ifdef GFX_TILEDLAYER_RETAINING_LOG
  { // scope ss
    std::stringstream ss;
    ss << "TiledLayerBuffer " << this << " finished pass 1 of update;"
       << " tilesMissing=" << tilesMissing << "\n";
    for (size_t i = 0; i < oldRetainedTiles.Length(); i++) {
      ss << "oldRetainedTiles[" << i << "] = ";
      oldRetainedTiles[i].Dump(ss);
      ss << "\n";
    }
    print_stderr(ss);
  }
#endif


  // Pass 1.5: Release excess tiles in oldRetainedTiles
  // Tiles in oldRetainedTiles that aren't in newRetainedTiles will be recycled
  // before creating new ones, but there could still be excess unnecessary
  // tiles. As tiles may not have a fixed memory cost (for example, due to
  // double-buffering), we should release these excess tiles first.
  int oldTileCount = 0;
  for (size_t i = 0; i < oldRetainedTiles.Length(); i++) {
    Tile oldTile = oldRetainedTiles[i];
    if (oldTile.IsPlaceholderTile()) {
      continue;
    }

    if (oldTileCount >= tilesMissing) {
      oldRetainedTiles[i] = AsDerived().GetPlaceholderTile();
      AsDerived().ReleaseTile(oldTile);
    } else {
      oldTileCount ++;
    }
  }

  if (!newValidRegion.Contains(aPaintRegion)) {
    gfxCriticalError() << "Painting outside visible:"
		       << " paint " << aPaintRegion.ToString().get()
                       << " old valid " << oldValidRegion.ToString().get()
                       << " new valid " << newValidRegion.ToString().get();
  }
#ifdef DEBUG
  nsIntRegion oldAndPainted(oldValidRegion);
  oldAndPainted.Or(oldAndPainted, aPaintRegion);
  if (!oldAndPainted.Contains(newValidRegion)) {
    gfxCriticalError() << "Not fully painted:"
		       << " paint " << aPaintRegion.ToString().get()
                       << " old valid " << oldValidRegion.ToString().get()
                       << " old painted " << oldAndPainted.ToString().get()
                       << " new valid " << newValidRegion.ToString().get();
  }
#endif

  nsIntRegion regionToPaint(aPaintRegion);

#ifdef GFX_TILEDLAYER_RETAINING_LOG
  { // scope ss
    std::stringstream ss;
    ss << "TiledLayerBuffer " << this << " finished pass 1.5 of update\n";
    for (size_t i = 0; i < oldRetainedTiles.Length(); i++) {
      ss << "oldRetainedTiles[" << i << "] = ";
      oldRetainedTiles[i].Dump(ss);
      ss << "\n";
    }
    for (size_t i = 0; i < newRetainedTiles.Length(); i++) {
      ss << "newRetainedTiles[" << i << "] = ";
      newRetainedTiles[i].Dump(ss);
      ss << "\n";
    }
    print_stderr(ss);
  }
#endif

  // Pass 2: Validate
  // We know at this point that any tile in the new buffer that had valid content
  // from the previous buffer is placed correctly in the new buffer.
  // We know that any tile in the old buffer that isn't a place holder is
  // of no use and can be recycled.
  // We also know that any place holder tile in the new buffer must be
  // allocated.
  tileX = 0;
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Update %i, %i, %i, %i\n", newBound.x, newBound.y, newBound.width, newBound.height);
#endif
  for (int x = newBound.x; x < newBound.x + newBound.width; tileX++) {
    // Compute tileRect(x,y,width,height) in layer space coordinate
    // giving us the rect of the tile that hits the newBounds.
    int tileStartX = RoundDownToTileEdge(x, scaledTileSize.width);
    int width = scaledTileSize.width - GetTileStart(x, scaledTileSize.width);
    if (x + width > newBound.XMost())
      width = newBound.XMost() - x;

    tileY = 0;
    for (int y = newBound.y; y < newBound.y + newBound.height; tileY++) {
      int tileStartY = RoundDownToTileEdge(y, scaledTileSize.height);
      int height = scaledTileSize.height - GetTileStart(y, scaledTileSize.height);
      if (y + height > newBound.YMost()) {
        height = newBound.YMost() - y;
      }

      const gfx::IntRect tileRect(x, y, width, height);

      nsIntRegion tileDrawRegion;
      tileDrawRegion.And(tileRect, regionToPaint);

      if (tileDrawRegion.IsEmpty()) {
        // We have a tile but it doesn't hit the draw region
        // because we can reuse all of the content from the
        // previous buffer.
#ifdef DEBUG
        int currTileX = floor_div(x - newBufferOrigin.x, scaledTileSize.width);
        int currTileY = floor_div(y - newBufferOrigin.y, scaledTileSize.height);
        int index = TileIndex(currTileX, currTileY);
        // If allocating a tile failed we can run into this assertion.
        // Rendering is going to be glitchy but we don't want to crash.
        NS_ASSERTION(!newValidRegion.Intersects(tileRect) ||
                     !newRetainedTiles.
                                    SafeElementAt(index, AsDerived().GetPlaceholderTile()).IsPlaceholderTile(),
                     "Unexpected placeholder tile");

#endif
        y += height;
        continue;
      }

      int tileX = floor_div(x - newBufferOrigin.x, scaledTileSize.width);
      int tileY = floor_div(y - newBufferOrigin.y, scaledTileSize.height);
      int index = TileIndex(tileX, tileY);
      MOZ_ASSERT(index >= 0 &&
                 static_cast<unsigned>(index) < newRetainedTiles.Length(),
                 "index out of range");

      Tile newTile = newRetainedTiles[index];

      // Try to reuse a tile from the old retained tiles that had no partially
      // valid content.
      while (newTile.IsPlaceholderTile() && oldRetainedTiles.Length() > 0) {
        AsDerived().SwapTiles(newTile, oldRetainedTiles[oldRetainedTiles.Length()-1]);
        oldRetainedTiles.RemoveElementAt(oldRetainedTiles.Length()-1);
        if (!newTile.IsPlaceholderTile()) {
          oldTileCount--;
        }
      }

      // We've done our best effort to recycle a tile but it can be null
      // in which case it's up to the derived class's ValidateTile()
      // implementation to allocate a new tile before drawing
      nsIntPoint tileOrigin(tileStartX, tileStartY);
      newTile = AsDerived().ValidateTile(newTile, nsIntPoint(tileStartX, tileStartY),
                                         tileDrawRegion);
      NS_ASSERTION(!newTile.IsPlaceholderTile(), "Unexpected placeholder tile - failed to allocate?");
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
      printf_stderr("Store Validate tile %i, %i -> %i\n", tileStartX, tileStartY, index);
#endif
      newRetainedTiles[index] = newTile;

      y += height;
    }

    x += width;
  }

  AsDerived().PostValidate(aPaintRegion);
  for (unsigned int i = 0; i < newRetainedTiles.Length(); ++i) {
    AsDerived().UnlockTile(newRetainedTiles[i]);
  }

#ifdef GFX_TILEDLAYER_RETAINING_LOG
  { // scope ss
    std::stringstream ss;
    ss << "TiledLayerBuffer " << this << " finished pass 2 of update;"
       << " oldTileCount=" << oldTileCount << "\n";
    for (size_t i = 0; i < oldRetainedTiles.Length(); i++) {
      ss << "oldRetainedTiles[" << i << "] = ";
      oldRetainedTiles[i].Dump(ss);
      ss << "\n";
    }
    for (size_t i = 0; i < newRetainedTiles.Length(); i++) {
      ss << "newRetainedTiles[" << i << "] = ";
      newRetainedTiles[i].Dump(ss);
      ss << "\n";
    }
    print_stderr(ss);
  }
#endif

  // At this point, oldTileCount should be zero
  MOZ_ASSERT(oldTileCount == 0, "Failed to release old tiles");

  mRetainedTiles = newRetainedTiles;
  mValidRegion = newValidRegion;

  mTiles.mFirst.x = floor_div(mValidRegion.GetBounds().x, scaledTileSize.width);
  mTiles.mFirst.y = floor_div(mValidRegion.GetBounds().y, scaledTileSize.height);

  mPaintedRegion.Or(mPaintedRegion, aPaintRegion);
}

} // layers
} // mozilla

#endif // GFX_TILEDLAYERBUFFER_H
