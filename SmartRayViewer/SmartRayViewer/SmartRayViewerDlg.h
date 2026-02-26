// SmartRayViewerDlg.h
#pragma once

#include <afxcmn.h>

#include "vGridCtrl.h"
#include "vUtil.h"
#include "vImage.h"

#include "UIHelper.h"
#include "ColorDefine.h"
#include "TimerDefine.h"
#include "Common.h"

#include "ZMapRenderer.h"
#include "VtkPointCloudView.h"

#include "LogManager.h"
#include "DioDeviceManager.h"
#include "DlgLog.h"
#include "DlgParam.h"

#include "SmartRaySensor.h"
#include "Result.h"

constexpr double INVALID = -999999.0;

// UI thread로 전달하기 위한 custom message
constexpr UINT WM_PCFRAME_READY = WM_APP + 10; // cam0 pointcloud
constexpr UINT WM_ZMAP_READY = WM_APP + 11; // cam0 zmap
constexpr UINT WM_PC_THICKNESS_READY = WM_APP + 12; // thickness(pc format)

// ROI 통계 기준 데이터 선택
enum class RoiStatSource
{
    PointCloud, // PcFrame(points) 기반으로 ROI 통계
    ZMap16      // ZMap16(m_zmap) 기반으로 ROI 통계
};

// 센서 파라미터(앱 파라미터에서 읽어옴)
struct SensorParams
{
    int   profiles = 200;
    float xScale = 0.019f;

    // ✅ 센서별
    int exposure_s1 = 100;
    int brightnessTh_s1 = 10;
    int exposure_s2 = 100;
    int brightnessTh_s2 = 10;

    // 공통(트리거)
    int TriggerMode = 0;
    int TriggerFrequency = 25;
    int TriggerSource = 0;
    int TriggerDivider = 1;
    int TriggerDelay = 0;
    int TriggerDirection = 0;
};

// AppStore → SensorParams 로드
static SensorParams LoadSensorParams()
{
    SensorParams p;

    p.profiles = AppStore::Get().GetParameterAsInt("Sensor", "NumberOfProfiles");
    p.xScale = (float)AppStore::Get().GetParameterAsDouble("Sensor", "X_Scale");

    // ✅ 센서별로 읽기
    p.exposure_s1 = AppStore::Get().GetParameterAsInt("Sensor", "S1_ExposureTime");
    p.brightnessTh_s1 = AppStore::Get().GetParameterAsInt("Sensor", "S1_BrightnessThreshold");

    p.exposure_s2 = AppStore::Get().GetParameterAsInt("Sensor", "S2_ExposureTime");
    p.brightnessTh_s2 = AppStore::Get().GetParameterAsInt("Sensor", "S2_BrightnessThreshold");

    // 공통(트리거)
    p.TriggerMode = AppStore::Get().GetParameterAsInt("Sensor", "TriggerMode");
    p.TriggerFrequency = AppStore::Get().GetParameterAsInt("Sensor", "Trigger_Frequency");
    p.TriggerSource = AppStore::Get().GetParameterAsInt("Sensor", "Trigger_Source");
    p.TriggerDivider = AppStore::Get().GetParameterAsInt("Sensor", "Trigger_Divider");
    p.TriggerDelay = AppStore::Get().GetParameterAsInt("Sensor", "Trigger_Delay");
    p.TriggerDirection = AppStore::Get().GetParameterAsInt("Sensor", "Trigger_Direction");

    return p;
}

// SensorParams → SmartRaySensor 적용
static void ApplyToSensor(SmartRaySensor& s, int camIndex, const SensorParams& p)
{
    s.SetProfilesToCapture((uint32_t)p.profiles);
    s.SetMergeExpectedProfiles((uint32_t)p.profiles);
    s.SetXScale(p.xScale);

    // ✅ 센서별 exposure/threshold 적용
    if (camIndex == 0)
    {
        s.SetExposureTime(p.exposure_s1);
        s.SetLaserLineBrightnessThreshold(p.brightnessTh_s1);
    }
    else
    {
        s.SetExposureTime(p.exposure_s2);
        s.SetLaserLineBrightnessThreshold(p.brightnessTh_s2);
    }

    // 공통(트리거)
    s.SetTriggerMode(p.TriggerMode);
    s.SetTriggerFrequency(p.TriggerFrequency);
    s.SetTriggerSource(p.TriggerSource);
    s.SetTriggerDivider(p.TriggerDivider);
    s.SetTriggerDelay(p.TriggerDelay);
    s.SetTriggerDirection(p.TriggerDirection);
}



enum class MoveDir { Stop, Fwd, Bwd };
enum class AutoPhase { Idle, SwitchDeadTime, SegmentRun };
enum class StopPlan { None, StopAfterThisSegment, ReturnAfterFwd };




// ============================================================================
// CSmartRayViewerDlg
// - UI / ZMap Viewer / VTK PC Viewer
// - Sensor(2대) 연결 + Result 파이프라인 연결
// - ROI 통계(PC or ZMap16) 계산하여 grid 갱신
// ============================================================================
class CSmartRayViewerDlg : public CDialogEx
{
public:
    // -----------------------------
    // lifecycle
    // -----------------------------
    CSmartRayViewerDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_SMARTRAYVIEWER_DIALOG };
#endif

protected:
    // -----------------------------
    // MFC overrides
    // -----------------------------
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();

    // -----------------------------
    // MFC messages
    // -----------------------------
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

    DECLARE_MESSAGE_MAP()

private:
    // =========================================================================
    // UI init / timers
    // =========================================================================
    void InitClass();
    void InitLayout();
    void InitGrid();
    void ClearGridData();
    void InitImage();

    void SetTimers();
    void KillTimers();

    // =========================================================================
    // ZMap viewer
    // =========================================================================
    void InitZMapSliders(uint16_t mn, uint16_t mx);
    void InitZMapSliders(); // 슬라이더 Range는 항상 0~65535
    void UpdateZMapJet();
    void UpdateZMapValueLabels();

    void DrawZMapColorBar(CDC* pDC);
    COLORREF GetJetColor(double t);

    void DrawMonitoringSignalOnOff(int nCtrlID, COLORREF color);

private:
    // =========================================================================
    // Sensor / Result pipeline
    // =========================================================================
    void InitSensor();
    bool InitOneSensor(SmartRaySensor& sensor,
        int camIndex,
        const std::string& name,
        const std::string& ip,
        unsigned short port,
        const SensorParams& p);

    bool StartAcquisition();   // void -> bool
    void StopAcquisition();
    void BindResultCallbacks();  // Result → PostMessage(WM_xxx)

private:
    // =========================================================================
    // ROI stats
    // =========================================================================
    void ComputeRoiStats_AndFillGrid(); // 선택 분기 + grid 갱신
    bool ComputeRoiStats_FromPointCloud(const PcFrame& frame, bool rotateCW90);
    bool ComputeRoiStats_FromZMap(uint16_t invalidValue);
    void AddGridMeasureZ(int RoiNo, const RoiInfoData& st);

private:
    // =========================================================================
    // Custom messages (UI thread safe update)
    // =========================================================================
    afx_msg LRESULT OnPcFrameReady(WPARAM, LPARAM);
    afx_msg LRESULT OnZMapReady(WPARAM, LPARAM);
    afx_msg LRESULT OnPcThicknessReady(WPARAM, LPARAM);

public:
    // =========================================================================
    // UI buttons
    // =========================================================================
    afx_msg void OnBnClickedButtonLoadImg();
    afx_msg void OnBnClickedBtnMinimize();
    afx_msg void OnBnClickedBtnExit();
    afx_msg void OnBnClickedButtonResult();
    afx_msg void OnBnClickedButtonLoad3dData();
    afx_msg void OnBnClickedButtonStart();
    afx_msg void OnBnClickedButtonSetting();
    afx_msg void OnBnClickedButtonTopView();
    afx_msg void OnBnClickedButtonFrontView();
    afx_msg void OnBnClickedButtonSideLeftView();
    afx_msg void OnBnClickedButtonOpenFolder();

private:
    // =========================================================================
    // resources / basic
    // =========================================================================
    HICON m_hIcon = nullptr;
    vUtil _Util;

// 모터 관련
private:


    bool m_jogFwdOn = false;
    bool m_jogBwdOn = false;

    void Jog_AllOff();

    // 검사(센서) 상태
    bool m_measureAllowed = false; // 자동 중 측정을 켤 수 있는 상태인지

private:
    // =========================================================================
    // ZMap render + image UI
    // =========================================================================
    CZMapRenderer m_zmap;
    vImage _image{ IMG_WIDTH, IMG_HEIGHT, eImageDepth::Color, eImageModeUI::UI };

    int  m_lastZImgW = 0;
    int  m_lastZImgH = 0;
    bool m_hasZmap = false;

    uint16_t m_vmin = 1;
    uint16_t m_vmax = 65535;

private:
    // =========================================================================
    // ROI state
    // =========================================================================
    RoiStatSource m_roiSource = RoiStatSource::PointCloud; // 기본값
    bool          m_roiAutoUpdate = true;                      // 프레임 들어올 때 자동 계산

private:
    // =========================================================================
    // Grid / dialogs
    // =========================================================================
    vGridCtrl _vGridResult;

    DlgLog   _dlgLog;
    DlgParam* _dlgParam = nullptr;

private:
    // =========================================================================
    // UI controls
    // =========================================================================
    vLabel _vLabelLogo;
    vLabel _vLabelLogo2;
    vLabel _vLabelTitle;
    vLabel _vLabelVersion;
    vLabel _vLabelTime;
    vLabel _vLabelPCInfo;

    vLabel _vLabel3DTile;
    vLabel _vLabelZmapTitle;

    vIconButton _vBtnMinimize;
    vIconButton _vBtnExit;
    vIconButton _btnLoadImg;
    vIconButton _btnResult;
    vIconButton _btnLoad3DData;

    vIconButton _btnConnect;
    vIconButton _btnStart;
    vIconButton _btnSetting;

    vIconButton _btnAutoRotate;
    vIconButton _btnTopView;
    vIconButton _btnFrontView;
    vIconButton _btnSideLeftView;
    vIconButton _btnOpenFolder;
    vIconButton _btnSaveFile;
    vIconButton _btnLoadMat;

    vIconButton _btnFwdMove;
    vIconButton _btnBwdMove;

    vIconButton _btnAutoRange;

    vLabel _labelConnectSensor1;
    vLabel _labelConnectSensor2;

    CSliderCtrl m_sliderVmin;
    CSliderCtrl m_sliderVmax;
    vLabel _vLabelvMin;
    vLabel _vLabelvMax;

    CToolTipCtrl m_toolTip;
    CFont m_toolTipFont;
    void InitToolTips();

private:
    // =========================================================================
    // VTK viewer
    // =========================================================================
    CVtkPointCloudView m_vtkView;
    UINT_PTR m_timerRotate = 0;
    DWORD    m_lastTick = 0;

private:
    // =========================================================================
    // Sensors + Result
    // =========================================================================
    SmartRaySensor m_Sensor0;
    SmartRaySensor m_Sensor1;
    Result         m_result;
public:
    afx_msg void OnBnClickedButtonDataSave();
    afx_msg void OnBnClickedButtonSensorConnect();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    afx_msg void OnBnClickedButtonLoadMat();
    afx_msg void OnBnClickedButtonJogfwd();
    afx_msg void OnBnClickedButtonJogbwd();
    afx_msg void OnBnClickedButtonAutoRotate();
    afx_msg void OnBnClickedButtonAutoRange();
};
