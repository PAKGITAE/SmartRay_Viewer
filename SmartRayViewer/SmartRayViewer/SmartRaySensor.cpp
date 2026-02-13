#include "pch.h"
#include "SmartRaySensor.h"
#include <sstream>
#include <iostream>

std::mutex SmartRaySensor::s_apiMtx;
int  SmartRaySensor::s_apiRefCount = 0;
bool SmartRaySensor::s_apiInited = false;
std::atomic<bool> SmartRaySensor::s_pcCbRegistered{ false };

std::mutex SmartRaySensor::s_instMtx;
std::map<int, SmartRaySensor*> SmartRaySensor::s_instances;

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

    m_configuredOnce.store(false);
}

void SmartRaySensor::SetFrameCallback(FrameCallback cb)
{
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_frameCb = std::move(cb);
}

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
    ResetMergeBuffer();
}

bool SmartRaySensor::Start()
{
    if (!IsConnected())
        return false;

    if (!EnsureConfiguredOnce())
        return false;

    // Start 직전 profiles 변경 반영
    uint32_t profiles = m_profilesToCapture.load();
    if (profiles > 0)
    {
        if (!ApplyProfilesIfChanged(profiles))
            return false;
    }

    // Start 직전 XScale 변경 반영
    const float xScale = m_xScaleToSet.load();
    if (!ApplyXScaleIfChanged(xScale))
        return false;

    ResetMergeBuffer();

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

bool SmartRaySensor::EnsureConfiguredOnce()
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;
    if (m_configuredOnce.load()) return true;

    uint32_t profiles = m_profilesToCapture.load();
    if (profiles == 0) profiles = 200;

    if (!ConfigurePointCloud_Once(profiles))
        return false;

    m_configuredOnce.store(true);
    return true;
}

bool SmartRaySensor::ConfigurePointCloud_Once(uint32_t profiles)
{
    if (!IsConnected()) return false;

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

    ret = SR_API_SetNumberOfProfilesToCapture(m_sensor, profiles);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetNumberOfProfilesToCapture");

    ret = SR_API_SetPacketSize(m_sensor, 0);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetPacketSize");

    ret = SR_API_SetPacketTimeOut(m_sensor, 500);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetPacketTimeOut");

    ret = SR_API_SendParameterSetToSensor(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SendParameterSetToSensor");

    ret = SR_API_SetMetaDataExportEnabled(m_sensor, true);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetMetaDataExportEnabled");

    ret = SR_API_SetDataTriggerMode(m_sensor, DataTriggerMode_FreeRunning);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetDataTriggerMode(FreeRunning)");

    const float xScale = m_xScaleToSet.load();
    ret = SR_API_SetTransportResolution(m_sensor, xScale);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetTransportResolution(XScale)");

    // 로컬 캐시 업데이트
    SR_API_GetNumberOfProfilesToCapture(m_sensor, &m_sensorConfiguredProfiles);
    SR_API_GetTransportResolution(m_sensor, &m_sensorConfiguredXScale);
    SR_API_GetPacketSize(m_sensor, &m_packetSize);
    SR_API_GetPacketTimeOut(m_sensor, &m_packetTimeout);

    return true;
}

bool SmartRaySensor::ApplyProfilesIfChanged(uint32_t profiles)
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;
    if (profiles == 0) return true;

    if (m_sensorConfiguredProfiles == profiles)
        return true;

    int ret = SR_API_SetNumberOfProfilesToCapture(m_sensor, profiles);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetNumberOfProfilesToCapture");

    ret = SR_API_SendParameterSetToSensor(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SendParameterSetToSensor(profiles changed)");

    SR_API_GetNumberOfProfilesToCapture(m_sensor, &m_sensorConfiguredProfiles);
    SR_API_GetPacketSize(m_sensor, &m_packetSize);
    SR_API_GetPacketTimeOut(m_sensor, &m_packetTimeout);

    return true;
}

bool SmartRaySensor::ApplyXScaleIfChanged(float xScale)
{
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    if (!IsConnected()) return false;
    if (xScale <= 0.f) return true;

    int ret = SR_API_SetTransportResolution(m_sensor, xScale);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetTransportResolution(XScale)");

    SR_API_GetTransportResolution(m_sensor, &m_sensorConfiguredXScale);

    return true;
}

void SmartRaySensor::ResetMergeBuffer()
{
    std::lock_guard<std::mutex> lk(m_accMtx);
    m_accProfiles = 0;
    m_accPoints.clear();
    m_accIntensity.clear();
}

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

void SmartRaySensor::DispatchFrame(std::shared_ptr<PcFrame> frame)
{
    if (!frame) return;
    frame->frameNo = ++m_frameCounter;

    FrameCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_frameCb;
    }
    if (cb) cb(frame);
}

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

    SmartRaySensor* owner = nullptr;
    {
        std::lock_guard<std::mutex> lk(s_instMtx);
        auto it = s_instances.find((int)sensor->cam_index);
        if (it != s_instances.end())
            owner = it->second;
    }
    if (!owner) return 0;

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

    const uint32_t expected = owner->m_mergeExpectedProfiles.load();

    {
        std::lock_guard<std::mutex> lk(owner->m_accMtx);

        if (expected == 0)
        {
            toSend = chunk;
        }
        else
        {
            owner->m_accPoints.insert(owner->m_accPoints.end(),
                chunk->points.begin(), chunk->points.end());

            if (!chunk->intensity.empty())
            {
                owner->m_accIntensity.insert(owner->m_accIntensity.end(),
                    chunk->intensity.begin(), chunk->intensity.end());
            }

            owner->m_accProfiles += chunk->numProfiles;

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
