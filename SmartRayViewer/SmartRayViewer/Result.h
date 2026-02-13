#pragma once
#include <Windows.h>

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <cmath>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <deque>
#include <algorithm>
#include <array>

#include "vUtil.h"
#include "AppStore.h"
#include "LogManager.h"
#include "vThreadPool.h"

#include "SR_ZMapConverter.h"
#include "SR_Thickness_Measurement.h"

#include "Types.h"
#include "SR_API_public.h"

// ============================================================================
// Result
// - SmartRaySensor -> PcFrame 수신
// - sanitize / last 보관
// - (옵션) ZMap 생성 + UI 콜백 + (옵션) PNG16 저장
// - (옵션) Thickness 계산
// - (옵션) PointCloud ASC 저장 (D/E 분산) : vThreadPool 사용
//
// AppStore 적용:
//   System/UpdateFrameViewer
//   Sensor/X_Scale, Y_Scale, Z_Scale
// ============================================================================
class Result
{
public:
    using PcCb = std::function<void(std::shared_ptr<PcFrame>)>;
    using ZMapCb = std::function<void(std::shared_ptr<ZMapFrame>)>;
    using ThkCb = std::function<void(const ThicknessFrame&)>;

    Result(int saveThreadsPerDrive = 1);   // 드라이브별 저장 스레드 수(기본 1)
    ~Result();

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    // ---- input ----
    void OnPointCloud(std::shared_ptr<PcFrame> frame);

    // ---- callbacks (Result -> UI) ----
    void SetPointCloudCallback(PcCb cb);
    void SetZMapCallback(ZMapCb cb);
    void SetThicknessCallback(ThkCb cb);

    // ---- options ----
    void SetZMapEnabled(bool on) { m_zmapEnabled.store(on); }
    void SetThicknessEnabled(bool on) { m_thkEnabled.store(on); }

    // ✅ D/E 저장 루트 분산
    void SetSaveRoots(const std::wstring& rootD, const std::wstring& rootE);

    void SetSaveEnabled(bool on) { m_saveEnabled.store(on); }
    void SetZMapSaveEnabled(bool on) { m_zmapSaveEnabled.store(on); } // ✅ 추가
    void SetUseAppStoreParams(bool on) { m_useAppStoreParams.store(on); }


        // thickness용: 어떤 센서든 하나만 등록하면 됨
    void RegisterAnySensorHandle(const SRSensor* sensor);

    // ---- last data ----
    std::shared_ptr<PcFrame>    GetLastPcFrame(int camIndex) const;
    std::shared_ptr<ZMapFrame>  GetLastZMapFrame(int camIndex) const;

private:
    // ---- sanitize ----
    void SanitizePointCloudInPlace(PcFrame& frame);

    // ---- async save (thread pool) ----
    int  PickDriveIndexForSave(int camIndex);
    void EnqueueSaveAsc(std::shared_ptr<PcFrame> frame, int driveIdx);
    void EnqueueSaveZMap(std::shared_ptr<ZMapFrame> zf, int driveIdx);

    bool SaveFrameAsAscToRoot(const std::wstring& root, const std::shared_ptr<PcFrame>& frame);
    bool SaveZMapAsPng16ToRoot(const std::wstring& root, const std::shared_ptr<ZMapFrame>& zf);

    // ---- zmap ----
    bool MakeZMapFromPointCloud(const std::shared_ptr<PcFrame>& frame,
        std::shared_ptr<ZMapFrame>& outZf);

    // ---- thickness ----
    void TryComputeThickness();

private:
    vUtil _Util;

    // ✅ D/E 저장 루트
    std::array<std::wstring, 2> m_saveRoots = { L"D:\\Data", L"E:\\Data" };

    // drive별 저장용 thread pool (D / E)
    std::array<std::shared_ptr<vThreadPool>, 2> m_savePools;

    // 분산 정책: round-robin (cam 고정 원하면 PickDriveIndexForSave에서 바꾸면 됨)
    std::atomic<uint64_t> m_rr{ 0 };

    std::atomic<bool> m_saveEnabled{ true };
    std::atomic<bool> m_zmapEnabled{ true };
    std::atomic<bool> m_thkEnabled{ true };
    std::atomic<bool> m_zmapSaveEnabled{ false };   // ✅ ZMap 저장 on/off
    std::atomic<bool> m_useAppStoreParams{ true };

    // callbacks sync
    std::mutex m_cbMtx;
    PcCb   m_pcCb;
    ZMapCb m_zmapCb;
    ThkCb  m_thkCb;

    // last frames
    mutable std::mutex m_lastMtx;
    std::shared_ptr<PcFrame>   m_lastPc[2];
    std::shared_ptr<ZMapFrame> m_lastZf[2];

    // cam별 update count
    std::atomic<uint64_t> m_viewCount[2] = { 0, 0 };

    // thickness needs any sensor handle
    std::mutex m_sensorMtx;
    const SRSensor* m_anySensor = nullptr;
};
