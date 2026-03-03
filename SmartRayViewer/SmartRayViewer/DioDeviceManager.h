#pragma once
#include "cdio.h"
#include <vector>
#include <thread>
#include <chrono>
#include <afxmt.h>

struct DioInputBit
{
    static const int Measure = 0;  // 측정
    static const int CameraFwd = 1;  // 카메라 전진
    static const int CameraBwd = 2;  // 카메라 후진
    static const int CameraFwdSensor = 3;  // 카메라 전진 센서 감지
    static const int CameraBwdSensor = 4;  // 카메라 후진 센서 감지
};

struct DioOutputBit
{
    static const int MoveFwd = 2;  // 정방향 이동
    static const int MoveBwd = 0;  // 역방향 이동
    static const int MovePowerOff = 3;  // 전원 차단
};

class DioDeviceManager
{
public:
    // 싱글톤 접근 함수
    static DioDeviceManager& Instance();

    // 핵심 API
    bool Init(const char* deviceName);
    bool SetIoDirection(DWORD dir);
    bool IsConnected() const { return m_IsConnected; }

    std::vector<bool> GetInputStatus();
    std::vector<bool> GetOutputStatus();

    bool InpByte(short portNo, BYTE& data);
    bool InpBit(short bitNo, BYTE& data);
    bool OutByte(short portNo, BYTE data);
    bool OutBit(short bitNo, BYTE data);
    bool EchoBackByte(short portNo, BYTE& data);
    bool EchoBackBit(short bitNo, BYTE& data);


    void Exit();

private:
    // 생성자, 복사금지
    DioDeviceManager();
    ~DioDeviceManager();

    DioDeviceManager(const DioDeviceManager&) = delete;
    DioDeviceManager& operator=(const DioDeviceManager&) = delete;


    short m_Id;
    long  m_Ret;
    char  m_Error[256];

    bool m_IsConnected;

    bool m_KillThread;
    std::thread* m_pThread;

    std::vector<bool> m_InputStatus;
    std::vector<bool> m_OutputStatus;

    CRITICAL_SECTION m_Crit;
    void StartMonitorThread();
    void StopMonitorThread();
    void MonitorThreadProc();

    int m_ReconnectIntervalMs = 1000;
};
