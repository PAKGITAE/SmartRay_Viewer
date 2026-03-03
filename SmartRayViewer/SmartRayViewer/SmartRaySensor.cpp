#include "pch.h"
#include "SmartRaySensor.h"

#include <sstream>
#include <iostream>
#include <climits>

// ============================================================================
// statics
// ============================================================================
std::mutex SmartRaySensor::s_apiMtx;
int  SmartRaySensor::s_apiRefCount = 0;
bool SmartRaySensor::s_apiInited = false;
std::atomic<bool> SmartRaySensor::s_pcCbRegistered{ false };

std::mutex SmartRaySensor::s_instMtx;
std::map<int, SmartRaySensor*> SmartRaySensor::s_instances;

// ============================================================================
// lifecycle
// ============================================================================
SmartRaySensor::SmartRaySensor()
{
    m_sensor = new SRSensor();
    std::memset(m_sensor, 0, sizeof(SRSensor));
    m_sensor->usercbf = &SmartRaySensor::UnknownCommandCallback;
}

SmartRaySensor::~SmartRaySensor()
{
    Disconnect();
    delete m_sensor;
    m_sensor = nullptr;
}

// ============================================================================
// public: configure / set params
// ============================================================================
void SmartRaySensor::Configure(const std::string& name,
    const std::string& ip,
    unsigned short port,
    int camIndex)
{
    m_name = name;
    m_ip = ip;
    m_port = port;
    m_camIndex = camIndex;

    std::memset(m_sensor, 0, sizeof(SRSensor));
    m_sensor->cam_index = m_camIndex;
    m_sensor->active = 0;

    strcpy_s(m_sensor->name, name.c_str());
    strcpy_s(m_sensor->IPAdr, ip.c_str());
    m_sensor->portnum = port;

    m_sensor->usercbf = &SmartRaySensor::UnknownCommandCallback;

    // 새 구성으로 다시 연결 시 heavy config 다시 하도록
    m_configuredOnce.store(false);

    // (선택) 상태 초기화
    ResetRunState(true, true);
}

void SmartRaySensor::SetExposureTime(int32_t exposure, int channel)
{
    m_exposureToSet.store(exposure);
    m_paramChannel.store(channel);
}

void SmartRaySensor::SetLaserLineBrightnessThreshold(int32_t th, int channel)
{
    m_brightnessThToSet.store(th);
    m_paramChannel.store(channel);
}

void SmartRaySensor::SetFrameCallback(FrameCallback cb)
{
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_frameCb = std::move(cb);
}

// ============================================================================
// static: SDK init / exit / register callback
// ============================================================================
bool SmartRaySensor::EnsureApiInitialized()
{
    std::lock_guard<std::mutex> lk(s_apiMtx);

    if (!s_apiInited)
    {
        int rc = SR_API_Initalize(nullptr);
        if (rc != SUCCESS)
            return false;

        s_apiInited = true;
        s_apiRefCount = 0;
    }

    if (s_apiRefCount == INT_MAX)
        return false;

    ++s_apiRefCount;
    return true;
}

void SmartRaySensor::ReleaseApiIfNeeded()
{
    std::lock_guard<std::mutex> lk(s_apiMtx);

    if (!s_apiInited) { s_apiRefCount = 0; return; }
    if (s_apiRefCount <= 0) { s_apiRefCount = 0; return; }

    --s_apiRefCount;

    if (s_apiRefCount == 0)
    {
        SR_API_Exit();
        s_apiInited = false;
    }
}

bool SmartRaySensor::RegisterPointCloudCallbackOnce()
{
    bool expected = false;
    if (s_pcCbRegistered.compare_exchange_strong(expected, true))
    {
        int ret = SR_API_RegisterPointCloudCB(&SmartRaySensor::CallbackPointCloud);
        if (ret != SUCCESS)
        {
            s_pcCbRegistered.store(false);
            return false;
        }
    }
    return true;
}

// ============================================================================
// public: connect / disconnect
// ============================================================================
bool SmartRaySensor::Connect(int timeoutS)
{
    if (m_connected.load()) return true;

    if (!m_apiRefHeld)
    {
        if (!EnsureApiInitialized())
            return false;
        m_apiRefHeld = true;
    }

    int rc = SR_API_ConnectSensor(m_sensor, timeoutS);
    if (rc != SUCCESS)
    {
        ReleaseApiIfNeeded();
        m_apiRefHeld = false;
        return HandleReturnCode(rc, "SR_API_ConnectSensor");
    }

    m_connected.store(true);
    RegisterInstance();

    if (!RegisterPointCloudCallbackOnce())
    {
        Disconnect();
        return false;
    }

    // ✅ Heavy 1회
    if (!EnsureConfiguredOnce())
    {
        Disconnect();
        return false;
    }

    return true;
}

void SmartRaySensor::Disconnect()
{
    Stop();

    if (m_connected.load())
    {
        SR_API_DisconnectSensor(m_sensor);
        m_connected.store(false);
        UnregisterInstance();
    }

    if (m_apiRefHeld)
    {
        ReleaseApiIfNeeded();
        m_apiRefHeld = false;
    }

    m_configuredOnce.store(false);
    ResetRunState(true, true);
}

// ============================================================================
// public: start / stop acquisition
// ============================================================================
bool SmartRaySensor::Start()
{
    if (!IsConnected()) return false;

    if (!EnsureConfiguredOnce())
        return false;

    // ✅ Start 직전에만 runtime apply
    if (!SetRuntimeConfig())
        return false;

    ResetRunState(true, true);

    int ret = SR_API_StartAcquisition(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_StartAcquisition");

    m_running.store(true);
    return true;
}

void SmartRaySensor::Stop()
{
    if (!m_running.load())
        return;

    int ret = SR_API_StopAcquisition(m_sensor);
    HandleReturnCode(ret, "SR_API_StopAcquisition");
    m_running.store(false);
}

// ============================================================================
// private: heavy config (Connect 후 1회)
// ============================================================================
bool SmartRaySensor::EnsureConfiguredOnce()
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;
    if (m_configuredOnce.load()) return true;

    if (!SetBaseConfig())
        return false;

    m_configuredOnce.store(true);

    // (선택) base 반영된 값 읽기
    ReadBackAppliedConfig();
    return true;
}


bool SmartRaySensor::SetBaseConfig()
{
    int ret = 0;

    ret = SR_API_LoadCalibrationDataFromSensor(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_LoadCalibrationDataFromSensor");

    if (!m_parFile.empty())
    {
        ret = SR_API_LoadParameterSetFromFile(m_sensor, m_parFile.c_str());
        if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_LoadParameterSetFromFile");
    }

    ret = SR_API_SetImageAcquisitionType(m_sensor, ImageAquisitionType_PointCloud);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetImageAcquisitionType(PointCloud)");

    // 패킷은 base로 두는게 안정적
    ret = SR_API_SetPacketSize(m_sensor, 0);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetPacketSize");

    ret = SR_API_SetPacketTimeOut(m_sensor, 1000);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetPacketTimeOut");

    // (선택)
    // ret = SR_API_SetMetaDataExportEnabled(m_sensor, true);

    //ret = SR_API_SetSmartXactMode(m_sensor, SR_API_SMARTXACT_MODE_DEFAULT);
    //if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetSmartXactMode");

    float sensorConfiguredXScale = 0.025;
    ret = SR_API_SetTransportResolution(m_sensor, sensorConfiguredXScale);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_GetTransportResolution");

    // ✅ base 변경 반영 확정
    ret = SR_API_SendParameterSetToSensor(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SendParameterSetToSensor");

    return true;
}


// ============================================================================
// private: runtime apply (Start 직전 변경 반영)
// ============================================================================
bool SmartRaySensor::SetRuntimeConfig()
{
    const uint32_t profiles = m_profilesToCapture.load();
    if (profiles > 0)
        if (!SetProfilesIfChanged(profiles)) return false;

    //const float xScale = m_xScaleToSet.load();
    //if (!SetXScaleIfChanged(xScale)) return false;

    //const int ch = m_paramChannel.load();

    //const int32_t exposure = m_exposureToSet.load();
    //if (!SetExposureIfChanged(exposure, ch)) return false;

    //const int32_t th = m_brightnessThToSet.load();
    //if (!SetBrightnessThresholdIfChanged(th, ch)) return false;

    //// ✅ trigger는 분리해서 마지막에
    //if (!SetTriggerIfChanged()) return false;

    // 변경 후 한번에 하자
    int ret = SR_API_SendParameterSetToSensor(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SendParameterSetToSensor");

    // 적용 후 읽기(선택)
    ReadBackAppliedConfig();
    return true;
}

bool SmartRaySensor::SetProfilesIfChanged(uint32_t profiles)
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;
    if (profiles == 0) return true;

    int ret = SR_API_SetNumberOfProfilesToCapture(m_sensor, profiles);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetNumberOfProfilesToCapture");

    return true;
}

bool SmartRaySensor::SetXScaleIfChanged(float xScale)
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;
    if (xScale <= 0.f) return true;

    int ret = SR_API_SetTransportResolution(m_sensor, xScale);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetTransportResolution(XScale)");

    return true;
}

bool SmartRaySensor::SetExposureIfChanged(int32_t exposure, int channel)
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;
    if (exposure <= 0) return true;

    int ret = SR_API_SetExposureTime(m_sensor, channel, exposure);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetExposureTime");

    return true;
}

bool SmartRaySensor::SetBrightnessThresholdIfChanged(int32_t th, int channel)
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;
    if (th < 0) return true;

    int ret = SR_API_Set3DLaserLineBrightnessThreshold(m_sensor, channel, th);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_Set3DLaserLineBrightnessThreshold");

    return true;
}

bool SmartRaySensor::SetTriggerIfChanged()
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;

    // mode
    DataTriggerMode mode = DataTriggerMode_FreeRunning;
    const uint32_t tm = m_TriggerMode.load();
    if (tm == 1) mode = DataTriggerMode_Internal;
    else if (tm == 2) mode = DataTriggerMode_External;

    int ret = SR_API_SetDataTriggerMode(m_sensor, mode);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetDataTriggerMode");

    // internal frequency
    ret = SR_API_SetDataTriggerInternalFrequency(m_sensor, m_TriggerFrequency.load());
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetDataTriggerInternalFrequency");

    // source
    DataTriggerSource src = DataTriggerSource_QuadEncoder;
    const uint32_t ts = m_TriggerSource.load();
    if (ts == 0) src = DataTriggerSource_Input1;
    else if (ts == 1) src = DataTriggerSource_Input2;
    else if (ts == 2) src = DataTriggerSource_Combined;
    else if (ts == 3) src = DataTriggerSource_QuadEncoder;

    ret = SR_API_SetDataTriggerExternalTriggerSource(m_sensor, src);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetDataTriggerExternalTriggerSource");

    // edge(direction)
    TriggerEdgeMode edge = TriggerEdgeMode_RisingEdge;
    const uint32_t td = m_TriggerDirection.load();
    if (td == 0) edge = TriggerEdgeMode_RisingEdge;
    else if (td == 1) edge = TriggerEdgeMode_FallingEdge;
    else if (td == 2) edge = TriggerEdgeMode_Both;

    ret = SR_API_SetDataTriggerExternalTriggerParameters(
        m_sensor,
        m_TriggerDivider.load(),
        m_TriggerDelay.load(),
        edge
    );
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetDataTriggerExternalTriggerParameters");

    return true;
}

void SmartRaySensor::ReadBackAppliedConfig()
{
    if (!IsConnected()) return;

    const int ch = m_paramChannel.load();

    SR_API_GetNumberOfProfilesToCapture(m_sensor, &m_sensorConfiguredProfiles);
    SR_API_GetTransportResolution(m_sensor, &m_sensorConfiguredXScale);
    SR_API_GetPacketSize(m_sensor, &m_packetSize);
    SR_API_GetPacketTimeOut(m_sensor, &m_packetTimeout);
    SR_API_Get3DLaserLineBrightnessThreshold(m_sensor, ch, &m_sensorConfiguredBrightnessTh);
    SR_API_GetExposureTime(m_sensor, ch, &m_sensorConfiguredExposureTime);
}

// ============================================================================
// private: run-state reset (frame counter / merge buffer)
// ============================================================================
void SmartRaySensor::ResetMergeBuffer()
{
    std::lock_guard<std::mutex> lk(m_accMtx);
    m_accProfiles = 0;
    m_accPoints.clear();
    m_accIntensity.clear();
}

void SmartRaySensor::ResetRunState(bool resetFrameCounter, bool resetMerge)
{
    if (resetFrameCounter)
        m_frameCounter.store(0);

    if (resetMerge)
        ResetMergeBuffer();
}

// ============================================================================
// private: instance map (cam_index → object)
// ============================================================================
void SmartRaySensor::RegisterInstance()
{
    std::lock_guard<std::mutex> lk(s_instMtx);
    s_instances[m_camIndex] = this;
}

void SmartRaySensor::UnregisterInstance()
{
    std::lock_guard<std::mutex> lk(s_instMtx);
    auto it = s_instances.find(m_camIndex);
    if (it != s_instances.end() && it->second == this)
        s_instances.erase(it);
}

// ============================================================================
// private: dispatch to user callback
// ============================================================================
void SmartRaySensor::DispatchFrame(std::shared_ptr<PcFrame> frame)
{
    if (!frame) return;

    // frameNo는 센서 인스턴스에서 증가시키는 값
    frame->frameNo = ++m_frameCounter;

    FrameCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_frameCb;
    }
    if (cb) cb(frame);
}

// ============================================================================
// static: SDK callback
// ============================================================================
int SmartRaySensor::CallbackPointCloud(
    SRSensor* sensor,
    ImageDataType /*dattyp*/,
    uint32_t numPoints,
    uint32_t numProfile,
    SR_3DPOINT* point_cloud,
    unsigned short* intensity,
    unsigned short* /*laserlinethickness*/,
    unsigned int* /*profileIdx*/,
    unsigned int* /*columnIdx*/,
    uint32_t /*numExtData*/,
    void* /*extData*/)
{
    if (!sensor || !point_cloud || numPoints == 0)
        return 0;

    // cam_index로 owner 찾기
    SmartRaySensor* owner = nullptr;
    {
        std::lock_guard<std::mutex> lk(s_instMtx);
        auto it = s_instances.find((int)sensor->cam_index);
        if (it != s_instances.end())
            owner = it->second;
    }
    if (!owner) return 0;

    // chunk frame 생성
    auto chunk = std::make_shared<PcFrame>();
    chunk->camIndex = (int)sensor->cam_index;
    chunk->numPoints = numPoints;
    chunk->numProfiles = numProfile;

    chunk->points.resize((size_t)numPoints);
    std::memcpy(chunk->points.data(), point_cloud, sizeof(SR_3DPOINT) * (size_t)numPoints);

    if (intensity)
    {
        chunk->intensity.resize((size_t)numPoints);
        std::memcpy(chunk->intensity.data(), intensity, sizeof(unsigned short) * (size_t)numPoints);
    }

    std::shared_ptr<PcFrame> toSend;

    // merge 정책: expectedProfiles==0 → 그대로 전달
    const uint32_t expected = owner->m_mergeExpectedProfiles.load();

    {
        std::lock_guard<std::mutex> lk(owner->m_accMtx);

        if (expected == 0)
        {
            toSend = chunk;
        }
        else
        {
            // 누적
            owner->m_accPoints.insert(owner->m_accPoints.end(),
                chunk->points.begin(), chunk->points.end());

            if (!chunk->intensity.empty())
            {
                owner->m_accIntensity.insert(owner->m_accIntensity.end(),
                    chunk->intensity.begin(), chunk->intensity.end());
            }

            owner->m_accProfiles += chunk->numProfiles;

            // expected 도달 시 1프레임으로 합쳐서 전달
            if (owner->m_accProfiles >= expected)
            {
                toSend = std::make_shared<PcFrame>();
                toSend->camIndex = chunk->camIndex;
                toSend->numProfiles = owner->m_accProfiles;

                toSend->points.swap(owner->m_accPoints);
                toSend->intensity.swap(owner->m_accIntensity);
                toSend->numPoints = (uint32_t)toSend->points.size();

                owner->m_accProfiles = 0;
            }
        }
    }

    if (toSend)
        owner->DispatchFrame(toSend);

    return 0;
}

int SmartRaySensor::UnknownCommandCallback(SRSensor* sensor)
{
    std::cout << "Unknown command from sensor: "
        << (sensor ? sensor->name : "(null)") << std::endl;
    return 0;
}

// ============================================================================
// error handling
// ============================================================================
bool SmartRaySensor::HandleReturnCode(int rc, const char* where)
{
    if (rc == SUCCESS) return true;

    char* apiErr = nullptr;
    SR_API_GetErrorMsg(rc, &apiErr);

    std::ostringstream oss;
    oss << where << " failed. rc=" << rc;
    if (apiErr) oss << ", msg=" << apiErr;

    {
        std::lock_guard<std::mutex> lk(m_errMtx);
        m_lastErr = rc;
        m_lastErrText = oss.str();
    }

    std::cout << "ERROR: " << oss.str() << std::endl;
    return false;
}

int SmartRaySensor::GetLastErrorCode() const
{
    std::lock_guard<std::mutex> lk(m_errMtx);
    return m_lastErr;
}

std::string SmartRaySensor::GetLastErrorText() const
{
    std::lock_guard<std::mutex> lk(m_errMtx);
    return m_lastErrText;
}
