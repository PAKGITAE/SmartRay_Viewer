#include "pch.h"
#include "Result.h"
#include "GrabHelper.h"  // PcFrame, SR_3DPOINT
#include <cstdio>
#include <string>


bool Result::SaveFrameAsAsc(const std::shared_ptr<PcFrame>& frame)
{
    if (!frame) return false;

    std::lock_guard<std::mutex> lk(m_mtx);

    // 날짜 폴더 + Cam 폴더 분리
    // D:\Data\20260209\Sensor0
    std::wstring date = _Util.MakeTimestamp(vUtil::TimeUnit::Day, true, false);
    std::wstring camFolder = L"Sensor" + std::to_wstring(frame->camIndex);

    std::wstring outFolder = m_rootFolder + L"\\" + date + L"\\" + camFolder;
    _Util.CreateDirectories(outFolder);

    // 파일명 유니크: ms + cam + frameNo
    // 20260209_141530_123_Sensor0_00000012_pointcloud.asc
    std::wstring ts = _Util.MakeTimestamp(vUtil::TimeUnit::Millisecond, true, false);

    wchar_t nameBuf[256];
    swprintf_s(nameBuf, L"%s_cam%d_%08llu_pointcloud.asc",
        ts.c_str(),
        frame->camIndex,
        (unsigned long long)frame->frameNo);

    std::wstring filePath = outFolder + L"\\" + nameBuf;

    HANDLE h = CreateFileW(
        filePath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    // --- ASC 저장 (x y z) ---
    DWORD written = 0;
    std::string buf;
    buf.reserve(1 << 20);

    char line[128];

    for (size_t i = 0; i < frame->points.size(); ++i)
    {
        const auto& p = frame->points[i];
        int len = sprintf_s(line, sizeof(line),
            "%.4f %.7f %.7f\n",
            (double)p.x, (double)p.y, (double)p.z);

        buf.append(line, (size_t)len);

        if (buf.size() >= (1 << 20))
        {
            WriteFile(h, buf.data(), (DWORD)buf.size(), &written, nullptr);
            buf.clear();
        }
    }

    if (!buf.empty())
        WriteFile(h, buf.data(), (DWORD)buf.size(), &written, nullptr);

    CloseHandle(h);

    LogManager& logMgr = LogManager::GetInstance();
    std::wstring strLog = L"Save OK / cam=" + std::to_wstring(frame->camIndex) +
        L" frameNo=" + std::to_wstring(frame->frameNo) +
        L" count=" + std::to_wstring(frame->points.size());
    logMgr.PushLog(Log::Viewer, L"SaveFrameAsAsc", strLog);

    return true;
}

void Result::SetUiFrameCallback(std::function<void(std::shared_ptr<PcFrame>)> cb)
{
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_uiCb = std::move(cb);
}

void Result::OnFrameArrived(std::shared_ptr<PcFrame> frame)
{
    if (!frame) return;

    int nUpdateFrame = AppStore::Get().GetParameterAsInt("System", "UpdateFrameViewer");

    static std::atomic<uint64_t> viewCount{ 0 };

    if (frame->camIndex == 0)
    {
        if (++viewCount % nUpdateFrame == 0)
        {
            std::function<void(std::shared_ptr<PcFrame>)> cb;
            {
                std::lock_guard<std::mutex> lk(m_cbMtx);
                cb = m_uiCb;
            }
            if (cb) cb(frame);
        }
    }
  
    // 2) 저장 (원하면 저장 on/off 플래그로 제어 가능)
    SaveFrameAsAsc(frame);
}