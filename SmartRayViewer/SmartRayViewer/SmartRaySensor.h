#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstring>
#include <limits>

#include "Types.h"

// SmartRay SDK
#include "SR_API_public.h"
#include "SR_API_Defines.h"
#include "sr_api_errorcodes.h"

// ============================================================================
// SmartRaySensor
// - SmartRay SDK 래퍼
// - Connect()에서 heavy 설정(캘리브/파라파일/기본 파라미터) 1회 수행
// - Start()에서 lightweight 변경(Profiles/XScale/Exposure/Brightness) 반영
// - SDK PointCloud Callback 수신 → (옵션) merge → FrameCallback으로 전달
//
// 특징
// - SDK 초기화/해제는 static ref-count로 관리
// - PointCloud callback 등록은 1회만 수행
// - cam_index로 instance를 찾기 위해 static map 사용
// ============================================================================

struct SensorBaseConfig
{
    std::string parFile;          // optional
    uint32_t packetSize = 0;      // 0이면 SDK default
    uint32_t packetTimeoutMs = 1000;
    bool metaDataExport = true;
    ImageAquisitionType acqType = ImageAquisitionType_PointCloud;
};

struct SensorRuntimeConfig
{
    uint32_t profilesToCapture = 200;
    uint32_t mergeExpectedProfiles = 0;
    float transportResolution = 0.019f;

    // ✅ 채널별(최소 2채널까지 고려)
    int32_t exposure[2] = { 100, 100 };
    int32_t brightnessTh[2] = { 10, 10 };

    // trigger
    uint32_t triggerMode = 0;
    uint32_t triggerFrequency = 25;
    uint32_t triggerSource = 0;
    uint32_t triggerDivider = 1;
    uint32_t triggerDelay = 0;
    uint32_t triggerDirection = 0;
};

class SmartRaySensor
{
public:
    using FrameCallback = std::function<void(std::shared_ptr<PcFrame>)>;

public:
    // -----------------------------
    // lifecycle
    // -----------------------------
    SmartRaySensor();
    ~SmartRaySensor();

    SmartRaySensor(const SmartRaySensor&) = delete;
    SmartRaySensor& operator=(const SmartRaySensor&) = delete;

public:
    // -----------------------------
    // basic configure
    // -----------------------------
    void Configure(const std::string& name,
        const std::string& ip,
        unsigned short port,
        int camIndex);

    // Connect()에서 1회 heavy config, Start()에서 lightweight
    void SetParFilePath(const std::string& path) { m_parFile = path; }

    // 센서 설정 (프로파일 캡쳐 수 & merge 목표에 같이 사용 가능)
    void SetProfilesToCapture(uint32_t profiles) { m_profilesToCapture.store(profiles); }
    void SetMergeExpectedProfiles(uint32_t expectedProfiles) { m_mergeExpectedProfiles.store(expectedProfiles); } // 0이면 merge off

    // transport resolution (xScale로 명명했지만 SDK의 transport res로 설정)
    void SetXScale(float xScale) { m_xScaleToSet.store(xScale); }

    // 런타임 파라미터
    void SetExposureTime(int32_t exposure, int channel = 0);
    void SetLaserLineBrightnessThreshold(int32_t th, int channel = 0);

    void SetTriggerMode(uint32_t InputData) { m_TriggerMode.store(InputData); }
    void SetTriggerFrequency(uint32_t InputData) { m_TriggerFrequency.store(InputData); }
    void SetTriggerSource(uint32_t InputData) { m_TriggerSource.store(InputData); }
    void SetTriggerDivider(uint32_t InputData) { m_TriggerDivider.store(InputData); }
    void SetTriggerDelay(uint32_t InputData) { m_TriggerDelay.store(InputData); }
    void SetTriggerDirection(uint32_t InputData) { m_TriggerDirection.store(InputData); }

    // callback
    void SetFrameCallback(FrameCallback cb);

public:
    // -----------------------------
    // connection / acquisition
    // -----------------------------
    bool Connect(int timeoutS = 60);
    void Disconnect();

    bool Start();
    void Stop();

    bool IsConnected() const { return m_connected.load(); }
    bool IsRunning()   const { return m_running.load(); }

public:
    // -----------------------------
    // misc / info
    // -----------------------------
    // thickness SDK가 sensor handle을 요구할 때 외부에서 접근할 수 있도록 제공
    const SRSensor* GetHandle() const { return m_sensor; }

    int         GetLastErrorCode() const;
    std::string GetLastErrorText() const;

    // 검사 시작/재시작 시 호출: frame counter + merge buffer 초기화
    void ResetRunState(bool resetFrameCounter = true, bool resetMerge = true);

private:
    // =========================================================================
    // Internal: one-time configuration
    // =========================================================================
    bool EnsureConfiguredOnce();
    bool SetBaseConfig();

    // =========================================================================
    // Internal: runtime apply (Start 직전 변경 반영)
    // =========================================================================
    bool SetRuntimeConfig();
    bool SetProfilesIfChanged(uint32_t profiles);
    bool SetXScaleIfChanged(float xScale);
    bool SetExposureIfChanged(int32_t exposure, int channel);
    bool SetBrightnessThresholdIfChanged(int32_t th, int channel);
    bool SetTriggerIfChanged();

    void ReadBackAppliedConfig();

private:
    // =========================================================================
    // SDK init / callback registration (static)
    // =========================================================================
    static bool EnsureApiInitialized();
    static void ReleaseApiIfNeeded();
    static bool RegisterPointCloudCallbackOnce();

    static int CallbackPointCloud(
        SRSensor* sensor,
        ImageDataType dattyp,
        uint32_t numPoints,
        uint32_t numProfile,
        SR_3DPOINT* point_cloud,
        unsigned short* intensity,
        unsigned short* laserlinethickness,
        unsigned int* profileIdx,
        unsigned int* columnIdx,
        uint32_t numExtData,
        void* extData
    );

    static int UnknownCommandCallback(SRSensor* sensor);

private:
    // =========================================================================
    // instance map (cam_index → object)
    // =========================================================================
    void RegisterInstance();
    void UnregisterInstance();

private:
    // =========================================================================
    // callback/merge helpers
    // =========================================================================
    bool HandleReturnCode(int rc, const char* where);
    void DispatchFrame(std::shared_ptr<PcFrame> frame);

    void ResetMergeBuffer();

private:
    // =========================================================================
    // sensor identity / handle
    // =========================================================================
    SRSensor* m_sensor = nullptr;

    std::string m_name, m_ip;
    unsigned short m_port = 0;
    int m_camIndex = 0;

    std::string m_parFile;

private:
    // =========================================================================
    // state flags
    // =========================================================================
    std::atomic<bool> m_connected{ false };
    std::atomic<bool> m_running{ false };

    // heavy config 1회 보장용
    std::mutex m_cfgMtx;
    std::atomic<bool> m_configuredOnce{ false };

private:
    // =========================================================================
    // parameters to set (external) + sensor-applied cache (internal)
    // =========================================================================
    // 설정값(외부에서 변경)
    std::atomic<uint32_t> m_profilesToCapture{ 200 };
    std::atomic<uint32_t> m_mergeExpectedProfiles{ 0 };
    std::atomic<float>    m_xScaleToSet{ 0.019f };

    std::atomic<int32_t> m_exposureToSet{ 100 };
    std::atomic<int32_t> m_brightnessThToSet{ 10 };
    std::atomic<int>     m_paramChannel{ 0 };

    std::atomic<uint32_t> m_TriggerMode{ 0 };
    std::atomic<uint32_t> m_TriggerFrequency{ 25 };
    std::atomic<uint32_t> m_TriggerSource{ 0 };
    std::atomic<uint32_t> m_TriggerDivider{ 1 };
    std::atomic<uint32_t> m_TriggerDelay{ 0 };
    std::atomic<uint32_t> m_TriggerDirection{ 0 };

    // 센서에 실제 적용된 값(로컬 캐시)
    uint32_t m_sensorConfiguredProfiles = 0;
    float    m_sensorConfiguredXScale = -1.0f;
    uint32_t m_packetSize = 0;
    uint32_t m_packetTimeout = 0;

    int32_t m_sensorConfiguredExposureTime = std::numeric_limits<int32_t>::min();
    int32_t m_sensorConfiguredBrightnessTh = std::numeric_limits<int32_t>::min();

private:
    // =========================================================================
    // merge buffer
    // =========================================================================
    std::mutex m_accMtx;
    uint32_t m_accProfiles = 0;
    std::vector<SR_3DPOINT> m_accPoints;
    std::vector<unsigned short> m_accIntensity;

private:
    // =========================================================================
    // user callback
    // =========================================================================
    std::mutex m_cbMtx;
    FrameCallback m_frameCb;

    // 프레임 번호(센서 콜백마다 증가)
    std::atomic<uint64_t> m_frameCounter{ 0 };

private:
    // =========================================================================
    // error
    // =========================================================================
    mutable std::mutex m_errMtx;
    int m_lastErr = 0;
    std::string m_lastErrText;

    // API ref held (이 인스턴스가 SR_API_Initalize 참조 중인지)
    bool m_apiRefHeld = false;

private:
    // =========================================================================
    // statics (API + instance map)
    // =========================================================================
    static std::mutex s_apiMtx;
    static int  s_apiRefCount;
    static bool s_apiInited;
    static std::atomic<bool> s_pcCbRegistered;

    static std::mutex s_instMtx;
    static std::map<int, SmartRaySensor*> s_instances;
};
