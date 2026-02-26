#pragma once
#include <Windows.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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
// - Sensor 콜백(포인트클라우드) 수신
// - sanitize / last 보관
// - (옵션) ZMap 생성 + UI 콜백 + (옵션) PNG16 저장
// - (옵션) Thickness 계산(Top/Bottom frameNo 매칭) -> 비동기(vThreadPool)
// - (옵션) PointCloud ASC 저장(D/E 분산) -> vThreadPool
//
// AppStore 적용:
//   System/UpdateFrameViewer
//   Sensor/X_Scale, Y_Scale, Z_Scale
// ============================================================================
class Result
{
public:
    // -----------------------------
    // UI callbacks
    // -----------------------------
    using PcCb = std::function<void(std::shared_ptr<PcFrame>)>;     // Top PC UI 콜백
    using ZMapCb = std::function<void(std::shared_ptr<ZMapFrame>)>;   // ZMap UI 콜백
    using ThkCb = std::function<void(std::shared_ptr<PcFrame>)>;     // Thickness(PC형식) UI 콜백

public:
    // -----------------------------
    // lifecycle
    // -----------------------------
    Result(int saveThreadsPerDrive = 1);   // 드라이브별 저장 스레드 수(기본 1)
    ~Result();

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

public:
    // -----------------------------
    // input (Sensor -> Result)
    // -----------------------------
    // 센서 콜백에서 호출되는 entry.
    // 콜백 지연 방지를 위해 "가벼운 작업" 위주 + heavy는 thread pool로 넘김.
    void OnPointCloud(std::shared_ptr<PcFrame> frame);

public:
    // -----------------------------
    // callbacks (Result -> UI)
    // -----------------------------
    void SetPointCloudCallback(PcCb cb);
    void SetZMapCallback(ZMapCb cb);
    void SetThicknessCallback(ThkCb cb);

public:
    // -----------------------------
    // options / flags
    // -----------------------------
    void SetPCEnabled(bool on) { m_PCEnabled.store(on); }                 // PC UI 갱신 on/off
    void SetZMapEnabled(bool on) { m_zmapEnabled.store(on); }             // ZMap 생성/콜백 on/off
    void SetThicknessEnabled(bool on) { m_thkEnabled.store(on); }         // 두께 계산 on/off

    void SetSaveEnabled(bool on) { m_saveEnabled.store(on); }             // PC ASC 저장 on/off
    void SetThicknessSaveEnabled(bool on) { m_thkSaveEnabled.store(on); }
    void SetZMapSaveEnabled(bool on) { m_zmapSaveEnabled.store(on); }     // ZMap PNG 저장 on/off
    void SetUseAppStoreParams(bool on) { m_useAppStoreParams.store(on); } // AppStore 파라미터 사용 on/off

    void SetSaveRoots(const std::wstring& rootD, const std::wstring& rootE); // D/E 저장 루트 지정

    bool LoadBottomMatrixFromFile(const std::wstring& path);

    // thickness 라이브러리가 sensor handle을 요구하면, 센서 중 아무거나 1개 핸들 등록
    void RegisterAnySensorHandle(const SRSensor* sensor);

public:
    // -----------------------------
    // last data (thread-safe getter)
    // -----------------------------
    std::shared_ptr<PcFrame>   GetLastPcFrame(int camIndex) const;   // [0]=Top, [1]=Bottom, [2]=Thickness
    std::shared_ptr<ZMapFrame> GetLastZMapFrame(int camIndex) const; // [0]=cam0 ZMap, [1]=cam1 ZMap

    bool GetSaveEnabled() { return m_saveEnabled.load(); }             // PC ASC 저장 on/off

public:
    // -----------------------------
    // run-state reset
    // -----------------------------
    // 검사 시작/재시작 시 호출:
    // - 프레임 주기 카운터 초기화
    // - thickness 매칭 버퍼 초기화
    // - (옵션) last 프레임 초기화
    void ResetRunState(bool clearLastFrames = true);

private:
    // =========================================================================
    // OnPointCloud() sub-steps (읽기 흐름용)
    // =========================================================================
    int  GetUpdateFrameParam() const; // UpdateFrameViewer 읽기

    void StoreLastPcFrame(const std::shared_ptr<PcFrame>& frame); // lastPc[cam] 저장

    void HandleThicknessPairingAndEnqueue(
        const std::shared_ptr<PcFrame>& frame,
        int nUpdateFrame); // frameNo 매칭 + (주기 만족 시) 비동기 enqueue

    std::shared_ptr<ZMapFrame> BuildZMapAndNotify(
        const std::shared_ptr<PcFrame>& frame); // ZMap 생성 + last 저장 + UI 콜백

    bool ShouldProcessThisFrame(int cam, int nUpdateFrame); // cnt++ 후 주기 체크

    void HandleSaves(
        const std::shared_ptr<PcFrame>& frame,
        const std::shared_ptr<ZMapFrame>& zf,
        int driveIdx); // asc/zmap 저장 enqueue

    void HandleUiCallbacks(
        const std::shared_ptr<PcFrame>& frame,
        const std::shared_ptr<ZMapFrame>& zf); // pc 콜백(현재는 cam0만)

private:
    // =========================================================================
    // sanitize
    // =========================================================================
    void SanitizePointCloudInPlace(PcFrame& frame); // bad point 제거

private:
    // =========================================================================
    // save helpers
    // =========================================================================
    int  PickDriveIndexForSave(int camIndex); // 저장 드라이브 선택 정책
    void EnqueueSaveAsc(std::shared_ptr<PcFrame> frame, int driveIdx);
    void EnqueueSaveZMap(std::shared_ptr<ZMapFrame> zf, int driveIdx);

    bool SaveFrameAsAscToRoot(const std::wstring& root, const std::shared_ptr<PcFrame>& frame);
    bool SaveZMapAsPng16ToRoot(const std::wstring& root, const std::shared_ptr<ZMapFrame>& zf);

private:
    // =========================================================================
    // ZMap
    // =========================================================================
    bool MakeZMapFromPointCloud(
        const std::shared_ptr<PcFrame>& frame,
        std::shared_ptr<ZMapFrame>& outZf);

private:
    // =========================================================================
    // Thickness (heavy work)
    // =========================================================================
    // - Top/Bottom frameNo가 매칭된 pair만 들어옴
    // - bottom은 설치보정+z만 좌우반전 후 SDK Thickness 계산
    // - 반드시 worker thread에서 호출 권장
    void TryComputeThickness(
        std::shared_ptr<PcFrame> top,
        std::shared_ptr<PcFrame> bottom);

    Transform_Mat m_bottomInstallMat{};   // bottom 설치보정 매트릭스
    std::atomic<bool> m_hasBottomMat{ false };

    std::mutex m_matMtx;   // 혹시 런타임에 갱신할 가능성 대비

private:
    // =========================================================================
    // common util
    // =========================================================================
    vUtil _Util;

    // =========================================================================
    // flags
    // =========================================================================
    std::atomic<bool> m_PCEnabled{ false };
    std::atomic<bool> m_zmapEnabled{ true };
    std::atomic<bool> m_thkEnabled{ true };

    std::atomic<bool> m_saveEnabled{ false };
    std::atomic<bool> m_thkSaveEnabled{ false }; // 두께 PC 저장 on/off
    std::atomic<bool> m_zmapSaveEnabled{ false };
    std::atomic<bool> m_useAppStoreParams{ true };

    // =========================================================================
    // save system (D/E 분산)
    // =========================================================================
    std::array<std::wstring, 2> m_saveRoots = { L"D:\\Data", L"E:\\Data" };
    std::array<std::shared_ptr<vThreadPool>, 2> m_savePools;

    // =========================================================================
    // callbacks
    // =========================================================================
    std::mutex m_cbMtx;
    PcCb   m_pcCb;
    ZMapCb m_zmapCb;
    ThkCb  m_thkCb;

    // =========================================================================
    // last frames
    // =========================================================================
    mutable std::mutex m_lastMtx;
    std::shared_ptr<PcFrame>   m_lastPc[3]; // [0]=Top, [1]=Bottom, [2]=Thickness
    std::shared_ptr<ZMapFrame> m_lastZf[2]; // [0]=cam0, [1]=cam1

    // UI/저장 주기용 카운터 (cam별)
    std::atomic<uint64_t> m_viewCount[3] = { 0, 0, 0 };

    // =========================================================================
    // sensor handle (SDK 호출용)
    // =========================================================================
    std::mutex m_sensorMtx;
    const SRSensor* m_anySensor = nullptr;

    // =========================================================================
    // frame matching buffers (frameNo 기준)
    // =========================================================================
    mutable std::mutex m_pairMtx;
    std::unordered_map<uint64_t, std::shared_ptr<PcFrame>> m_topBuf;
    std::unordered_map<uint64_t, std::shared_ptr<PcFrame>> m_botBuf;
    size_t m_pairMaxHold = 100; // 매칭 지연/드랍 상황 고려해 조절

    // =========================================================================
    // thickness async worker
    // =========================================================================
    std::shared_ptr<vThreadPool> m_thkPool; // 두께 전용 풀(보통 1스레드)
    std::atomic<bool> m_thkBusy{ false };   // true면 새 두께 job 스킵(적체 방지)
};
