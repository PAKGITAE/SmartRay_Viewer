#include "pch.h"
#include "Result.h"

#include <algorithm>
#include <cstdio>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

// ============================================================================
// File-local helpers (static)
// ============================================================================
static inline bool IsBadPoint(const SR_3DPOINT& p)
{
    constexpr float BAD_TH = -999000.0f;
    if (p.x < BAD_TH || p.y < BAD_TH || p.z < BAD_TH) return true;

    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return true;

    constexpr float MAX_ABS = 1e6f;
    if (fabsf(p.x) > MAX_ABS || fabsf(p.y) > MAX_ABS || fabsf(p.z) > MAX_ABS) return true;

    return false;
}

// bottom 센서 데이터에서 "z만" 좌우 반전 (x,y 불변)
static inline void MirrorZOnly_PerProfile(std::vector<SR_3DPOINT>& pts, uint32_t numProfiles)
{
    if (pts.empty() || numProfiles == 0) return;

    const size_t N = pts.size();
    if (N % numProfiles != 0) return;

    const size_t cols = N / numProfiles;

    for (size_t r = 0; r < numProfiles; ++r)
    {
        size_t rowBase = r * cols;
        for (size_t c = 0; c < cols / 2; ++c)
        {
            size_t L = rowBase + c;
            size_t R = rowBase + (cols - 1 - c);
            std::swap(pts[L].z, pts[R].z);
        }
    }
}

// ============================================================================
// Lifecycle
// ============================================================================
Result::Result(int saveThreadsPerDrive)
{
    if (saveThreadsPerDrive <= 0) saveThreadsPerDrive = 1;

    // D/E 각각 독립 저장 풀
    m_savePools[0] = std::make_shared<vThreadPool>(saveThreadsPerDrive);
    m_savePools[1] = std::make_shared<vThreadPool>(saveThreadsPerDrive);

    // 두께 전용 풀 (보통 1개 스레드 권장)
    m_thkPool = std::make_shared<vThreadPool>(1);
}

Result::~Result()
{
    // vThreadPool destructor에서 join/stop 처리한다고 가정
}

// ============================================================================
// Public: callback setters / options
// ============================================================================
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
    if (!m_anySensor) m_anySensor = sensor;
}

// ============================================================================
// Public: getters / reset
// ============================================================================
std::shared_ptr<PcFrame> Result::GetLastPcFrame(int camIndex) const
{
    if (camIndex < 0 || camIndex >= 3) return nullptr;
    std::lock_guard<std::mutex> lk(m_lastMtx);
    return m_lastPc[camIndex];
}

std::shared_ptr<ZMapFrame> Result::GetLastZMapFrame(int camIndex) const
{
    if (camIndex < 0 || camIndex >= 2) return nullptr;
    std::lock_guard<std::mutex> lk(m_lastMtx);
    return m_lastZf[camIndex];
}

void Result::ResetRunState(bool clearLastFrames)
{
    // (1) 주기 카운터 초기화
    m_viewCount[0].store(0);
    m_viewCount[1].store(0);
    m_viewCount[2].store(0);

    // (2) thickness busy 해제
    m_thkBusy.store(false);

    // (3) frameNo 매칭 버퍼 비우기
    {
        std::lock_guard<std::mutex> lk(m_pairMtx);
        m_topBuf.clear();
        m_botBuf.clear();
    }

    // (4) last frames 초기화(옵션)
    if (clearLastFrames)
    {
        std::lock_guard<std::mutex> lk(m_lastMtx);
        m_lastPc[0].reset();
        m_lastPc[1].reset();
        m_lastPc[2].reset();

        m_lastZf[0].reset();
        m_lastZf[1].reset();
    }

    LogManager::GetInstance().PushLog(Log::Result, L"ResetRunState", L"Reset OK");
}

// ============================================================================
// Public: main entry (Sensor -> Result)
// ============================================================================
void Result::OnPointCloud(std::shared_ptr<PcFrame> frame)
{
    if (!frame) return;

    // (0) 로그(가볍게)
    {
        std::wstring msg =
            L"Callback: cam=" + std::to_wstring(frame->camIndex) +
            L" frameNo=" + std::to_wstring(frame->frameNo) +
            L" count=" + std::to_wstring(frame->points.size());
        LogManager::GetInstance().PushLog(Log::Result, L"OnPointCloud", msg);
    }

    // (1) UpdateFrameViewer 파라미터
    const int nUpdateFrame = GetUpdateFrameParam();

    // (2) sanitize
    SanitizePointCloudInPlace(*frame);

    // (3) cam validate
    const int cam = frame->camIndex;
    if (cam < 0 || cam >= 2) return;

    // (4) last 저장
    StoreLastPcFrame(frame);

    // (5) thickness 매칭 + (주기 만족 시) 비동기 enqueue
    HandleThicknessPairingAndEnqueue(frame, nUpdateFrame);

    // (6) zmap 생성/콜백 (cam0만)
    auto zf = BuildZMapAndNotify(frame);

    // (7) update frame 정책(저장/UI 주기)
    if (!ShouldProcessThisFrame(cam, nUpdateFrame))
        return;

    // (8) 저장
    const int driveIdx = PickDriveIndexForSave(cam);
    HandleSaves(frame, zf, driveIdx);

    // (9) UI PC 콜백
    HandleUiCallbacks(frame, zf);
}

// ============================================================================
// Private: OnPointCloud sub-steps
// ============================================================================
int Result::GetUpdateFrameParam() const
{
    int n = 1;
    if (m_useAppStoreParams.load())
    {
        n = AppStore::Get().GetParameterAsInt("System", "UpdateFrameViewer");
        if (n <= 0) n = 1;
    }
    return n;
}

void Result::StoreLastPcFrame(const std::shared_ptr<PcFrame>& frame)
{
    const int cam = frame->camIndex;
    std::lock_guard<std::mutex> lk(m_lastMtx);
    m_lastPc[cam] = frame;
}

void Result::HandleThicknessPairingAndEnqueue(const std::shared_ptr<PcFrame>& frame, int nUpdateFrame)
{
    if (!m_thkEnabled.load()) return;

    const int cam = frame->camIndex;
    std::shared_ptr<PcFrame> topMatch, botMatch;

    {
        std::lock_guard<std::mutex> lk(m_pairMtx);

        // cam0=top, cam1=bottom 가정
        if (cam == 0) m_topBuf[frame->frameNo] = frame;
        else          m_botBuf[frame->frameNo] = frame;

        // 같은 frameNo가 모였으면 pair 완성
        auto itT = m_topBuf.find(frame->frameNo);
        auto itB = m_botBuf.find(frame->frameNo);
        if (itT != m_topBuf.end() && itB != m_botBuf.end())
        {
            topMatch = itT->second;
            botMatch = itB->second;
            m_topBuf.erase(itT);
            m_botBuf.erase(itB);
        }

        // 버퍼 청소(너무 오래 쌓이면 메모리 증가)
        while (m_topBuf.size() > m_pairMaxHold)
        {
            auto oldest = std::min_element(
                m_topBuf.begin(), m_topBuf.end(),
                [](auto& a, auto& b) { return a.first < b.first; });
            m_topBuf.erase(oldest);
        }
        while (m_botBuf.size() > m_pairMaxHold)
        {
            auto oldest = std::min_element(
                m_botBuf.begin(), m_botBuf.end(),
                [](auto& a, auto& b) { return a.first < b.first; });
            m_botBuf.erase(oldest);
        }
    }

    // 매칭 실패면 종료
    if (!(topMatch && botMatch)) return;

    const uint64_t fn = topMatch->frameNo;

    // ✅ nUpdateFrame 주기일 때만 두께 계산
    if ((fn % (uint64_t)nUpdateFrame) != 0)
        return;

    // ✅ 적체 방지: 밀리면 스킵(최신 위주 정책)
    if (m_thkBusy.exchange(true))
        return;

    m_thkPool->AddJob([this, topMatch, botMatch]()
        {
            TryComputeThickness(topMatch, botMatch);
            m_thkBusy.store(false);
        });
}

std::shared_ptr<ZMapFrame> Result::BuildZMapAndNotify(const std::shared_ptr<PcFrame>& frame)
{
    const int cam = frame->camIndex;

    // 현재는 cam0만 ZMap 생성
    if (!m_zmapEnabled.load() || cam != 0)
        return nullptr;

    std::shared_ptr<ZMapFrame> zf;
    if (!MakeZMapFromPointCloud(frame, zf))
        return nullptr;

    // last 저장
    {
        std::lock_guard<std::mutex> lk(m_lastMtx);
        m_lastZf[cam] = zf;
    }

    // UI 콜백
    ZMapCb zcb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        zcb = m_zmapCb;
    }
    if (zcb) zcb(zf);

    return zf;
}

bool Result::ShouldProcessThisFrame(int cam, int nUpdateFrame)
{
    const uint64_t cnt = ++m_viewCount[cam];
    return (cnt % (uint64_t)nUpdateFrame) == 0;
}

void Result::HandleSaves(const std::shared_ptr<PcFrame>& frame,
    const std::shared_ptr<ZMapFrame>& zf,
    int driveIdx)
{
    if (m_saveEnabled.load())
        EnqueueSaveAsc(frame, driveIdx);

    if (m_zmapSaveEnabled.load() && zf && !zf->z.empty())
        EnqueueSaveZMap(zf, driveIdx);
}

void Result::HandleUiCallbacks(const std::shared_ptr<PcFrame>& frame,
    const std::shared_ptr<ZMapFrame>& /*zf*/)
{
    if (!m_PCEnabled.load())
        return;

    // UI는 cam0만 전달
    const int cam = frame->camIndex;
    if (cam != 0) return;

    PcCb pcb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        pcb = m_pcCb;
    }
    if (pcb) pcb(frame);
}

// ============================================================================
// Private: sanitize
// ============================================================================
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

// ============================================================================
// Private: save (enqueue / IO)
// ============================================================================
int Result::PickDriveIndexForSave(int camIndex)
{
    // cam0->D, cam1->E (고정 분산)
    return (camIndex & 1);
}

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

bool Result::SaveFrameAsAscToRoot(const std::wstring& root, const std::shared_ptr<PcFrame>& frame)
{
    if (!frame) return false;

    std::wstring date = _Util.MakeTimestamp(vUtil::TimeUnit::Day, true, false);
    std::wstring camFolder;
    if (frame->camIndex == 2) camFolder = L"Thickness";
    else camFolder = L"Sensor" + std::to_wstring(frame->camIndex);

    std::wstring outFolder = root + L"\\" + date + L"\\" + camFolder + L"\\PointCloud";
    _Util.CreateDirectories(outFolder);

    std::wstring ts = _Util.MakeTimestamp(vUtil::TimeUnit::Millisecond, true, false);

    wchar_t nameBuf[256];
    if (frame->camIndex == 2)
        swprintf_s(nameBuf, L"%s_Thickness_%08llu_Pointcloud.asc", ts.c_str(), (unsigned long long)frame->frameNo);
    else
        swprintf_s(nameBuf, L"%s_Sensor%d_%08llu_Pointcloud.asc", ts.c_str(), frame->camIndex, (unsigned long long)frame->frameNo);

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

// ============================================================================
// Private: ZMap convert
// ============================================================================
bool Result::MakeZMapFromPointCloud(const std::shared_ptr<PcFrame>& frame,
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

    {
        std::lock_guard<std::mutex> lk(m_matMtx);
        if (m_hasBottomMat.load())
            mat = m_bottomInstallMat;
        else
        {
            // fallback: identity
            mat.m[0][0] = 1.0f;
            mat.m[1][1] = 1.0f;
            mat.m[2][2] = 1.0f;
            mat.m[3][3] = 1.0f;
        }
    }

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
        delete[] outZ;
        return false;
    }

    auto zf = std::make_shared<ZMapFrame>();
    zf->camIndex = frame->camIndex;
    zf->frameNo = frame->frameNo;
    zf->w = w;
    zf->h = h;
    zf->invalid = 0;

    zf->z.assign(outZ, outZ + (size_t)w * (size_t)h);
    delete[] outZ;

    outZf = std::move(zf);
    return true;
}

// ============================================================================
// Private: Thickness
// ============================================================================
void Result::TryComputeThickness(std::shared_ptr<PcFrame> top, std::shared_ptr<PcFrame> bottom)
{
    const SRSensor* sensor = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_sensorMtx);
        sensor = m_anySensor;
    }

    if (!top || !bottom || !sensor) return;
    if (top->points.empty() || bottom->points.empty()) return;

    // bottom 복사본에 보정/반전 적용
    std::vector<SR_3DPOINT> bottomPts = bottom->points;

    Transform_Mat mat{};

    {
        std::lock_guard<std::mutex> lk(m_matMtx);
        if (m_hasBottomMat.load())
            mat = m_bottomInstallMat;
        else
        {
            // fallback: identity
            mat.m[0][0] = 1.0f;
            mat.m[1][1] = 1.0f;
            mat.m[2][2] = 1.0f;
            mat.m[3][3] = 1.0f;
        }
    }

    ApplyMatrix(bottomPts.data(), (int)bottomPts.size(), mat);
    MirrorZOnly_PerProfile(bottomPts, bottom->numProfiles);

    // out 결과 버퍼 (top size 기준으로 넉넉히)
    double* thickness_results = new double[top->points.size()];
    int out_num_results = 0;

    SR_Result rc = ThicknessMeasurment(
        sensor,
        top->points.data(), (int)top->points.size(),
        bottomPts.data(), (int)bottomPts.size(),
        &thickness_results,
        &out_num_results,
        0.019,
        40.0
    );

    if (rc != SR_Result::Success || !thickness_results || out_num_results <= 0)
    {
        delete[] thickness_results;
        return;
    }

    int n = std::min(out_num_results, (int)top->points.size());

    // Thickness를 PcFrame 형태로 만들어 UI에 전달
    auto thkPc = std::make_shared<PcFrame>();
    thkPc->camIndex = 2;
    thkPc->frameNo = top->frameNo;
    thkPc->numPoints = (uint32_t)n;
    thkPc->numProfiles = top->numProfiles;
    thkPc->points.resize(n);

    for (int i = 0; i < n; ++i)
    {
        SR_3DPOINT p{};
        p.x = top->points[i].x;
        p.y = top->points[i].y;
        p.z = (float)thickness_results[i];
        thkPc->points[i] = p;
    }

    delete[] thickness_results;

    // last thickness 저장
    {
        std::lock_guard<std::mutex> lk(m_lastMtx);
        m_lastPc[2] = thkPc;
    }

    // ✅ thickness 포인트클라우드도 저장
    if (m_saveEnabled.load() && m_thkSaveEnabled.load())
    {
        // camIndex=2 이므로 PickDriveIndexForSave(2) => 0 (D드라이브로 저장됨)
        const int driveIdx = PickDriveIndexForSave(thkPc->camIndex);
        EnqueueSaveAsc(thkPc, driveIdx);
    }

    // UI 콜백
    ThkCb cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_thkCb;
    }
    if (cb) cb(thkPc);
}

bool Result::LoadBottomMatrixFromFile(const std::wstring& path)
{
    std::lock_guard<std::mutex> lk(m_matMtx);

    std::wifstream fin(path);
    if (!fin.is_open())
        return false;

    double v[16] = {};
    for (int i = 0; i < 16; ++i)
    {
        if (!(fin >> v[i]))
            return false;
    }

    int k = 0;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            m_bottomInstallMat.m[r][c] = (float)v[k++];

    m_hasBottomMat.store(true);

    LogManager::GetInstance().PushLog(
        Log::Result,
        L"LoadBottomMatrixFromFile",
        L"Matrix Loaded OK"
    );

    return true;
}