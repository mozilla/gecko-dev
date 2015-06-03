#ifndef __CloudStorageTester__h
#define __CloudStorageTester__h

#include "nsString.h"
#include "fuse.h"
#include "nsDataHashtable.h"

namespace mozilla {
namespace system {
namespace cloudstorage {

class CloudStorageTester final
{
public:
  CloudStorageTester();
  ~CloudStorageTester();

  FuseAttr GetAttrByPath(const nsCString& aPath, const uint64_t aNodeId);
  void GetEntry(const nsCString& aPath, const uint64_t aOffset, nsCString& aEntryName, uint32_t& aEntryType);
  void GetData(const uint64_t aHandle, const uint32_t aSize, const uint64_t aOffset, char*& aData, int32_t& aActualSize);
  void Open(const nsCString& aPath, const uint64_t aHandle);
  void Close(const uint64_t aHandle);

private:
  static nsDataHashtable<nsUint64HashKey, int> sFileHash;
};

}
}
}
#endif
