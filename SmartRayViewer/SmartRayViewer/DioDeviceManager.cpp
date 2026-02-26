#include "pch.h"
#include "DioDeviceManager.h"
#include <cstring>

DioDeviceManager::DioDeviceManager()
{
    m_Id = -1;
    m_Ret = 0;
    m_IsConnected = false;

    m_KillThread = false;
    m_pThread = nullptr;

    InitializeCriticalSection(&m_Crit);
    memset(m_Error, 0, sizeof(m_Error));
}

DioDeviceManager::~DioDeviceManager()
{
    StopMonitorThread();
    Exit();
    DeleteCriticalSection(&m_Crit);
}

DioDeviceManager& DioDeviceManager::Instance()
{
    static DioDeviceManager instance;
    return instance;
}

// ------------------------------
// Init (char → char* 변환 포함)
// ------------------------------
bool DioDeviceManager::Init(const char* deviceName)
{
    char devName[256];
    strcpy_s(devName, deviceName);

    m_Ret = DioInit(devName, &m_Id);
    m_IsConnected = (m_Ret == DIO_ERR_SUCCESS);

    if (m_IsConnected)
    {
        // 128bit 기준 → 필요 시 변경 가능
        m_InputStatus.resize(128, false);
        m_OutputStatus.resize(128, false);

        StartMonitorThread();
    }

    return m_IsConnected;
}

void DioDeviceManager::Exit()
{
    StopMonitorThread();

    if (m_Id >= 0)
    {
        DioExit(m_Id);
        m_Id = -1;
    }

    m_IsConnected = false;
}

// ------------------------------
// I/O functions
// ------------------------------
bool DioDeviceManager::InpByte(short portNo, BYTE& data)
{
    if (!m_IsConnected) return false;
    m_Ret = DioInpByte(m_Id, portNo, &data);
    return (m_Ret == DIO_ERR_SUCCESS);
}

bool DioDeviceManager::InpBit(short bitNo, BYTE& data)
{
    if (!m_IsConnected) return false;
    m_Ret = DioInpBit(m_Id, bitNo, &data);
    return (m_Ret == DIO_ERR_SUCCESS);
}

bool DioDeviceManager::OutByte(short portNo, BYTE data)
{
    if (!m_IsConnected) return false;
    m_Ret = DioOutByte(m_Id, portNo, data);
    return (m_Ret == DIO_ERR_SUCCESS);
}

bool DioDeviceManager::OutBit(short bitNo, BYTE data)
{
    if (!m_IsConnected) return false;
    m_Ret = DioOutBit(m_Id, bitNo, data);
    return (m_Ret == DIO_ERR_SUCCESS);
}

bool DioDeviceManager::EchoBackByte(short portNo, BYTE& data)
{
    if (!m_IsConnected) return false;
    m_Ret = DioEchoBackByte(m_Id, portNo, &data);
    return (m_Ret == DIO_ERR_SUCCESS);
}

bool DioDeviceManager::EchoBackBit(short bitNo, BYTE& data)
{
    if (!m_IsConnected) return false;
    m_Ret = DioEchoBackBit(m_Id, bitNo, &data);
    return (m_Ret == DIO_ERR_SUCCESS);
}

bool DioDeviceManager::SetIoDirection(DWORD dir)
{
    if (!m_IsConnected) return false;
    m_Ret = DioSetIoDirection(m_Id, dir);
    return (m_Ret == DIO_ERR_SUCCESS);
}

// ------------------------------
// Real-time getters
// ------------------------------
std::vector<bool> DioDeviceManager::GetInputStatus()
{
    EnterCriticalSection(&m_Crit);
    auto copy = m_InputStatus;
    LeaveCriticalSection(&m_Crit);
    return copy;
}

std::vector<bool> DioDeviceManager::GetOutputStatus()
{
    EnterCriticalSection(&m_Crit);
    auto copy = m_OutputStatus;
    LeaveCriticalSection(&m_Crit);
    return copy;
}

// ------------------------------
// Thread Control
// ------------------------------
void DioDeviceManager::StartMonitorThread()
{
    if (m_pThread) return;
    m_KillThread = false;
    m_pThread = new std::thread(&DioDeviceManager::MonitorThreadProc, this);
}

void DioDeviceManager::StopMonitorThread()
{
    if (!m_pThread) return;
    m_KillThread = true;
    m_pThread->join();
    delete m_pThread;
    m_pThread = nullptr;
}

// ------------------------------
// Monitoring Thread
// ------------------------------
void DioDeviceManager::MonitorThreadProc()
{
    while (!m_KillThread)
    {
        // ---------------------------
        // 1) 연결 끊김 → 자동 재연결
        // ---------------------------
        if (!m_IsConnected)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_ReconnectIntervalMs));

            char devName[256] = "DIO000";
            long ret = DioInit(devName, &m_Id);

            if (ret == DIO_ERR_SUCCESS)
            {
                m_IsConnected = true;

                EnterCriticalSection(&m_Crit);
                std::fill(m_InputStatus.begin(), m_InputStatus.end(), false);
                std::fill(m_OutputStatus.begin(), m_OutputStatus.end(), false);
                LeaveCriticalSection(&m_Crit);
            }
            continue;
        }

        // ---------------------------
        // 2) 입력 / 출력 갱신
        // ---------------------------
        for (int bit = 0; bit < 8; ++bit)
        {
            BYTE val = 0;

            // INPUT
            if (DioInpBit(m_Id, bit, &val) != DIO_ERR_SUCCESS)
            {
                m_IsConnected = false;
                break;
            }

            bool inputVal = (val != 0);

            // OUTPUT (EchoBack)
            if (DioEchoBackBit(m_Id, bit, &val) != DIO_ERR_SUCCESS)
            {
                m_IsConnected = false;
                break;
            }

            bool outputVal = (val != 0);

            EnterCriticalSection(&m_Crit);
            m_InputStatus[bit] = inputVal;
            m_OutputStatus[bit] = outputVal;
            LeaveCriticalSection(&m_Crit);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}
