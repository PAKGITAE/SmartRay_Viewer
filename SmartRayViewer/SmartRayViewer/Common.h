#pragma once

#include <vector>
#include <string>

// 시스템 문자열 상수 네임스페이스
namespace SysConstants
{
    const std::wstring RecipeFolderName = L"Recipe";
    const std::wstring SysFolderName = L"System";

    const std::wstring SysFildName = L"SysParam.json";
}

inline std::wstring GetBasePath()
{
	// exe 실제 경로
	wchar_t exePath[MAX_PATH] = { 0 };
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);

	std::wstring exeDir(exePath);
	size_t pos = exeDir.find_last_of(L"\\/");
	if (pos != std::wstring::npos)
		exeDir = exeDir.substr(0, pos);

	return exeDir;
}

inline std::wstring CombinePath(const std::wstring& base, const std::wstring& relative)
{
    if (base.empty()) return relative;
    if (relative.empty()) return base;

    std::wstring result = base;
    if (result.back() != L'\\' && result.back() != L'/')
        result += L"\\";
    if (relative.front() == L'\\' || relative.front() == L'/')
        result += relative.substr(1);
    else
        result += relative;
    return result;
}

// 폴더 내 파일 리스트를 반환하는 함수
inline std::vector<std::wstring> GetFileList(const std::wstring& folderPath)
{
    std::vector<std::wstring> fileList;
    
    if (folderPath.empty())
        return fileList;

    // 경로 끝에 \*.* 추가
    std::wstring searchPath = folderPath;
    if (searchPath.back() != L'\\' && searchPath.back() != L'/')
        searchPath += L"\\";
    searchPath += L"*.*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
        return fileList;

    do
    {
        // . 과 .. 디렉토리 제외
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
            continue;

        // 파일만 추가 (디렉토리 제외)
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            fileList.push_back(findData.cFileName);
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);
    return fileList;
}

// 파일명에서 확장자를 제거하는 함수
inline std::wstring RemoveFileExtension(const std::wstring& fileName)
{
    if (fileName.empty())
        return fileName;

    size_t lastDot = fileName.find_last_of(L".");
    size_t lastSlash = fileName.find_last_of(L"\\/");

    // 마지막 점이 마지막 슬래시 이후에 있고, 점이 존재하는 경우
    if (lastDot != std::wstring::npos && (lastSlash == std::wstring::npos || lastDot > lastSlash))
    {
        return fileName.substr(0, lastDot);
    }

    // 확장자가 없는 경우 원본 반환
    return fileName;
}


// ===============================
// Utils(센서 연결 로그/메세지박스 출력을 위함)
// ===============================
static std::wstring ToW(const std::string& s)
{
    return std::wstring(s.begin(), s.end());
}

struct ConnectAttemptResult
{
    int cam = -1;
    std::string name;   // "Sensor0"...
    std::string ip;
    unsigned short port = 0;

    bool ok = false;
    DWORD elapsedMs = 0;
    std::string errText; // SmartRaySensor::GetLastErrorText()
};

// 로그용 한 줄
static std::wstring BuildLogLine(const ConnectAttemptResult& r)
{
    std::wstring w =
        L"[" + std::wstring(r.ok ? L"OK" : L"FAIL") + L"] " +
        ToW(r.name) + L" cam=" + std::to_wstring(r.cam) +
        L" ip=" + ToW(r.ip) + L":" + std::to_wstring(r.port) +
        L" elapsed=" + std::to_wstring(r.elapsedMs) + L"ms";

    if (!r.ok && !r.errText.empty())
        w += L" err=" + ToW(r.errText);

    return w;
}

// 메시지박스 요약
static std::wstring BuildSummaryMessage(const std::vector<ConnectAttemptResult>& results)
{
    bool allOk = true;
    for (auto& r : results) allOk &= r.ok;

    std::wstring msg;
    msg += allOk ? L"센서 연결 성공\n\n" : L"센서 연결 실패(일부)\n\n";

    for (auto& r : results)
    {
        msg += L"- " + ToW(r.name) + L" : " + (r.ok ? L"OK" : L"FAIL");
        msg += L" (" + ToW(r.ip) + L":" + std::to_wstring(r.port) + L")";
        if (!r.ok && !r.errText.empty())
            msg += L"\n   " + ToW(r.errText);
        msg += L"\n";
    }
    return msg;
}

//PC 사용량(CPU, 메모리, 하드디스크)
static ULONGLONG FtToUll(const FILETIME& ft)
{
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

// CPU usage (%), 이전 호출 대비
static double GetCpuUsagePercent()
{
    static bool s_init = false;
    static ULONGLONG s_prevIdle = 0, s_prevKernel = 0, s_prevUser = 0;

    FILETIME idleFt{}, kernelFt{}, userFt{};
    if (!::GetSystemTimes(&idleFt, &kernelFt, &userFt))
        return -1.0;

    ULONGLONG idle = FtToUll(idleFt);
    ULONGLONG kernel = FtToUll(kernelFt);
    ULONGLONG user = FtToUll(userFt);

    if (!s_init)
    {
        s_init = true;
        s_prevIdle = idle; s_prevKernel = kernel; s_prevUser = user;
        return 0.0; // 첫 샘플은 0으로
    }

    ULONGLONG idleDiff = idle - s_prevIdle;
    ULONGLONG kernelDiff = kernel - s_prevKernel;
    ULONGLONG userDiff = user - s_prevUser;

    s_prevIdle = idle; s_prevKernel = kernel; s_prevUser = user;

    ULONGLONG total = kernelDiff + userDiff;
    if (total == 0) return 0.0;

    double usage = (double)(total - idleDiff) * 100.0 / (double)total;
    if (usage < 0) usage = 0;
    if (usage > 100) usage = 100;
    return usage;
}

// Memory usage (%), usedGB/totalGB 도 같이 만들기 좋음
static bool GetMemoryUsage(double& outUsedPercent, double& outUsedGB, double& outTotalGB)
{
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!::GlobalMemoryStatusEx(&ms))
        return false;

    const double total = (double)ms.ullTotalPhys;
    const double avail = (double)ms.ullAvailPhys;
    const double used = total - avail;

    outTotalGB = total / (1024.0 * 1024.0 * 1024.0);
    outUsedGB = used / (1024.0 * 1024.0 * 1024.0);
    outUsedPercent = (total > 0) ? (used * 100.0 / total) : 0.0;
    return true;
}

// Drive free/total GB
static bool GetDriveUsageGB(const wchar_t* rootPath, double& outFreeGB, double& outTotalGB)
{
    ULARGE_INTEGER freeBytesAvail{}, totalBytes{}, totalFreeBytes{};
    if (!::GetDiskFreeSpaceExW(rootPath, &freeBytesAvail, &totalBytes, &totalFreeBytes))
        return false;

    outFreeGB = (double)totalFreeBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
    outTotalGB = (double)totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
    return true;
}