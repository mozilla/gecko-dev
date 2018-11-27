#if !defined(AgnosticDecoderModule_h_)
#define AgnosticDecoderModule_h_

#include "PlatformDecoderModule.h"

namespace mozilla {

class AgnosticDecoderModule : public PlatformDecoderModule {
 public:
  AgnosticDecoderModule() = default;

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;

 protected:
  virtual ~AgnosticDecoderModule() = default;
  // Decode thread.
  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override;

  // Decode thread.
  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override;
};

}  // namespace mozilla

#endif /* AgnosticDecoderModule_h_ */
