/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pthread.h>
#include <hardware/gps.h>

#include "GonkGPSGeolocationProvider.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsGeoPosition.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINetworkManager.h"
#include "nsIObserverService.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "prtime.h"

#ifdef MOZ_B2G_RIL
#include "nsIDOMIccInfo.h"
#include "nsIMobileConnectionInfo.h"
#include "nsIMobileCellInfo.h"
#include "nsIRadioInterfaceLayer.h"
#endif

#define SETTING_DEBUG_ENABLED "geolocation.debugging.enabled"

#ifdef AGPS_TYPE_INVALID
#define AGPS_HAVE_DUAL_APN
#endif

#define FLUSH_AIDE_DATA 0

using namespace mozilla;

static const int kDefaultPeriod = 1000; // ms
static int gGPSDebugging = false;

static const char* kNetworkConnStateChangedTopic = "network-connection-state-changed";

// While most methods of GonkGPSGeolocationProvider should only be
// called from main thread, we deliberately put the Init and ShutdownGPS
// methods off main thread to avoid blocking.
NS_IMPL_ISUPPORTS(GonkGPSGeolocationProvider,
                  nsIGeolocationProvider,
                  nsIObserver,
                  nsISettingsServiceCallback)

/* static */ GonkGPSGeolocationProvider* GonkGPSGeolocationProvider::sSingleton = nullptr;
GpsCallbacks GonkGPSGeolocationProvider::mCallbacks = {
  sizeof(GpsCallbacks),
  LocationCallback,
  StatusCallback,
  SvStatusCallback,
  NmeaCallback,
  SetCapabilitiesCallback,
  AcquireWakelockCallback,
  ReleaseWakelockCallback,
  CreateThreadCallback,
#ifdef GPS_CAPABILITY_ON_DEMAND_TIME
  RequestUtcTimeCallback,
#endif
};

#ifdef MOZ_B2G_RIL
AGpsCallbacks
GonkGPSGeolocationProvider::mAGPSCallbacks = {
  AGPSStatusCallback,
  CreateThreadCallback,
};

AGpsRilCallbacks
GonkGPSGeolocationProvider::mAGPSRILCallbacks = {
  AGPSRILSetIDCallback,
  AGPSRILRefLocCallback,
  CreateThreadCallback,
};
#endif // MOZ_B2G_RIL

void
GonkGPSGeolocationProvider::LocationCallback(GpsLocation* location)
{
  class UpdateLocationEvent : public nsRunnable {
  public:
    UpdateLocationEvent(nsGeoPosition* aPosition)
      : mPosition(aPosition)
    {}
    NS_IMETHOD Run() {
      nsRefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();
      provider->mLastGPSDerivedLocationTime = PR_Now();
      nsCOMPtr<nsIGeolocationUpdate> callback = provider->mLocationCallback;
      if (callback) {
        callback->Update(mPosition);
      }
      return NS_OK;
    }
  private:
    nsRefPtr<nsGeoPosition> mPosition;
  };

  MOZ_ASSERT(location);

  nsRefPtr<nsGeoPosition> somewhere = new nsGeoPosition(location->latitude,
                                                        location->longitude,
                                                        location->altitude,
                                                        location->accuracy,
                                                        location->accuracy,
                                                        location->bearing,
                                                        location->speed,
                                                        location->timestamp);
  NS_DispatchToMainThread(new UpdateLocationEvent(somewhere));
}

void
GonkGPSGeolocationProvider::StatusCallback(GpsStatus* status)
{
}

void
GonkGPSGeolocationProvider::SvStatusCallback(GpsSvStatus* sv_info)
{
}

void
GonkGPSGeolocationProvider::NmeaCallback(GpsUtcTime timestamp, const char* nmea, int length)
{
  if (gGPSDebugging) {
    nsContentUtils::LogMessageToConsole("NMEA: timestamp:\t%lld", timestamp);
    nsContentUtils::LogMessageToConsole("NMEA: nmea:     \t%s", nmea);
    nsContentUtils::LogMessageToConsole("NMEA  length:   \%d", length);
  }
}

void
GonkGPSGeolocationProvider::SetCapabilitiesCallback(uint32_t capabilities)
{
  class UpdateCapabilitiesEvent : public nsRunnable {
  public:
    UpdateCapabilitiesEvent(uint32_t aCapabilities)
      : mCapabilities(aCapabilities)
    {}
    NS_IMETHOD Run() {
      nsRefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();

      provider->mSupportsScheduling = mCapabilities & GPS_CAPABILITY_SCHEDULING;
#ifdef MOZ_B2G_RIL
      provider->mSupportsMSB = mCapabilities & GPS_CAPABILITY_MSB;
      provider->mSupportsMSA = mCapabilities & GPS_CAPABILITY_MSA;
#endif
      provider->mSupportsSingleShot = mCapabilities & GPS_CAPABILITY_SINGLE_SHOT;
#ifdef GPS_CAPABILITY_ON_DEMAND_TIME
      provider->mSupportsTimeInjection = mCapabilities & GPS_CAPABILITY_ON_DEMAND_TIME;
#endif
      return NS_OK;
    }
  private:
    uint32_t mCapabilities;
  };

  NS_DispatchToMainThread(new UpdateCapabilitiesEvent(capabilities));
}

void
GonkGPSGeolocationProvider::AcquireWakelockCallback()
{
}

void
GonkGPSGeolocationProvider::ReleaseWakelockCallback()
{
}

typedef void *(*pthread_func)(void *);

/** Callback for creating a thread that can call into the JS codes.
 */
pthread_t
GonkGPSGeolocationProvider::CreateThreadCallback(const char* name, void (*start)(void *), void* arg)
{
  pthread_t thread;
  pthread_attr_t attr;

  pthread_attr_init(&attr);

  /* Unfortunately pthread_create and the callback disagreed on what
   * start function should return.
   */
  pthread_create(&thread, &attr, reinterpret_cast<pthread_func>(start), arg);

  return thread;
}

void
GonkGPSGeolocationProvider::RequestUtcTimeCallback()
{
}

#ifdef MOZ_B2G_RIL
void
GonkGPSGeolocationProvider::AGPSStatusCallback(AGpsStatus* status)
{
  MOZ_ASSERT(status);

  class AGPSStatusEvent : public nsRunnable {
  public:
    AGPSStatusEvent(AGpsStatusValue aStatus)
      : mStatus(aStatus)
    {}
    NS_IMETHOD Run() {
      nsRefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();

      switch (mStatus) {
        case GPS_REQUEST_AGPS_DATA_CONN:
          provider->RequestDataConnection();
          break;
        case GPS_RELEASE_AGPS_DATA_CONN:
          provider->ReleaseDataConnection();
          break;
      }
      return NS_OK;
    }
  private:
    AGpsStatusValue mStatus;
  };

  NS_DispatchToMainThread(new AGPSStatusEvent(status->status));
}

void
GonkGPSGeolocationProvider::AGPSRILSetIDCallback(uint32_t flags)
{
  class RequestSetIDEvent : public nsRunnable {
  public:
    RequestSetIDEvent(uint32_t flags)
      : mFlags(flags)
    {}
    NS_IMETHOD Run() {
      nsRefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();
      provider->RequestSetID(mFlags);
      return NS_OK;
    }
  private:
    uint32_t mFlags;
  };

  NS_DispatchToMainThread(new RequestSetIDEvent(flags));
}

void
GonkGPSGeolocationProvider::AGPSRILRefLocCallback(uint32_t flags)
{
  class RequestRefLocEvent : public nsRunnable {
  public:
    RequestRefLocEvent()
    {}
    NS_IMETHOD Run() {
      nsRefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();
      provider->SetReferenceLocation();
      return NS_OK;
    }
  };

  if (flags & AGPS_RIL_REQUEST_REFLOC_CELLID) {
    NS_DispatchToMainThread(new RequestRefLocEvent());
  }
}
#endif // MOZ_B2G_RIL

GonkGPSGeolocationProvider::GonkGPSGeolocationProvider()
  : mStarted(false)
  , mSupportsScheduling(false)
#ifdef MOZ_B2G_RIL
  , mSupportsMSB(false)
  , mSupportsMSA(false)
#endif
  , mSupportsSingleShot(false)
  , mSupportsTimeInjection(false)
  , mGpsInterface(nullptr)
{
}

GonkGPSGeolocationProvider::~GonkGPSGeolocationProvider()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mStarted, "Must call Shutdown before destruction");

  sSingleton = nullptr;
}

already_AddRefed<GonkGPSGeolocationProvider>
GonkGPSGeolocationProvider::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton)
    sSingleton = new GonkGPSGeolocationProvider();

  nsRefPtr<GonkGPSGeolocationProvider> provider = sSingleton;
  return provider.forget();
}

const GpsInterface*
GonkGPSGeolocationProvider::GetGPSInterface()
{
  hw_module_t* module;

  if (hw_get_module(GPS_HARDWARE_MODULE_ID, (hw_module_t const**)&module))
    return nullptr;

  hw_device_t* device;
  if (module->methods->open(module, GPS_HARDWARE_MODULE_ID, &device))
    return nullptr;

  gps_device_t* gps_device = (gps_device_t *)device;
  const GpsInterface* result = gps_device->get_gps_interface(gps_device);

  if (result->size != sizeof(GpsInterface)) {
    return nullptr;
  }
  return result;
}

#ifdef MOZ_B2G_RIL
int32_t
GonkGPSGeolocationProvider::GetDataConnectionState()
{
  if (!mRadioInterface) {
    return nsINetworkInterface::NETWORK_STATE_UNKNOWN;
  }

  int32_t state;
  mRadioInterface->GetDataCallStateByType(NS_LITERAL_STRING("supl"), &state);
  return state;
}

void
GonkGPSGeolocationProvider::SetAGpsDataConn(nsAString& aApn)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mAGpsInterface);

  int32_t connectionState = GetDataConnectionState();
  if (connectionState == nsINetworkInterface::NETWORK_STATE_CONNECTED) {
    NS_ConvertUTF16toUTF8 apn(aApn);
#ifdef AGPS_HAVE_DUAL_APN
    mAGpsInterface->data_conn_open(AGPS_TYPE_SUPL,
                                   apn.get(),
                                   AGPS_APN_BEARER_IPV4);
#else
    mAGpsInterface->data_conn_open(apn.get());
#endif
  } else if (connectionState == nsINetworkInterface::NETWORK_STATE_DISCONNECTED) {
#ifdef AGPS_HAVE_DUAL_APN
    mAGpsInterface->data_conn_closed(AGPS_TYPE_SUPL);
#else
    mAGpsInterface->data_conn_closed();
#endif
  }
}

#endif // MOZ_B2G_RIL

void
GonkGPSGeolocationProvider::RequestSettingValue(char* aKey)
{
  MOZ_ASSERT(aKey);
  nsCOMPtr<nsISettingsService> ss = do_GetService("@mozilla.org/settingsService;1");
  if (!ss) {
    MOZ_ASSERT(ss);
    return;
  }
  nsCOMPtr<nsISettingsServiceLock> lock;
  ss->CreateLock(nullptr, getter_AddRefs(lock));
  lock->Get(aKey, this);
}

#ifdef MOZ_B2G_RIL
void
GonkGPSGeolocationProvider::RequestDataConnection()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mRadioInterface) {
    return;
  }

  if (GetDataConnectionState() == nsINetworkInterface::NETWORK_STATE_CONNECTED) {
    // Connection is already established, we don't need to setup again.
    // We just get supl APN and make AGPS data connection state updated.
    RequestSettingValue("ril.supl.apn");
  } else {
    mRadioInterface->SetupDataCallByType(NS_LITERAL_STRING("supl"));
  }
}

void
GonkGPSGeolocationProvider::ReleaseDataConnection()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mRadioInterface) {
    return;
  }

  mRadioInterface->DeactivateDataCallByType(NS_LITERAL_STRING("supl"));
}

void
GonkGPSGeolocationProvider::RequestSetID(uint32_t flags)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mRadioInterface) {
    return;
  }

  AGpsSetIDType type = AGPS_SETID_TYPE_NONE;

  nsCOMPtr<nsIRilContext> rilCtx;
  mRadioInterface->GetRilContext(getter_AddRefs(rilCtx));

  if (rilCtx) {
    nsAutoString id;
    if (flags & AGPS_RIL_REQUEST_SETID_IMSI) {
      type = AGPS_SETID_TYPE_IMSI;
      rilCtx->GetImsi(id);
    }

    if (flags & AGPS_RIL_REQUEST_SETID_MSISDN) {
      nsCOMPtr<nsIDOMMozIccInfo> iccInfo;
      rilCtx->GetIccInfo(getter_AddRefs(iccInfo));
      if (iccInfo) {
        nsCOMPtr<nsIDOMMozGsmIccInfo> gsmIccInfo = do_QueryInterface(iccInfo);
        if (gsmIccInfo) {
          type = AGPS_SETID_TYPE_MSISDN;
          gsmIccInfo->GetMsisdn(id);
        }
      }
    }

    NS_ConvertUTF16toUTF8 idBytes(id);
    mAGpsRilInterface->set_set_id(type, idBytes.get());
  }
}

void
GonkGPSGeolocationProvider::SetReferenceLocation()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mRadioInterface) {
    return;
  }

  nsCOMPtr<nsIRilContext> rilCtx;
  mRadioInterface->GetRilContext(getter_AddRefs(rilCtx));

  AGpsRefLocation location;

  // TODO: Bug 772750 - get mobile connection technology from rilcontext
  location.type = AGPS_REF_LOCATION_TYPE_UMTS_CELLID;

  if (rilCtx) {
    nsCOMPtr<nsIDOMMozIccInfo> iccInfo;
    rilCtx->GetIccInfo(getter_AddRefs(iccInfo));
    if (iccInfo) {
      nsresult result;
      nsAutoString mcc, mnc;

      iccInfo->GetMcc(mcc);
      iccInfo->GetMnc(mnc);

      location.u.cellID.mcc = mcc.ToInteger(&result);
      if (result != NS_OK) {
        NS_WARNING("Cannot parse mcc to integer");
        location.u.cellID.mcc = 0;
      }

      location.u.cellID.mnc = mnc.ToInteger(&result);
      if (result != NS_OK) {
        NS_WARNING("Cannot parse mnc to integer");
        location.u.cellID.mnc = 0;
      }
    }
    nsCOMPtr<nsIMobileConnectionInfo> voice;
    rilCtx->GetVoice(getter_AddRefs(voice));
    if (voice) {
      nsCOMPtr<nsIMobileCellInfo> cell;
      voice->GetCell(getter_AddRefs(cell));
      if (cell) {
        int32_t lac;
        int64_t cid;

        cell->GetGsmLocationAreaCode(&lac);
        // The valid range of LAC is 0x0 to 0xffff which is defined in
        // hardware/ril/include/telephony/ril.h
        if (lac >= 0x0 && lac <= 0xffff) {
          location.u.cellID.lac = lac;
        }

        cell->GetGsmCellId(&cid);
        // The valid range of cell id is 0x0 to 0xffffffff which is defined in
        // hardware/ril/include/telephony/ril.h
        if (cid >= 0x0 && cid <= 0xffffffff) {
          location.u.cellID.cid = cid;
        }
      }
    }
    if (mAGpsRilInterface) {
      mAGpsRilInterface->set_ref_location(&location, sizeof(location));
    }
  }
}

#endif // MOZ_B2G_RIL

void
GonkGPSGeolocationProvider::InjectLocation(double latitude,
                                           double longitude,
                                           float accuracy)
{
  if (gGPSDebugging) {
    nsContentUtils::LogMessageToConsole("*** injecting location");
    nsContentUtils::LogMessageToConsole("*** lat: %f", latitude);
    nsContentUtils::LogMessageToConsole("*** lon: %f", longitude);
    nsContentUtils::LogMessageToConsole("*** accuracy: %f", accuracy);
  }
  
  MOZ_ASSERT(NS_IsMainThread());
  if (!mGpsInterface) {
    return;
  }

  mGpsInterface->inject_location(latitude, longitude, accuracy);
}

void
GonkGPSGeolocationProvider::Init()
{
  // Must not be main thread. Some GPS driver's first init takes very long.
  MOZ_ASSERT(!NS_IsMainThread());

  mGpsInterface = GetGPSInterface();
  if (!mGpsInterface) {
    return;
  }

  if (mGpsInterface->init(&mCallbacks) != 0) {
    return;
  }

#ifdef MOZ_B2G_RIL
  mAGpsInterface =
    static_cast<const AGpsInterface*>(mGpsInterface->get_extension(AGPS_INTERFACE));
  if (mAGpsInterface) {
    mAGpsInterface->init(&mAGPSCallbacks);
  }

  mAGpsRilInterface =
    static_cast<const AGpsRilInterface*>(mGpsInterface->get_extension(AGPS_RIL_INTERFACE));
  if (mAGpsRilInterface) {
    mAGpsRilInterface->init(&mAGPSRILCallbacks);
  }
#endif

  NS_DispatchToMainThread(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::StartGPS));
}

void
GonkGPSGeolocationProvider::StartGPS()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGpsInterface);

  int32_t update = Preferences::GetInt("geo.default.update", kDefaultPeriod);

#ifdef MOZ_B2G_RIL
  if (mSupportsMSA || mSupportsMSB) {
    SetupAGPS();
  }
#endif

  int positionMode = GPS_POSITION_MODE_STANDALONE;
  bool singleShot = false;

#ifdef MOZ_B2G_RIL
  // XXX: If we know this is a single shot request, use MSA can be faster.
  if (singleShot && mSupportsMSA) {
    positionMode = GPS_POSITION_MODE_MS_ASSISTED;
  } else if (mSupportsMSB) {
    positionMode = GPS_POSITION_MODE_MS_BASED;
  }
#endif
  if (!mSupportsScheduling) {
    update = kDefaultPeriod;
  }

  mGpsInterface->set_position_mode(positionMode,
                                   GPS_POSITION_RECURRENCE_PERIODIC,
                                   update, 0, 0);
#if FLUSH_AIDE_DATA
  // Delete cached data
  mGpsInterface->delete_aiding_data(GPS_DELETE_ALL);
#endif

  mGpsInterface->start();
}

#ifdef MOZ_B2G_RIL
void
GonkGPSGeolocationProvider::SetupAGPS()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mAGpsInterface);

  const nsAdoptingCString& suplServer = Preferences::GetCString("geo.gps.supl_server");
  int32_t suplPort = Preferences::GetInt("geo.gps.supl_port", -1);
  if (!suplServer.IsEmpty() && suplPort > 0) {
    mAGpsInterface->set_server(AGPS_TYPE_SUPL, suplServer.get(), suplPort);
  } else {
    NS_WARNING("Cannot get SUPL server settings");
    return;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, kNetworkConnStateChangedTopic, false);
  }

  nsCOMPtr<nsIRadioInterfaceLayer> ril = do_GetService("@mozilla.org/ril;1");
  if (ril) {
    // TODO: Bug 878748 - B2G GPS: acquire correct RadioInterface instance in
    // MultiSIM configuration
    ril->GetRadioInterface(0 /* clientId */, getter_AddRefs(mRadioInterface));
  }
}
#endif // MOZ_B2G_RIL


NS_IMPL_ISUPPORTS(GonkGPSGeolocationProvider::NetworkLocationUpdate,
                  nsIGeolocationUpdate)

NS_IMETHODIMP
GonkGPSGeolocationProvider::NetworkLocationUpdate::Update(nsIDOMGeoPosition *position)
{
  nsRefPtr<GonkGPSGeolocationProvider> provider =
    GonkGPSGeolocationProvider::GetSingleton();

  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  position->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return NS_ERROR_FAILURE;
  }

  double lat, lon, acc;
  coords->GetLatitude(&lat);
  coords->GetLongitude(&lon);
  coords->GetAccuracy(&acc);

  double delta = MAXFLOAT;

  static double sLastMLSPosLat = 0;
  static double sLastMLSPosLon = 0;

  if (0 != sLastMLSPosLon || 0 != sLastMLSPosLat) {
    // Use spherical law of cosines to calculate difference
    // Not quite as correct as the Haversine but simpler and cheaper
    // Should the following be a utility function? Others might need this calc.
    const double radsInDeg = 3.14159265 / 180.0;
    const double rNewLat = lat * radsInDeg;
    const double rNewLon = lon * radsInDeg;
    const double rOldLat = sLastMLSPosLat * radsInDeg;
    const double rOldLon = sLastMLSPosLon * radsInDeg;
    // WGS84 equatorial radius of earth = 6378137m
    delta = acos( (sin(rNewLat) * sin(rOldLat)) +
                  (cos(rNewLat) * cos(rOldLat) * cos(rOldLon - rNewLon)) )
                  * 6378137;
  }

  sLastMLSPosLat = lat;
  sLastMLSPosLon = lon;

  // if the MLS coord change is smaller than this arbitrarily small value
  // assume the MLS coord is unchanged, and stick with the GPS location
  const double kMinMLSCoordChangeInMeters = 10;

  // if we haven't seen anything from the GPS device for 10s,
  // use this network derived location.
  const int kMaxGPSDelayBeforeConsideringMLS = 10000;
  int64_t diff = PR_Now() - provider->mLastGPSDerivedLocationTime;
  if (provider->mLocationCallback && diff > kMaxGPSDelayBeforeConsideringMLS
      && delta > kMinMLSCoordChangeInMeters)
  {
    provider->mLocationCallback->Update(position);
  }

  provider->InjectLocation(lat, lon, acc);
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::NetworkLocationUpdate::LocationUpdatePending()
{
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::NetworkLocationUpdate::NotifyError(uint16_t error)
{
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Startup()
{
  MOZ_ASSERT(NS_IsMainThread());

  RequestSettingValue(SETTING_DEBUG_ENABLED);
  if (mStarted) {
    return NS_OK;
  }

  if (!mInitThread) {
    nsresult rv = NS_NewThread(getter_AddRefs(mInitThread));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mInitThread->Dispatch(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::Init),
                        NS_DISPATCH_NORMAL);

  mNetworkLocationProvider = do_CreateInstance("@mozilla.org/geolocation/mls-provider;1");
  if (mNetworkLocationProvider) {
    nsresult rv = mNetworkLocationProvider->Startup();
    if (NS_SUCCEEDED(rv)) {
      nsRefPtr<NetworkLocationUpdate> update = new NetworkLocationUpdate();
      mNetworkLocationProvider->Watch(update);
    }
  }

  mLastGPSDerivedLocationTime = 0;
  mStarted = true;
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Watch(nsIGeolocationUpdate* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  mLocationCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mStarted) {
    return NS_OK;
  }
  mStarted = false;
  if (mNetworkLocationProvider) {
    mNetworkLocationProvider->Shutdown();
    mNetworkLocationProvider = nullptr;
  }
#ifdef MOZ_B2G_RIL
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, kNetworkConnStateChangedTopic);
  }
#endif

  mInitThread->Dispatch(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::ShutdownGPS),
                        NS_DISPATCH_NORMAL);

  return NS_OK;
}

void
GonkGPSGeolocationProvider::ShutdownGPS()
{
  MOZ_ASSERT(!mStarted, "Should only be called after Shutdown");

  if (mGpsInterface) {
    mGpsInterface->stop();
    mGpsInterface->cleanup();
  }
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::SetHighAccuracy(bool)
{
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Observe(nsISupports* aSubject,
                                    const char* aTopic,
                                    const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

#ifdef MOZ_B2G_RIL
  if (!strcmp(aTopic, kNetworkConnStateChangedTopic)) {
    nsCOMPtr<nsIRilNetworkInterface> iface = do_QueryInterface(aSubject);
    if (!iface) {
      return NS_OK;
    }

    RequestSettingValue("ril.supl.apn");
  }
#endif

  return NS_OK;
}

/** nsISettingsServiceCallback **/

NS_IMETHODIMP
GonkGPSGeolocationProvider::Handle(const nsAString& aName,
                                   JS::Handle<JS::Value> aResult)
{
#ifdef MOZ_B2G_RIL
  if (aName.EqualsLiteral("ril.supl.apn")) {
    // When we get the APN, we attempt to call data_call_open of AGPS.
    if (aResult.isString()) {
      JSContext *cx = nsContentUtils::GetCurrentJSContext();
      NS_ENSURE_TRUE(cx, NS_OK);

      // NB: No need to enter a compartment to read the contents of a string.
      nsDependentJSString apn;
      apn.init(cx, aResult.toString());
      if (!apn.IsEmpty()) {
        SetAGpsDataConn(apn);
      }
    }
  } else
#endif // MOZ_B2G_RIL
  if (aName.EqualsLiteral(SETTING_DEBUG_ENABLED)) {
    gGPSDebugging = aResult.isBoolean() ? aResult.toBoolean() : false;
    return NS_OK;
  }
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::HandleError(const nsAString& aErrorMessage)
{
  return NS_OK;
}
