#include "pch.h"
#include "LogManager.h"
#include "DlgLog.h"

LogManager::LogManager() : _initialized(false), _pDlgLog(nullptr)
{
}

void LogManager::Initialize(const std::wstring& baseLogPath, bool useFlush)
{
    if (_initialized) {
        return; // 이미 초기화된 경우 무시
    }

    // 로그 카테고리 설정
    _logList.clear();

    // 시스템 로그
    _logList.emplace_back(LogList(Log::System, baseLogPath + L"\\System\\"));

    // 각 페이지별 로그
    _logList.emplace_back(LogList(Log::Sensor, baseLogPath + L"\\Sensor\\"));
    _logList.emplace_back(LogList(Log::Copy, baseLogPath + L"\\Copy\\"));
    _logList.emplace_back(LogList(Log::Viewer, baseLogPath + L"\\Viewer\\"));


    // 각 로거 생성
    for (auto& log : _logList)
    {
        _log.CreateFileLogger(log.logKey, log.logPath, useFlush);
    }

    _initialized = true;

    // 초기화 완료 로그
    PushLog(Log::System, L"Initialize", L"=== Log Manager Initialized ===");
}

void LogManager::PushLog(const std::wstring& logKey, const std::wstring& textFunc, const std::wstring& message)
{
    if (!_initialized) {
        // 초기화되지 않은 경우 기본 경로로 초기화
        Initialize();
    }

    // 로그 키가 존재하는지 확인
    bool keyExists = false;
    for (const auto& log : _logList) {
        if (log.logKey == logKey) {
            keyExists = true;
            break;
        }
    }

    // 키가 존재하지 않으면 system 로그에 기록
    std::wstring actualKey = keyExists ? logKey : Log::System;

    std::wstring resultMessage = L"[" + textFunc + L"] " + message;
    _log.PushLog(actualKey, resultMessage);

    // DlgLog 에 추가
    if (_pDlgLog)
    {
        _pDlgLog->AddLog(logKey, textFunc, message);
    }
}

void LogManager::SetLogDialog(DlgLog* pDlgLog)
{
    _pDlgLog = pDlgLog;
}

void LogManager::Shutdown()
{
    if (!_initialized) {
        return;
    }

    PushLog(Log::System, L"Shutdown", L"=== Log Manager Shutdown ===");
    _initialized = false;
}

