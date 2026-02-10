#pragma once
#include <string>
#include <atomic>
#include <mutex>

#include <map>
#include <memory>

#include "vThreadPool.h"

#include "SR_API_public.h"
#include "SR_API_Defines.h"
#include "sr_api_errorcodes.h"

#include "GrabHelper.h"

#include "LogManager.h"


static const float DEFAULT_RESOLUTION =
(float)0.100000;  // Default x-axis resolution (transport resolution) for point cloud data.

class SmartRaySensor
{
public:
    SmartRaySensor();
    ~SmartRaySensor();

    SmartRaySensor(const SmartRaySensor&) = delete;
    SmartRaySensor& operator=(const SmartRaySensor&) = delete;

public:
    //----------------------------------------------//
    // 연결 정보 설정
    void Configure(const std::string& name,
        const std::string& ip,
        unsigned short port,
        int sensorIndex = 0);

    // 연결
    bool Connect(int timeoutS = 60);

    bool m_apiRefHeld = false;

    // 연결 해제
    void Disconnect();

    // 연결 상태 반환
    bool IsConnected() const { return m_connected.load(); }

    //측정상태 반환
    bool IsRunning() const { return m_running.load(); }
    //----------------------------------------------//

    //----------------------------------------------//
    //유틸 함수
    int         GetLastErrorCode() const;
    std::string GetLastErrorText() const;
    //----------------------------------------------//

    //----------------------------------------------//
    // 검사 관련 함수들
    // PointCloud 수신 시작
    bool StartPointCloud();

    // PointCloud 수신 정지
    void StopPointCloud();

    // 센서 설정값 변경 -> 검사 시작전 호출
    bool ConfigurePointCloud(
        int numberOfProfiles = 200,
        float transportResolution = DEFAULT_RESOLUTION,
        bool enableMetaData = true);
    //----------------------------------------------//

    void BeginCapture(); // SR_API_SetNumberOfProfilesToCapture() 호출하는 곳에서 같이 불러도 됨

private:
    // 연결전 확인정보
    static bool EnsureApiInitialized();

    // API연결 종료
    static void ReleaseApiIfNeeded();

    // 에러코드 메세지 출력용
    bool HandleReturnCode(int rc, const char* where);

    //뭔가 로그 출력용 같음 -> 필요없어 보임
    static int UnknownCommandCallback(SRSensor* sensor);

private:
    SRSensor* m_sensor = nullptr;

    // 센서 정보
    std::string m_name;
    std::string m_ip;
    unsigned short m_port = 0;
    int m_sensorIndex = 0;

    // 연속해서 센서 설정을 변경하지 않기 위한 변수
    bool m_isPcConfigured = false;

    // 연결정보
    std::atomic<bool> m_connected{ false };

    // 측정중인지 확인하는 정보
    std::atomic<bool> m_running{ false };

    // 뮤텍스
    mutable std::mutex m_errMtx;

    // 에러 정보
    int m_lastErr = 0;
    std::string m_lastErrText;

    //초기화/종료 이중 접근 방지용 뮤텍스
    static std::mutex s_apiMtx;

    //센서 2개 off후 API종료하기 위한 카운트
    static int  s_apiRefCount;

    // 이중 API종료 방지용
    static bool s_apiInited;

    //콜백 등록을 1회만 하기 위한 변수
    static std::atomic<bool> s_pcCbRegistered;


private:
    // ✅ SDK가 요구하는 정확한 시그니처
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

private:
    // cam_index -> SmartRaySensor 인스턴스 매핑 (멀티센서 핵심)
    static std::mutex s_instMtx;
    static std::map<int, SmartRaySensor*> s_instances;

    void RegisterInstance();
    void UnregisterInstance();

private:  
    uint32_t m_packetSize = 0;              //패킷 사이즈
    uint32_t m_packetTimeout = 0;           //타임아웃

    float m_transportResolution = DEFAULT_RESOLUTION;   //스케일
    int   m_numProfilesToCapture = 200;                 //설정 캡쳐 갯수


private:
    std::mutex m_accMtx;

    uint32_t m_expectedProfiles = 0;   // 설정값(예: 1000)
    uint32_t m_accProfiles = 0;        // 누적된 profiles
    int      m_camIndex = 0;

    std::vector<SR_3DPOINT>       m_accPoints;
    std::vector<unsigned short>  m_accIntensity;

    std::atomic<bool> m_stopRequested{ false }; // ✅ 콜백 밖에서 stop 하려고

private:
    GrabHelper m_grabHelper;
};
