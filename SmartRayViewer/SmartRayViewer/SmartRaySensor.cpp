#include "pch.h"
#include "SmartRaySensor.h"
#include <sstream>
#include <iostream>

#include <cstring>

std::mutex SmartRaySensor::s_instMtx;
std::map<int, SmartRaySensor*> SmartRaySensor::s_instances;

std::mutex SmartRaySensor::s_apiMtx;
int  SmartRaySensor::s_apiRefCount = 0;
bool SmartRaySensor::s_apiInited = false;

std::atomic<bool> SmartRaySensor::s_pcCbRegistered{ false };

void ThreadMakeGrabData(void* lParam, uint32_t width, uint32_t height, uint8_t* p_img)
{

}

SmartRaySensor::SmartRaySensor()
{
    m_grabHelper.SetFrameHandler([](std::shared_ptr<PcFrame> frame)
        {
            //Result::GetInstance().SaveFrameAsAsc(frame);
            Result::GetInstance().OnFrameArrived(frame);
        });
}

SmartRaySensor::~SmartRaySensor()
{
    Disconnect();
    delete m_sensor;
}


void SmartRaySensor::Configure(const std::string& name,
    const std::string& ip,
    unsigned short port,
    int sensorIndex)
{
    m_name = name;
    m_ip = ip;
    m_port = port;
    m_sensorIndex = sensorIndex;

    if (!m_sensor)
        m_sensor = new SRSensor();

    // ⭐ 중요: 구조체 전체 0 초기화 (멀티 센서 안정성)
    memset(m_sensor, 0, sizeof(SRSensor));

    m_sensor->cam_index = m_sensorIndex;
    m_sensor->active = 0;

    strcpy_s(m_sensor->name, name.c_str());
    strcpy_s(m_sensor->IPAdr, ip.c_str());
    m_sensor->portnum = port;

    m_sensor->usercbf = &SmartRaySensor::UnknownCommandCallback;

}

bool SmartRaySensor::EnsureApiInitialized()
{
    std::lock_guard<std::mutex> lk(s_apiMtx);

    if (!s_apiInited)
    {
        int rc = SR_API_Initalize(nullptr);
        if (rc != SUCCESS)
            return false;

        // (선택) 버전 확인은 실패해도 init 자체를 실패로 보지 않는 게 보통 안전
        char* apiVersion = nullptr;
        SR_API_GetAPIVersion(&apiVersion);

        s_apiInited = true;
        s_apiRefCount = 0; // ✅ init 시점에 refcount 정합성 보장
    }

    // ✅ overflow는 현실적으로 거의 없지만, 방어해두면 좋음
    if (s_apiRefCount == INT_MAX)
        return false;

    ++s_apiRefCount;
    return true;
}

void SmartRaySensor::ReleaseApiIfNeeded()
{
    std::lock_guard<std::mutex> lk(s_apiMtx);

    // ✅ init 안됐는데 release가 들어오면 "호출 짝이 깨진 것"
    if (!s_apiInited)
    {
        // 여기서 조용히 return 해도 되지만, 디버그에선 잡아두는 게 좋음
        // ASSERT(false); 또는 Log
        s_apiRefCount = 0; // 정합성 유지
        return;
    }

    // ✅ 0인데도 release가 들어오면 중복 release/실패 경로 정리가 꼬인 것
    if (s_apiRefCount <= 0)
    {
        // ASSERT(false); 또는 Log6
        s_apiRefCount = 0;
        return;
    }

    --s_apiRefCount;

    if (s_apiRefCount == 0)
    {
        SR_API_Exit();
        s_apiInited = false;
        // s_apiRefCount는 이미 0
    }
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
        // ✅ Init만 되고 Connect 실패면 바로 ref 반환
        ReleaseApiIfNeeded();
        m_apiRefHeld = false;
        return false;
    }

    m_connected.store(true);
    RegisterInstance();
    return true;
}

void SmartRaySensor::Disconnect()
{
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
}

bool SmartRaySensor::HandleReturnCode(int rc, const char* where)
{
    if (rc == SUCCESS)
        return true;

    char* apiErr = nullptr;
    SR_API_GetErrorMsg(rc, &apiErr);

    std::ostringstream oss;
    oss << where << " failed. rc=" << rc;
    if (apiErr)
        oss << ", msg=" << apiErr;

    {
        std::lock_guard<std::mutex> lk(m_errMtx);
        m_lastErr = rc;
        m_lastErrText = oss.str();
    }

    std::cout << "ERROR: " << oss.str() << std::endl;
    return false;
}

int SmartRaySensor::UnknownCommandCallback(SRSensor* sensor)
{
    std::cout << "Unknown command from sensor: "
        << (sensor ? sensor->name : "(null)") << std::endl;
    return 0;
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

bool SmartRaySensor::StartPointCloud()
{
    if (!IsConnected())
        return false;

    if (!m_isPcConfigured)
    {
        int nProfiles = AppStore::Get().GetParameterAsInt("System", "NumberOfProfiles");

        if (!ConfigurePointCloud(nProfiles, DEFAULT_RESOLUTION, true))
            return false;
    }

    BeginCapture();

    // 1) 콜백 등록은 프로세스에서 1번만
    //if (!s_pcCbRegistered.load())
    //{
    //    int retCb = SR_API_RegisterPointCloudCB(&SmartRaySensor::CallbackPointCloud);
    //    if (retCb != SUCCESS)
    //        return HandleReturnCode(retCb, "SR_API_RegisterPointCloudCB");

    //    s_pcCbRegistered.store(true);
    //}
    // ✅ 1회 등록을 원자적으로 보장
    bool expected = false;
    if (s_pcCbRegistered.compare_exchange_strong(expected, true))
    {
        int retCb = SR_API_RegisterPointCloudCB(&SmartRaySensor::CallbackPointCloud);
        if (retCb != SUCCESS)
        {
            s_pcCbRegistered.store(false); // 실패 시 되돌리기
            return HandleReturnCode(retCb, "SR_API_RegisterPointCloudCB");
        }
    }

    // 2) Start acquisition
    std::cout << "start sensor data acquisition.\n";
    int ret = SR_API_StartAcquisition(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_StartAcquisition");

    m_running.store(true);
    return true;

}

void SmartRaySensor::StopPointCloud()
{
    if (!m_running.load())
        return;

    std::cout << "Stop acquisition\n";
    int ret = SR_API_StopAcquisition(m_sensor);
    HandleReturnCode(ret, "SR_API_StopAcquisition");

    m_running.store(false);
}

//여기 함수가 졸라 중요함
bool SmartRaySensor::ConfigurePointCloud(int numberOfProfiles,
    float transportResolution,
    bool enableMetaData)
{
    if (!IsConnected())
        return false;

    m_numProfilesToCapture = numberOfProfiles;
    m_transportResolution = transportResolution;

    int ret = 0;

    // 1) Calibration from sensor
    ret = SR_API_LoadCalibrationDataFromSensor(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_LoadCalibrationDataFromSensor");

    // 2) Load parameter set file (예제: Snapshot3dParameterSet)
    // 예제는 환경변수 SmartRay 기반으로 파일 경로를 만든 뒤 LoadParameterSetFromFile 호출함.
    // 여기서는 "일단 너가 쓰는 par 경로"를 넣어야 함.
    // ------------------------------------
    // TODO: 아래 경로를 실제 par 파일 경로로 바꿔!
    const char* parFile = "C:\\SmartRay\\SmartRay DevKit\\SR_API\\sr_parameter_sets\\Pars_ECCO95\\ECCO95_3D_Repeat_Snapshot.par";
    //const char* parFile = "C:\\SmartRay\\SmartRay DevKit\\SR_API\\sr_parameter_sets\\Pars_ECCO95\\ECCO95_3D_Snapshot.par";
    
    ret = SR_API_LoadParameterSetFromFile(m_sensor, parFile);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_LoadParameterSetFromFile");

    // 3) Configure 3D Acquisition = PointCloud + profiles
    ret = SR_API_SetImageAcquisitionType(m_sensor, ImageAquisitionType_PointCloud);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetImageAcquisitionType(PointCloud)");

    ret = SR_API_SetNumberOfProfilesToCapture(m_sensor, numberOfProfiles);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetNumberOfProfilesToCapture");

    ret = SR_API_SetPacketSize(m_sensor, 0); // autopacketsize
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetPacketSize");

    // ECCO95 only default 500ms
    ret = SR_API_SetPacketTimeOut(m_sensor, 500);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetPacketTimeOut");

    // 4) Send parameter set to sensor (중요!)
    ret = SR_API_SendParameterSetToSensor(m_sensor);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SendParameterSetToSensor");

    // 5) Meta data enable (선택)
    ret = SR_API_SetMetaDataExportEnabled(m_sensor, enableMetaData);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetMetaDataExportEnabled");

    // 6) Transport resolution 설정(예제 PreparePointCloudsAcquisition 내부에 있음)
    ret = SR_API_SetTransportResolution(m_sensor, transportResolution);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SR_API_SetTransportResolution");

    // 7) 프리런 모드로 설정
    ret = SR_API_SetDataTriggerMode(m_sensor, DataTriggerMode_FreeRunning);
    if (ret != SUCCESS) return HandleReturnCode(ret, "SetDataTriggerMode FreeRun");


    // (옵션) expected profiles / packet info 확인 (예제 StartAcquisition에서 했던 것)
    SR_API_GetNumberOfProfilesToCapture(m_sensor, &m_expectedProfiles);
    SR_API_GetPacketSize(m_sensor, &m_packetSize);
    SR_API_GetPacketTimeOut(m_sensor, &m_packetTimeout);

    std::wstring msg =
        L"[ConfigurePointCloud] profiles=" + std::to_wstring(m_expectedProfiles) +
        L" packetSize=" + std::to_wstring(m_packetSize) +
        L" packetTimeout=" + std::to_wstring(m_packetTimeout);

    LogManager& logMgr = LogManager::GetInstance();
    logMgr.PushLog(Log::Sensor, L"ConfigurePointCloud", msg);

    m_isPcConfigured = true;
    return true;
}


void SmartRaySensor::RegisterInstance()
{
    std::lock_guard<std::mutex> lk(s_instMtx);
    s_instances[m_sensorIndex] = this; // key = cam_index
}

void SmartRaySensor::UnregisterInstance()
{
    std::lock_guard<std::mutex> lk(s_instMtx);
    auto it = s_instances.find(m_sensorIndex);
    if (it != s_instances.end() && it->second == this)
        s_instances.erase(it);
}

// SmartRaySensor.cpp
void SmartRaySensor::BeginCapture()
{
    std::lock_guard<std::mutex> lk(m_accMtx);
    m_accProfiles = 0;
    m_accPoints.clear();
    m_accIntensity.clear();
    m_stopRequested.store(false);
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

    // ✅ 이번 chunk를 안전하게 복사
    auto chunk = std::make_shared<PcFrame>();
    chunk->camIndex = (int)sensor->cam_index;
    chunk->numPoints = numPoints;
    chunk->numProfiles = numProfile;

    chunk->points.resize((size_t)numPoints);
    std::memcpy(chunk->points.data(), point_cloud,
        sizeof(SR_3DPOINT) * (size_t)numPoints);

    if (intensity)
    {
        chunk->intensity.resize((size_t)numPoints);
        std::memcpy(chunk->intensity.data(), intensity,
            sizeof(unsigned short) * (size_t)numPoints);
    }

    std::shared_ptr<PcFrame> mergedToSend; // 목표 도달 시 이것만 채워서 밖에서 AddGrabData

    LogManager& logMgr = LogManager::GetInstance();

    //std::wstring strStartLog =
    //    L"[Cam " + std::to_wstring(sensor->cam_index) +
    //    L"] chunkProfile=" + std::to_wstring(numProfile) +
    //    L" acc=" + std::to_wstring(owner->m_accProfiles) +
    //    L"/" + std::to_wstring(owner->m_expectedProfiles);

    //logMgr.PushLog(Log::Sensor, L"CallbackPointCloud", strStartLog);
   
    {
        std::lock_guard<std::mutex> lk(owner->m_accMtx);

        // expectedProfiles가 0이면 “머지 모드 아님”으로 처리(원하면 바로 전달)
        if (owner->m_expectedProfiles == 0)
        {
            mergedToSend = chunk; // 그대로 전달
        }
        else
        {
            // ✅ 누적: points/intensity를 acc 버퍼에 append
            owner->m_accPoints.insert(owner->m_accPoints.end(),
                chunk->points.begin(), chunk->points.end());

            if (!chunk->intensity.empty())
            {
                owner->m_accIntensity.insert(owner->m_accIntensity.end(),
                    chunk->intensity.begin(), chunk->intensity.end());
            }

            owner->m_accProfiles += chunk->numProfiles;

            // ✅ 목표 도달했으면 최종 1개 생성
            if (owner->m_accProfiles >= owner->m_expectedProfiles)
            {
                mergedToSend = std::make_shared<PcFrame>();
                mergedToSend->camIndex = chunk->camIndex;
                mergedToSend->numProfiles = owner->m_accProfiles; // 또는 m_expectedProfiles로 고정해도 됨

                // swap으로 복사 없이 넘김
                mergedToSend->points.swap(owner->m_accPoints);
                mergedToSend->intensity.swap(owner->m_accIntensity);

                mergedToSend->numPoints = (uint32_t)mergedToSend->points.size();

                // ✅ 다음 캡처를 위해 초기화
                owner->m_accProfiles = 0;

                // (선택) 1회 촬상이라면 stop 요청 (콜백에서 stop 직접 호출하지 말기)
                owner->m_stopRequested.store(true);
            }
        }
    }

    // ✅ 락 밖에서 AddGrabData (중요)
    if (mergedToSend) {
        owner->m_grabHelper.AddGrabData(mergedToSend);

        std::wstring strLog = L"Data Merge Success(Result클래스 전달) = " + std::to_wstring(mergedToSend->numProfiles);
        logMgr.PushLog(Log::Sensor, L"CallbackPointCloud", strLog);

    }

    return 0;
}
