#include "pch.h"
#include "Result.h"

#include <cstdio>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

// ------------------------------------------------------------
// (1) 이상값 판정
// ------------------------------------------------------------
static inline bool IsBadPoint(const SR_3DPOINT& p)
{
    constexpr float BAD_TH = -999000.0f;
    if (p.x < BAD_TH || p.y < BAD_TH || p.z < BAD_TH)
        return true;

    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        return true;

    constexpr float MAX_ABS = 1e6f;
    if (fabsf(p.x) > MAX_ABS || fabsf(p.y) > MAX_ABS || fabsf(p.z) > MAX_ABS)
        return true;

    return false;
}

Result::Result(int saveThreadsPerDrive)
{
    if (saveThreadsPerDrive <= 0) saveThreadsPerDrive = 1;

    // ✅ D/E 각각 독립 저장 풀
    m_savePools[0] = std::make_shared<vThreadPool>(saveThreadsPerDrive);
    m_savePools[1] = std::make_shared<vThreadPool>(saveThreadsPerDrive);
}

Result::~Result()
{
    // vThreadPool이 destructor에서 join/stop 처리한다고 가정.
    // 만약 vThreadPool에 Stop/Join API가 있다면 여기서 호출해줘야 함.
}

// ------------------------------------------------------------
// callbacks
// ------------------------------------------------------------
void Result::SetPointCloudCallback(PcCb cb)
{
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_pcCb = std::move(cb);
}
void Result::SetZMapCallback(ZMapCb cb)
{
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_zmapCb = std::move(cb);
}
void Result::SetThicknessCallback(ThkCb cb)
{
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_thkCb = std::move(cb);
}

void Result::SetSaveRoots(const std::wstring& rootD, const std::wstring& rootE)
{
    m_saveRoots[0] = rootD;
    m_saveRoots[1] = rootE;
}

void Result::RegisterAnySensorHandle(const SRSensor* sensor)
{
    if (!sensor) return;
    std::lock_guard<std::mutex> lk(m_sensorMtx);
    if (!m_anySensor)
        m_anySensor = sensor;
}

// ------------------------------------------------------------
// last getters
// ------------------------------------------------------------
std::shared_ptr<PcFrame> Result::GetLastPcFrame(int camIndex) const
{
    if (camIndex < 0 || camIndex >= 2) return nullptr;
    std::lock_guard<std::mutex> lk(m_lastMtx);
    return m_lastPc[camIndex];
}

std::shared_ptr<ZMapFrame> Result::GetLastZMapFrame(int camIndex) const
{
    if (camIndex < 0 || camIndex >= 2) return nullptr;
    std::lock_guard<std::mutex> lk(m_lastMtx);
    return m_lastZf[camIndex];
}

// ------------------------------------------------------------
// sanitize
// ------------------------------------------------------------
void Result::SanitizePointCloudInPlace(PcFrame& frame)
{
    auto& pts = frame.points;
    if (pts.empty()) return;

    const size_t before = pts.size();

    pts.erase(
        std::remove_if(pts.begin(), pts.end(),
            [](const SR_3DPOINT& p) { return IsBadPoint(p); }),
        pts.end()
    );

    const size_t after = pts.size();
    if (after != before)
    {
        std::wstring msg =
            L"Sanitize: cam=" + std::to_wstring(frame.camIndex) +
            L" frameNo=" + std::to_wstring(frame.frameNo) +
            L" removed=" + std::to_wstring(before - after) +
            L" remain=" + std::to_wstring(after);
        LogManager::GetInstance().PushLog(Log::Result, L"SanitizePointCloudInPlace", msg);
    }
}

// ------------------------------------------------------------
// drive select policy
// - 기본: round robin (D/E 번갈아)
// - cam 고정 원하면: return (camIndex & 1);
// ------------------------------------------------------------
int Result::PickDriveIndexForSave(int camIndex)
{
    return (camIndex & 1); // cam0->D, cam1->E
}

// ------------------------------------------------------------
// async enqueue (thread pool)
// ------------------------------------------------------------
void Result::EnqueueSaveAsc(std::shared_ptr<PcFrame> frame, int driveIdx)
{
    if (!frame) return;
    if (driveIdx < 0 || driveIdx > 1) driveIdx = 0;

    auto pool = m_savePools[driveIdx];
    auto root = m_saveRoots[driveIdx];

    pool->AddJob([this, root, frame]()
        {
            SaveFrameAsAscToRoot(root, frame);
        });
}

void Result::EnqueueSaveZMap(std::shared_ptr<ZMapFrame> zf, int driveIdx)
{
    if (!zf) return;
    if (driveIdx < 0 || driveIdx > 1) driveIdx = 0;

    auto pool = m_savePools[driveIdx];
    auto root = m_saveRoots[driveIdx];

    pool->AddJob([this, root, zf]()
        {
            SaveZMapAsPng16ToRoot(root, zf);
        });
}

// ------------------------------------------------------------
// main entry
// ------------------------------------------------------------
void Result::OnPointCloud(std::shared_ptr<PcFrame> frame)
{
    if (!frame) return;

    // (A) sanitize
    SanitizePointCloudInPlace(*frame);

    // cam index validate
    const int cam = frame->camIndex;
    if (cam < 0 || cam >= 2) return;

    // (B) last pc store
    {
        std::lock_guard<std::mutex> lk(m_lastMtx);
        m_lastPc[cam] = frame;
    }

    // (C) thickness
    if (m_thkEnabled.load())
        TryComputeThickness();

    // (D) AppStore params
    int nUpdateFrame = 1;
    if (m_useAppStoreParams.load())
    {
        nUpdateFrame = AppStore::Get().GetParameterAsInt("System", "UpdateFrameViewer");
        if (nUpdateFrame <= 0) nUpdateFrame = 1;
    }

    const uint64_t cnt = ++m_viewCount[cam];

    // (E) ZMap (cam0만)
    std::shared_ptr<ZMapFrame> zf;
    if (m_zmapEnabled.load() && cam == 0)
    {
        if (MakeZMapFromPointCloud(frame, zf))
        {
            {
                std::lock_guard<std::mutex> lk(m_lastMtx);
                m_lastZf[cam] = zf;
            }

            ZMapCb zcb;
            {
                std::lock_guard<std::mutex> lk(m_cbMtx);
                zcb = m_zmapCb;
            }
            if (zcb) zcb(zf);
        }
    }

    // (F) update frame 정책
    if (cnt % (uint64_t)nUpdateFrame != 0)
        return;

    // ✅ 저장 드라이브 선택(분산)
    const int driveIdx = PickDriveIndexForSave(cam);

    // (G) save async (ASC)
    if (m_saveEnabled.load())
        EnqueueSaveAsc(frame, driveIdx);

    // (G-2) ZMap PNG 저장(옵션)
    if (m_zmapSaveEnabled.load() && zf && !zf->z.empty())
        EnqueueSaveZMap(zf, driveIdx);

    // (H) UI PC callback (cam0만 표시)
    if (cam == 0)
    {
        PcCb pcb;
        {
            std::lock_guard<std::mutex> lk(m_cbMtx);
            pcb = m_pcCb;
        }
        if (pcb) pcb(frame);
    }
}

// ------------------------------------------------------------
// Save ASC to root
// ------------------------------------------------------------
bool Result::SaveFrameAsAscToRoot(const std::wstring& root, const std::shared_ptr<PcFrame>& frame)
{
    if (!frame) return false;

    std::wstring date = _Util.MakeTimestamp(vUtil::TimeUnit::Day, true, false);
    std::wstring camFolder = L"Sensor" + std::to_wstring(frame->camIndex);

    std::wstring outFolder = root + L"\\" + date + L"\\" + camFolder + L"\\PointCloud";
    _Util.CreateDirectories(outFolder);

    std::wstring ts = _Util.MakeTimestamp(vUtil::TimeUnit::Millisecond, true, false);

    wchar_t nameBuf[256];
    swprintf_s(nameBuf, L"%s_Sensor%d_%08llu_Pointcloud.asc",
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

    DWORD written = 0;
    std::string buf;
    buf.reserve(1 << 20);

    char line[128];

    for (const auto& p : frame->points)
    {
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

    LogManager::GetInstance().PushLog(
       Log::Result,
        L"SaveFrameAsAscToRoot",
        L"Save OK root=" + root +
        L" cam=" + std::to_wstring(frame->camIndex) +
        L" frameNo=" + std::to_wstring(frame->frameNo) +
        L" count=" + std::to_wstring(frame->points.size())
    );

    return true;
}

// ------------------------------------------------------------
// Save ZMap PNG16 to root (async job)
// ------------------------------------------------------------
bool Result::SaveZMapAsPng16ToRoot(const std::wstring& root, const std::shared_ptr<ZMapFrame>& zf)
{
    if (!zf || zf->z.empty() || zf->w <= 0 || zf->h <= 0)
        return false;

    std::wstring date = _Util.MakeTimestamp(vUtil::TimeUnit::Day, true, false);
    std::wstring camFolder = L"Sensor" + std::to_wstring(zf->camIndex);

    std::wstring outFolder = root + L"\\" + date + L"\\" + camFolder + L"\\Zmap";
    _Util.CreateDirectories(outFolder);

    std::wstring ts = _Util.MakeTimestamp(vUtil::TimeUnit::Millisecond, true, false);

    wchar_t nameBuf[256];
    swprintf_s(nameBuf, L"%s_cam%d_%08llu_zmap16.png",
        ts.c_str(),
        zf->camIndex,
        (unsigned long long)zf->frameNo);

    std::wstring filePath = outFolder + L"\\" + nameBuf;

    // OpenCV는 wide path 직접 못 받는 경우가 많아 ANSI 변환
    CStringW cw(filePath.c_str());
    CStringA ca(cw);

    cv::Mat img(zf->h, zf->w, CV_16UC1, (void*)zf->z.data());
    bool ok = cv::imwrite(std::string(ca.GetString()), img);

    if (ok)
    {
        LogManager::GetInstance().PushLog(
           Log::Result,
            L"SaveZMapAsPng16ToRoot",
            L"Save ZMap OK root=" + root +
            L" cam=" + std::to_wstring(zf->camIndex) +
            L" frameNo=" + std::to_wstring(zf->frameNo) +
            L" size=" + std::to_wstring(zf->w) + L"x" + std::to_wstring(zf->h)
        );
    }
    return ok;
}

// ------------------------------------------------------------
// ZMap convert (AppStore X/Y/Z scale 적용)
// ------------------------------------------------------------
bool Result::MakeZMapFromPointCloud(
    const std::shared_ptr<PcFrame>& frame,
    std::shared_ptr<ZMapFrame>& outZf)
{
    outZf.reset();
    if (!frame || frame->points.empty())
        return false;

    float lateralRes = 0.019f;
    float transportRes = 0.019f;
    float verticalRes = 0.001f;

    if (m_useAppStoreParams.load())
    {
        lateralRes = (float)AppStore::Get().GetParameterAsDouble("Sensor", "X_Scale");
        transportRes = (float)AppStore::Get().GetParameterAsDouble("Sensor", "Y_Scale");
        verticalRes = (float)AppStore::Get().GetParameterAsDouble("Sensor", "Z_Scale");
    }

    if (lateralRes <= 0.f || transportRes <= 0.f || verticalRes <= 0.f)
        return false;

    ZMapInfo info{};
    SR_Result rcInfo = SR_GetZMapImageInfo(
        frame->points.data(),
        (int)frame->points.size(),
        &info,
        lateralRes,
        transportRes
    );
    if (rcInfo != SR_Result::Success)
        return false;

    const int w = (int)info.width;
    const int h = (int)info.height;
    if (w <= 0 || h <= 0)
        return false;

    Transform_Mat mat{};
    mat.m[0][0] = 1.0f;
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;

    // ✅ 사용자 요청: 이 라인은 유지
    uint16_t* outZ = new uint16_t[info.width * info.height];

    SR_Result rc = SR_ConvertPointCloudToZMap_CPU(
        const_cast<SR_3DPOINT*>(frame->points.data()),
        (int)frame->points.size(),
        lateralRes,
        transportRes,
        verticalRes,
        &outZ,
        mat,
        &info
    );

    if (rc != SR_Result::Success || !outZ)
    {
        // 혹시라도 new된 outZ가 살아있다면 해제
        delete[] outZ;
        return false;
    }

    auto zf = std::make_shared<ZMapFrame>();
    zf->camIndex = frame->camIndex;
    zf->frameNo = frame->frameNo;
    zf->w = w;
    zf->h = h;
    zf->invalid = 0; // 프로젝트 규약에 맞게 필요 시 세팅

    zf->z.assign(outZ, outZ + (size_t)w * (size_t)h);

    // ✅ new[]와 짝: delete[]
    delete[] outZ;

    outZf = std::move(zf);
    return true;
}

// ------------------------------------------------------------
// Thickness
// ------------------------------------------------------------
void Result::TryComputeThickness()
{
    std::shared_ptr<PcFrame> top;
    std::shared_ptr<PcFrame> bottom;
    const SRSensor* sensor = nullptr;

    {
        std::lock_guard<std::mutex> lk(m_lastMtx);
        top = m_lastPc[0];
        bottom = m_lastPc[1];
    }
    {
        std::lock_guard<std::mutex> lk(m_sensorMtx);
        sensor = m_anySensor;
    }

    if (!top || !bottom || !sensor) return;
    if (top->points.empty() || bottom->points.empty()) return;

    std::vector<SR_3DPOINT> bottomPts = bottom->points;

    Transform_Mat mat{};
    mat.m[0][0] = 1.0f;
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;

    SR_Result rcMat = ApplyMatrix(bottomPts.data(), (int)bottomPts.size(), mat);

    const int cap = (int)std::min(top->points.size(), bottomPts.size());
    std::vector<double> outBuf(cap);
    double* out = outBuf.data();
    int outCount = cap;

    // 필요하면 이것도 AppStore로 빼도 됨
    SR_Result rc = ThicknessMeasurment(
        sensor,
        top->points.data(), (int)top->points.size(),
        bottomPts.data(), (int)bottomPts.size(),
        &out,
        &outCount,
        0.019,
        40.0
    );

    if (rc != SR_Result::Success || !out || outCount <= 0)
        return;

    ThicknessFrame tf;
    tf.topFrameNo = top->frameNo;
    tf.bottomFrameNo = bottom->frameNo;
    tf.t.assign(out, out + outCount);

    ThkCb cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_thkCb;
    }
    if (cb) cb(tf);
}
