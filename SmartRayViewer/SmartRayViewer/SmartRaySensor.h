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

class SmartRaySensor
{
public:
    using FrameCallback = std::function<void(std::shared_ptr<PcFrame>)>;

    SmartRaySensor();
    ~SmartRaySensor();

    SmartRaySensor(const SmartRaySensor&) = delete;
    SmartRaySensor& operator=(const SmartRaySensor&) = delete;

public:
    void Configure(const std::string& name,
        const std::string& ip,
        unsigned short port,
        int camIndex);

    // Connect에서 1회 heavy config, Start에서 lightweight
    void SetParFilePath(const std::string& path) { m_parFile = path; }

    // 여기 값은 센서 설정(프로파일 캡쳐 수) & 머지 목표에 같이 사용
    void SetProfilesToCapture(uint32_t profiles) { m_profilesToCapture.store(profiles); }
    void SetXScale(float xScale) { m_xScaleToSet.store(xScale); }

    // 0이면 머지 OFF (chunk 그대로 전달)
    void SetMergeExpectedProfiles(uint32_t expectedProfiles) { m_mergeExpectedProfiles.store(expectedProfiles); }

    void SetFrameCallback(FrameCallback cb);

    bool Connect(int timeoutS = 60);
    void Disconnect();

    bool Start();
    void Stop();

    bool IsConnected() const { return m_connected.load(); }
    bool IsRunning()   const { return m_running.load(); }

    // Thickness 라이브러리가 sensor handle을 요구하면 외부에서 가져가도록 제공
    const SRSensor* GetHandle() const { return m_sensor; }

    int         GetLastErrorCode() const;
    std::string GetLastErrorText() const;

private:
    bool EnsureConfiguredOnce();
    bool ConfigurePointCloud_Once(uint32_t profiles);
    bool ApplyProfilesIfChanged(uint32_t profiles);
    bool ApplyXScaleIfChanged(float xScale);

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

    void RegisterInstance();
    void UnregisterInstance();

    bool HandleReturnCode(int rc, const char* where);
    void DispatchFrame(std::shared_ptr<PcFrame> frame);
    void ResetMergeBuffer();

private:
    SRSensor* m_sensor = nullptr;

    std::string m_name, m_ip;
    unsigned short m_port = 0;
    int m_camIndex = 0;

    std::string m_parFile;

    std::atomic<bool> m_connected{ false };
    std::atomic<bool> m_running{ false };

    std::mutex m_cfgMtx;
    std::atomic<bool> m_configuredOnce{ false };

    // 설정/머지 관련
    std::atomic<uint32_t> m_profilesToCapture{ 200 };
    std::atomic<uint32_t> m_mergeExpectedProfiles{ 0 };
    std::atomic<float> m_xScaleToSet{ 0.019f };

    uint32_t m_sensorConfiguredProfiles = 0; // 실제 센서에 적용된 값(로컬 캐시)
    float m_sensorConfiguredXScale = -1.0f;
    uint32_t m_packetSize = 0;
    uint32_t m_packetTimeout = 0;

    // merge buffer
    std::mutex m_accMtx;
    uint32_t m_accProfiles = 0;
    std::vector<SR_3DPOINT> m_accPoints;
    std::vector<unsigned short> m_accIntensity;

    // callback
    std::mutex m_cbMtx;
    FrameCallback m_frameCb;

    std::atomic<uint64_t> m_frameCounter{ 0 };

    // error
    mutable std::mutex m_errMtx;
    int m_lastErr = 0;
    std::string m_lastErrText;

    bool m_apiRefHeld = false;

private:
    // statics
    static std::mutex s_apiMtx;
    static int  s_apiRefCount;
    static bool s_apiInited;
    static std::atomic<bool> s_pcCbRegistered;

    static std::mutex s_instMtx;
    static std::map<int, SmartRaySensor*> s_instances;
};
