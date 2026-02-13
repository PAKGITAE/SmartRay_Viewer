#pragma once

#include "vLog.h"
#include <vector>

class DlgLog;  // 전방 선언 (LogManager에서 DlgLog 접근용)

// 로그 키 상수 정의
namespace Log {
    const std::wstring Main = L"Main";
    const std::wstring Sensor = L"Sensor";
    const std::wstring Result = L"Result";
}

// LogList 구조체 정의
struct LogList
{
    std::wstring logKey;
    std::wstring logPath;

    LogList(const std::wstring& key, const std::wstring& path)
        : logKey(key), logPath(path) {
    }
};

// LogManager 싱글톤 클래스
class LogManager
{
public:
    static LogManager& GetInstance() {
        static LogManager instance;
        return instance;
    }

    // 로그 시스템 초기화
    // baseLogPath: 기본 로그 경로 (예: "D:\\Log")
    // useFlush: 파일 플러시 사용 여부
    void Initialize(const std::wstring& baseLogPath = L"D:\\Log", bool useFlush = true);

    // 로그 출력
    // logKey: 로그 카테고리 키 (grab, calibration, defect, gap, gray, system 등)
    // message: 로그 메시지
    void PushLog(const std::wstring& logKey, const std::wstring& textFunc, const std::wstring& message);

    /** 로그 다이얼로그 설정. ForTiDlg 초기화 시 &_dlgLog 전달. */
    void SetLogDialog(DlgLog* pDlgLog);

    /** 등록된 로그 다이얼로그 접근. 없으면 nullptr. */
    DlgLog* GetLogDialog() const { return _pDlgLog; }

    // 로그 시스템 종료
    void Shutdown();

private:
    LogManager();
    ~LogManager() = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    bool _initialized;
    vLog _log;
    std::vector<LogList> _logList;

    DlgLog* _pDlgLog = nullptr;
};

