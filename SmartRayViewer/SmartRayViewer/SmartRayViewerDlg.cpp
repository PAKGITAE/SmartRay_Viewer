// SmartRayViewerDlg.cpp
#include "pch.h"
#include "framework.h"
#include "SmartRayViewer.h"
#include "SmartRayViewerDlg.h"
#include "afxdialogex.h"

#include <algorithm>
#include <fstream>
#include <vector>
#include <filesystem>
#include <regex>

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// local utils
// ============================================================================

// 파일명에서 SensorN / frameNo 파싱
static bool ParseSensorAndFrameNoFromFilename(
    const std::wstring& filename,
    int& outCam,
    uint64_t& outFrameNo)
{
    outCam = 0;
    outFrameNo = 0;

    // 예: 20260212_221711738_Sensor1_00000002_Pointcloud.asc
    std::wregex re(LR"(Sensor(\d+).*?_(\d+)_Pointcloud)", std::regex::icase);
    std::wsmatch m;
    if (!std::regex_search(filename, m, re))
        return false;

    try
    {
        outCam = std::stoi(m[1].str());
        outFrameNo = (uint64_t)std::stoull(m[2].str());
        return true;
    }
    catch (...)
    {
        return false;
    }
}

// (참고) pixel mapping helper (현재 ComputeRoiStats_FromPointCloud에서 직접 계산을 쓰고 있음)
// 필요하면 여기 함수로 통일 가능
static inline bool MapPointToPixel(
    float x, float y,
    int W, int H,
    bool rotateCW90,
    int& outX, int& outY,
    float xmin, float xmax, float ymin, float ymax,
    bool looksLikePixel)
{
    int ix = 0, iy = 0;

    if (looksLikePixel)
    {
        ix = (int)std::lround(x);
        iy = (int)std::lround(y);
    }
    else
    {
        const double dx = (double)(xmax - xmin);
        const double dy = (double)(ymax - ymin);
        if (dx == 0.0 || dy == 0.0) return false;

        const double nx = (x - xmin) / dx; // 0..1
        const double ny = (y - ymin) / dy; // 0..1

        ix = (int)std::lround(nx * (W - 1));
        iy = (int)std::lround(ny * (H - 1));
    }

    // y flip
    iy = (H - 1) - iy;

    // CW90 rotate (pixel space)
    if (rotateCW90)
    {
        int rx = (H - 1) - iy;
        int ry = ix;
        ix = rx;
        iy = ry;
    }

    if (ix < 0 || ix >= W || iy < 0 || iy >= H) return false;

    outX = ix;
    outY = iy;
    return true;
}

// ============================================================================
// About dialog
// ============================================================================
class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX) {}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// ============================================================================
// CSmartRayViewerDlg
// ============================================================================

CSmartRayViewerDlg::CSmartRayViewerDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_SMARTRAYVIEWER_DIALOG, pParent)
    , _dlgLog(this)
    , _dlgParam(nullptr)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSmartRayViewerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);

    // labels
    DDX_Control(pDX, IDC_LABEL_LOGO, _vLabelLogo);
    DDX_Control(pDX, IDC_LABEL_LOGO2, _vLabelLogo2);
    DDX_Control(pDX, IDC_LABEL_NAME, _vLabelTitle);
    DDX_Control(pDX, IDC_LABEL_TIME, _vLabelTime);
    DDX_Control(pDX, IDC_LABEL_VERSION, _vLabelVersion);
    DDX_Control(pDX, IDC_LABEL_PC_INFO, _vLabelPCInfo);
    DDX_Control(pDX, IDC_LABEL_INSPECT_INFO, _vLabelViewInfo);
    DDX_Control(pDX, IDC_LABEL_LINE, _vLabelLine);
    
    
    

    DDX_Control(pDX, IDC_LABEL_3D_VIEW_TITLE, _vLabel3DTile);
    DDX_Control(pDX, IDC_LABEL_ZMAP_VIEW_TITLE, _vLabelZmapTitle);

    // window buttons
    DDX_Control(pDX, IDC_BTN_MINIMIZE, _vBtnMinimize);
    DDX_Control(pDX, IDC_BTN_EXIT, _vBtnExit);

    // main buttons
    DDX_Control(pDX, IDC_BUTTON_LOAD_IMG, _btnLoadImg);
    DDX_Control(pDX, IDC_BUTTON_RESULT, _btnResult);
    DDX_Control(pDX, IDC_BUTTON_LOAD_3D_DATA, _btnLoad3DData);

    // sensor connect labels
    DDX_Control(pDX, IDC_LABEL_SENSOR_CONNECT_1, _labelConnectSensor1);
    DDX_Control(pDX, IDC_LABEL_SENSOR_CONNECT_2, _labelConnectSensor2);

    // zmap sliders
    DDX_Control(pDX, IDC_SLIDER_VMIN, m_sliderVmin);
    DDX_Control(pDX, IDC_SLIDER_VMAX, m_sliderVmax);
    DDX_Control(pDX, IDC_LABEL_VMIN, _vLabelvMin);
    DDX_Control(pDX, IDC_LABEL_VMAX, _vLabelvMax);

    // views
    DDX_Control(pDX, IDC_IMAGE_VIEW, _image);
    DDX_Control(pDX, IDC_CUSTOM_RESULT_GRID, _vGridResult);

    // start/stop/setting
    DDX_Control(pDX, IDC_BUTTON_START, _btnStart);
    DDX_Control(pDX, IDC_BUTTON_SETTING, _btnSetting);
    DDX_Control(pDX, IDC_BUTTON_DATA_SAVE, _btnSaveFile);
    DDX_Control(pDX, IDC_BUTTON_SENSOR_CONNECT, _btnConnect);
    DDX_Control(pDX, IDC_BUTTON_LOAD_MAT, _btnLoadMat);
    
    DDX_Control(pDX, IDC_BUTTON_JOGFWD, _btnFwdMove);
    DDX_Control(pDX, IDC_BUTTON_JOGBWD, _btnBwdMove);

    DDX_Control(pDX, IDC_BUTTON_AUTO_RANGE, _btnAutoRange);
    

    // vtk view buttons
    DDX_Control(pDX, IDC_BUTTON_AUTO_ROTATE, _btnAutoRotate);
    DDX_Control(pDX, IDC_BUTTON_TOP_VIEW, _btnTopView);
    DDX_Control(pDX, IDC_BUTTON_FRONT_VIEW, _btnFrontView);
    DDX_Control(pDX, IDC_BUTTON_SIDE_LEFT_VIEW, _btnSideLeftView);
    DDX_Control(pDX, IDC_BUTTON_OPEN_FOLDER, _btnOpenFolder);
}

BEGIN_MESSAGE_MAP(CSmartRayViewerDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_ERASEBKGND()
    ON_WM_TIMER()
    ON_WM_HSCROLL()
    ON_WM_DESTROY()

    // buttons
    ON_BN_CLICKED(IDC_BUTTON_LOAD_IMG, &CSmartRayViewerDlg::OnBnClickedButtonLoadImg)
    ON_BN_CLICKED(IDC_BTN_MINIMIZE, &CSmartRayViewerDlg::OnBnClickedBtnMinimize)
    ON_BN_CLICKED(IDC_BTN_EXIT, &CSmartRayViewerDlg::OnBnClickedBtnExit)
    ON_BN_CLICKED(IDC_BUTTON_RESULT, &CSmartRayViewerDlg::OnBnClickedButtonResult)
    ON_BN_CLICKED(IDC_BUTTON_LOAD_3D_DATA, &CSmartRayViewerDlg::OnBnClickedButtonLoad3dData)
    ON_BN_CLICKED(IDC_BUTTON_START, &CSmartRayViewerDlg::OnBnClickedButtonStart)
    ON_BN_CLICKED(IDC_BUTTON_SETTING, &CSmartRayViewerDlg::OnBnClickedButtonSetting)
    ON_BN_CLICKED(IDC_BUTTON_TOP_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonTopView)
    ON_BN_CLICKED(IDC_BUTTON_FRONT_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonFrontView)
    ON_BN_CLICKED(IDC_BUTTON_SIDE_LEFT_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonSideLeftView)
    ON_BN_CLICKED(IDC_BUTTON_OPEN_FOLDER, &CSmartRayViewerDlg::OnBnClickedButtonOpenFolder)

    // custom messages
    ON_MESSAGE(WM_PCFRAME_READY, &CSmartRayViewerDlg::OnPcFrameReady)
    ON_MESSAGE(WM_ZMAP_READY, &CSmartRayViewerDlg::OnZMapReady)
    ON_MESSAGE(WM_PC_THICKNESS_READY, &CSmartRayViewerDlg::OnPcThicknessReady)
    ON_BN_CLICKED(IDC_BUTTON_DATA_SAVE, &CSmartRayViewerDlg::OnBnClickedButtonDataSave)
    ON_BN_CLICKED(IDC_BUTTON_SENSOR_CONNECT, &CSmartRayViewerDlg::OnBnClickedButtonSensorConnect)
    ON_BN_CLICKED(IDC_BUTTON_LOAD_MAT, &CSmartRayViewerDlg::OnBnClickedButtonLoadMat)
    ON_BN_CLICKED(IDC_BUTTON_JOGFWD, &CSmartRayViewerDlg::OnBnClickedButtonJogfwd)
    ON_BN_CLICKED(IDC_BUTTON_JOGBWD, &CSmartRayViewerDlg::OnBnClickedButtonJogbwd)
    ON_BN_CLICKED(IDC_BUTTON_AUTO_ROTATE, &CSmartRayViewerDlg::OnBnClickedButtonAutoRotate)
    ON_BN_CLICKED(IDC_BUTTON_AUTO_RANGE, &CSmartRayViewerDlg::OnBnClickedButtonAutoRange)
END_MESSAGE_MAP()

// ============================================================================
// init / destroy
// ============================================================================
BOOL CSmartRayViewerDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // system menu "About"
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr)
    {
        BOOL bNameValid;
        CString strAboutMenu;
        bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
        ASSERT(bNameValid);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    // icon
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // init (UI)
    InitClass();
    InitLayout();
    InitGrid();
    InitImage();
    SetTimers();

    // init (sensor/result)
    InitSensor();

    DioDeviceManager::Instance().OutBit(DioOutputBit::MoveFwd, false);
    DioDeviceManager::Instance().OutBit(DioOutputBit::MoveBwd, false);
    Sleep(500);
    DioDeviceManager::Instance().OutBit(DioOutputBit::MovePowerOff, false);

    // init (vtk view)
    m_vtkView1.Init(GetDlgItem(IDC_VTK_VIEW)->GetSafeHwnd());
    m_vtkView1.ResizeToHost();

    m_vtkView2.Init(GetDlgItem(IDC_VTK_VIEW2)->GetSafeHwnd());
    m_vtkView2.ResizeToHost();

    m_lastMode = GetSensorMode();
    ApplyVtkLayoutByMode(m_lastMode);

    LogManager::GetInstance().PushLog(Log::Main, L"OnInitDialog", L"PGM START");
    return TRUE;
}

void CSmartRayViewerDlg::OnDestroy()
{
    KillTimers();

    // callbacks 해제
    m_result.SetPointCloudCallback(nullptr);
    m_result.SetZMapCallback(nullptr);
    m_result.SetThicknessCallback(nullptr);

    // sensor disconnect
    m_Sensor0.Disconnect();
    m_Sensor1.Disconnect();

    DioDeviceManager::Instance().OutBit(DioOutputBit::MoveFwd, false);
    DioDeviceManager::Instance().OutBit(DioOutputBit::MoveBwd, false);
    Sleep(500);
    DioDeviceManager::Instance().OutBit(DioOutputBit::MovePowerOff, false);

    CDialogEx::OnDestroy();
}

// ============================================================================
// basic MFC handlers
// ============================================================================
void CSmartRayViewerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

void CSmartRayViewerDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);

        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();

        // 덧그리기(컬러바)
        CClientDC dc(this);
        DrawZMapColorBar(&dc);
    }
}

HCURSOR CSmartRayViewerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

BOOL CSmartRayViewerDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect rect;
    GetClientRect(rect);
    pDC->FillSolidRect(rect, AppColor::RGB_WEAK_BK_COLOR);
    return TRUE;
}

void CSmartRayViewerDlg::OnOK() { /* block Enter */ }
void CSmartRayViewerDlg::OnCancel() { /* block Esc   */ }

// ============================================================================
// timer
// ============================================================================
void CSmartRayViewerDlg::SetTimers()
{
    SetTimer(TimerID::UpdateTime, 500, NULL);
    SetTimer(TimerID::UpdateConnect, 500, NULL);

    SetTimer(TimerID::ZMapScan, 1000, NULL);
}

void CSmartRayViewerDlg::KillTimers()
{
    KillTimer(TimerID::UpdateTime);
    KillTimer(TimerID::UpdateConnect);

    KillTimer(TimerID::ZMapScan);
}

void CSmartRayViewerDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TimerID::UpdateTime)
    {
        // ======================
        // 시간 표시
        // ======================
        std::wstring wStrTime =
            _Util.MakeTimestamp(vUtil::TimeUnit::Second, false, false);

        _vLabelTime.SetText(wStrTime);
        _vLabelTime.Draw();

        // ======================
        // System CPU
        // ======================
        double cpu = GetCpuUsagePercent();

        // ======================
        // System Memory
        // ======================
        double memUsedPct = 0, memUsedGB = 0, memTotalGB = 0;
        bool memOk = GetMemoryUsage(memUsedPct, memUsedGB, memTotalGB);

        // ======================
        // Drives
        // ======================
        double cFree = 0, cTotal = 0;
        double dFree = 0, dTotal = 0;
        double eFree = 0, eTotal = 0;

        bool cOk = GetDriveUsageGB(L"C:\\", cFree, cTotal);
        bool dOk = GetDriveUsageGB(L"D:\\", dFree, dTotal);
        bool eOk = GetDriveUsageGB(L"E:\\", eFree, eTotal);

        // ======================
        // Text formatting (English)
        // ======================
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(0);

        ss << L"- CPU: " << cpu << L"%\n";

        if (memOk)
            ss << L"- Memory: " << memUsedPct << L"%\n";
        else
            ss << L"- Memory: NA\n\n";

        if (cOk)
            ss << L"- C Drive: " << cFree << L"/" << cTotal << L" GB Free\n";
        else
            ss << L"- C Drive: NA\n";

        if (dOk)
            ss << L"- D Drive: " << dFree << L"/" << dTotal << L" GB Free\n";
        else
            ss << L"- D Drive: NA\n";

        if (eOk)
            ss << L"- E Drive: " << eFree << L"/" << eTotal << L" GB Free\n";
        else
            ss << L"- E Drive: NA\n";

        _vLabelPCInfo.SetText(ss.str());
        _vLabelPCInfo.Draw();
        
    }
    else if (nIDEvent == TimerID::UpdateConnect)
    {
        // =========================
    // Sensor #1
    // =========================
        std::string ip1 = AppStore::Get().GetParameterAsString("Sensor", "Sensor1_IP");
        short port1 = (short)AppStore::Get().GetParameterAsInt("Sensor", "Sensor1_Port");

        std::wstring text1 =
            L"Sensor#1 (" +
            std::wstring(ip1.begin(), ip1.end()) +
            L" / " +
            std::to_wstring(port1) +
            L")";

        _labelConnectSensor1.SetText(text1);
        _labelConnectSensor1.Draw();

        DrawMonitoringSignalOnOff(
            IDC_SIGNAL_SENSOR_CONNECT_1,
            m_Sensor0.IsConnected() ? AppColor::RGB_GREEN : AppColor::RGB_RED);


        // =========================
        // Sensor #2
        // =========================
        std::string ip2 = AppStore::Get().GetParameterAsString("Sensor", "Sensor2_IP");
        short port2 = (short)AppStore::Get().GetParameterAsInt("Sensor", "Sensor2_Port");

        const SensorMode mode = GetSensorMode();
        if (!NeedSensor1(mode))
        {
            _labelConnectSensor2.SetText(L"Sensor#2 (N/A)");
            _labelConnectSensor2.Draw();
            DrawMonitoringSignalOnOff(IDC_SIGNAL_SENSOR_CONNECT_2, AppColor::RGB_GRAY);
        }
        else
        {
            std::wstring text2 =
                L"Sensor#2 (" +
                std::wstring(ip2.begin(), ip2.end()) +
                L" / " +
                std::to_wstring(port2) +
                L")";

            _labelConnectSensor2.SetText(text2);
            _labelConnectSensor2.Draw();

            DrawMonitoringSignalOnOff(
                IDC_SIGNAL_SENSOR_CONNECT_2,
                m_Sensor1.IsConnected() ? AppColor::RGB_GREEN : AppColor::RGB_RED);
        }
    }

    if (nIDEvent == TimerID::AutoRotate)
    {
        DWORD now = ::GetTickCount64();
        double dt = (now - m_lastTick) / 1000.0;
        m_lastTick = now;
        if (dt > 0.1) dt = 0.1;

        const SensorMode mode = GetSensorMode();
        m_vtkView1.TickAutoRotate(dt);
        if (mode == SensorMode::Dual)
            m_vtkView2.TickAutoRotate(dt);

        return;
    }

    if (nIDEvent == TimerID::ZMapScan)
    {
        if (m_zWinAnimOn)
            TickZMapWindowAnim();
        return;
    }

    CDialogEx::OnTimer(nIDEvent);
}


// ============================================================================
// UI buttons
// ============================================================================
void CSmartRayViewerDlg::OnBnClickedBtnMinimize()
{
    ShowWindow(SW_MINIMIZE);
}

void CSmartRayViewerDlg::OnBnClickedBtnExit()
{
    int result = AfxMessageBox(L"프로그램을 종료하시겠습니까?", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (result == IDYES)
    {
        m_result.SetPointCloudCallback(nullptr);
        m_result.SetZMapCallback(nullptr);
        m_result.SetThicknessCallback(nullptr);

        CDialogEx::OnCancel();
    }
}

// ZMap file load (PNG/JPG/etc)
void CSmartRayViewerDlg::OnBnClickedButtonLoadImg()
{
    LogManager& logMgr = LogManager::GetInstance();

    CFileDialog dlg(TRUE, L"png", nullptr,
        OFN_FILEMUSTEXIST,
        L"Image Files (*.bmp;*.jpg;*.jpeg;*.png)|*.bmp;*.jpg;*.jpeg;*.png||");

    if (dlg.DoModal() != IDOK)
        return;

    CString path = dlg.GetPathName();

    if (!m_zmap.Load(path))
    {
        OutputDebugString(L"[ZMAP] load failed\n");
        logMgr.PushLog(Log::Main, L"OnBnClickedButtonLoadImg", L"[ZMAP] load failed");
        return;
    }

    // min/max
    uint16_t dataMin = 1, dataMax = 65535;
    uint16_t invalidValue = 0;
    if (!m_zmap.GetDataMinMax(dataMin, dataMax, invalidValue))
    {
        OutputDebugString(L"[ZMAP] min/max failed\n");
        logMgr.PushLog(Log::Main, L"OnBnClickedButtonLoadImg", L"[ZMAP] min/max failed");
        return;
    }

    // vImage init
    _image.Init(m_zmap.Width(), m_zmap.Height(), 24);

    //InitZMapSliders(dataMin, dataMax);
    InitZMapSliders(dataMin, dataMax);;

    CaptureBaseVminVmax();

    m_hasZmap = true;

    UpdateZMapJet();
}

// ROI stats button
void CSmartRayViewerDlg::OnBnClickedButtonResult()
{
    ComputeRoiStats_AndFillGrid();
}

// 3D data file load (ply/asc/xyz/txt)
void CSmartRayViewerDlg::OnBnClickedButtonLoad3dData()
{
    if (m_Sensor0.IsRunning() || m_Sensor1.IsRunning())
    {
        AfxMessageBox(L"측정중입니다.\n측정을 멈춘 후 데이터를 불러올 수 있습니다.");
        return;
    }

    CFileDialog dlg(TRUE, L"ply", nullptr,
        OFN_FILEMUSTEXIST,
        L"Point Cloud (*.ply;*.asc;*.xyz;*.txt)|*.ply;*.asc;*.xyz;*.txt|All Files (*.*)|*.*||",
        this);

    if (dlg.DoModal() != IDOK)
        return;

    CString path = dlg.GetPathName();

    // file name parse (SensorN / frameNo)
    CString fnameC = dlg.GetFileName();
    std::wstring fname = (LPCWSTR)fnameC;

    int camFromName = 0;
    uint64_t frameNoFromName = 0;
    bool okParse = ParseSensorAndFrameNoFromFilename(fname, camFromName, frameNoFromName);

    // file → PcFrame
    auto frame = m_vtkView1.LoadPointCloudToFrame(path, okParse ? camFromName : 0);
    if (!frame)
    {
        AfxMessageBox(L"포인트 클라우드 로드 실패\n(확장자/포맷/데이터를 확인해 주세요)");
        return;
    }

    // frameNo
    frame->frameNo = okParse ? frameNoFromName : 0;

    // Result pipeline 통과(= ZMap / thickness / save / UI)
    m_result.OnPointCloud(frame);
}

// start / stop
void CSmartRayViewerDlg::OnBnClickedButtonStart()
{
    auto& log = LogManager::GetInstance();
    const SensorMode mode = GetSensorMode();

    // ✅ 연타 방지: 버튼 잠깐 Hide
    CWnd* pBtn = GetDlgItem(IDC_BUTTON_START);
    if (pBtn) pBtn->ShowWindow(FALSE);

    auto EnableBtn = [pBtn]() {
        if (pBtn) pBtn->ShowWindow(TRUE);
        };

    log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[REQ] " + ModeToText(mode));

    // 1) 연결 확인 (모드에서 필요한 센서만)
    if (NeedSensor0(mode) && !m_Sensor0.IsConnected())
    {
        log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[FAIL] Sensor0 not connected");
        AfxMessageBox(L"Sensor0 연결 상태를 확인해주세요.");
        EnableBtn();
        return;
    }
    if (NeedSensor1(mode) && !m_Sensor1.IsConnected())
    {
        log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[FAIL] Sensor1 not connected");
        AfxMessageBox(L"Sensor1 연결 상태를 확인해주세요.");
        EnableBtn();
        return;
    }

    // 2) 이미 실행 중이면 -> Stop (모드 기반)
    const bool anyRunning =
        (NeedSensor0(mode) && m_Sensor0.IsRunning()) ||
        (NeedSensor1(mode) && m_Sensor1.IsRunning());

    if (anyRunning)
    {
        log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[REQ] Stop");

        StopAcquisition(); // <= 아래에서 모드 기반으로 수정할 것

        // Stop 후 상태 재확인(모드 기반)
        const bool stillRunning =
            (NeedSensor0(mode) && m_Sensor0.IsRunning()) ||
            (NeedSensor1(mode) && m_Sensor1.IsRunning());

        if (stillRunning)
        {
            log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[FAIL] Stop failed");
            AfxMessageBox(L"정지에 실패했습니다.\n센서 상태를 확인해주세요.");

            UIHelper::InitIconButton(_btnStart, L"", L"Stop.png", 28, true,
                AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
        }
        else
        {
            log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[OK] Stopped");

            UIHelper::InitIconButton(_btnStart, L"", L"Start.png", 28, true,
                AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
        }

        EnableBtn();
        return;
    }

    // 3) 실행 중이 아니면 -> Start
    log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[REQ] Start");

    // ✅ Start 직전 파라미터 적용(모드에서 필요한 센서만)
    const SensorParams p = LoadSensorParams();
    if (NeedSensor0(mode)) ApplyRuntimeToSensor(m_Sensor0, 0, p);
    if (NeedSensor1(mode)) ApplyRuntimeToSensor(m_Sensor1, 1, p);

    const bool ok = StartAcquisition(); // <= 아래에서 모드 기반으로 수정할 것

    if (ok)
    {
        // Start 후 상태 확인(모드 기반)
        const bool runningNow =
            (!NeedSensor0(mode) || m_Sensor0.IsRunning()) &&
            (!NeedSensor1(mode) || m_Sensor1.IsRunning());

        if (runningNow)
        {
            log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[OK] Started");

            UIHelper::InitIconButton(_btnStart, L"", L"Stop.png", 28, true,
                AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
        }
        else
        {
            log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[FAIL] Start mismatch");
            AfxMessageBox(L"시작 요청은 성공했으나 실행 상태 확인에 실패했습니다.\n센서 상태를 확인해주세요.");

            UIHelper::InitIconButton(_btnStart, L"", L"Start.png", 28, true,
                AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);

            if (NeedSensor0(mode)) m_Sensor0.Stop();
            if (NeedSensor1(mode)) m_Sensor1.Stop();
        }
    }
    else
    {
        log.PushLog(Log::Main, L"OnBnClickedButtonStart", L"[FAIL] Start failed");

        std::wstring msg = L"시작에 실패했습니다.\n\n";
        if (NeedSensor0(mode))
            msg += L"- Sensor0: " + std::wstring(m_Sensor0.IsRunning() ? L"RUN" : L"FAIL") + L"\n";
        if (NeedSensor1(mode))
            msg += L"- Sensor1: " + std::wstring(m_Sensor1.IsRunning() ? L"RUN" : L"FAIL") + L"\n";
        msg += L"\n로그를 확인해주세요.";

        AfxMessageBox(msg.c_str());

        UIHelper::InitIconButton(_btnStart, L"", L"Start.png", 28, true,
            AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    }

    EnableBtn();
}

void CSmartRayViewerDlg::OnBnClickedButtonSetting()
{
    if (_dlgParam->IsWindowVisible()) {
        _dlgParam->ShowWindow(SW_HIDE);
    }
    else {
        _dlgParam->ShowWindow(SW_SHOW);
    }
}

void CSmartRayViewerDlg::OnBnClickedButtonOpenFolder()
{
    namespace fs = std::filesystem;

    const std::wstring date = _Util.MakeTimestamp(vUtil::TimeUnit::Day, true, false);
    fs::path folder = L"D:\\Data\\" + date;

    if (!fs::exists(folder))
        fs::create_directories(folder);

    ShellExecute(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CSmartRayViewerDlg::OnBnClickedButtonDataSave()
{
    int nType = AppStore::Get().GetParameterAsInt("System", "Thickness_View");

    if (m_result.GetSaveEnabled() == false) {
        m_result.SetSaveEnabled(true);
        if(nType == 2)
            m_result.SetThicknessSaveEnabled(true);
        UIHelper::InitIconButton(_btnSaveFile, L"", L"Save_stop.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    }
    else {
        m_result.SetSaveEnabled(false);
        if (nType == 1 || nType == 0)
            m_result.SetThicknessSaveEnabled(false);
        UIHelper::InitIconButton(_btnSaveFile, L"", L"Save.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    }

}

void CSmartRayViewerDlg::OnBnClickedButtonSensorConnect()
{
    LogManager& logMgr = LogManager::GetInstance();
    const SensorMode mode = GetSensorMode();

    // ✅ (선택) 연타 방지: Connect 버튼 잠깐 Disable
    CWnd* pBtn = GetDlgItem(IDC_BUTTON_SENSOR_CONNECT);
    if (pBtn) pBtn->ShowWindow(FALSE);
    auto EnableBtn = [pBtn]() { if (pBtn) pBtn->ShowWindow(TRUE); };

    logMgr.PushLog(Log::Result, L"SensorConnect", L"[REQ] " + ModeToText(mode));

    const bool rollbackOnFail = false;

    // 0) 이미 연결 중이면 -> 해제 (모드 기준)
    if (m_Sensor0.IsConnected() || m_Sensor1.IsConnected())
    {
        if (NeedSensor0(mode) && m_Sensor0.IsConnected()) m_Sensor0.Disconnect();
        if (NeedSensor1(mode) && m_Sensor1.IsConnected()) m_Sensor1.Disconnect();

        UIHelper::InitIconButton(_btnConnect, L"", L"DisConnect.png", 28, true,
            AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);

        logMgr.PushLog(Log::Result, L"SensorConnect", L"[OK] Disconnected by mode");
        AfxMessageBox(L"센서 연결 해제 완료");
        EnableBtn();
        return;
    }

    // 1) 파라미터 로드
    const SensorParams p = LoadSensorParams();

    // thickness 모드에서만 매트릭스 강제 로드(권장)
    if (mode == SensorMode::Thickness)
    {
        m_result.LoadBottomMatrixFromFile(L"C:\\Forvis\\matrix.txt");
        logMgr.PushLog(Log::Result, L"SensorConnect", L"Bottom matrix loaded for thickness mode");
    }

    std::vector<ConnectAttemptResult> results;
    results.reserve(2);

    bool okAll = true;

    // 2) Sensor0 connect attempt (필수)
    if (NeedSensor0(mode))
    {
        std::string ip0 = AppStore::Get().GetParameterAsString("Sensor", "Sensor1_IP");
        short port0 = (short)AppStore::Get().GetParameterAsInt("Sensor", "Sensor1_Port");

        ConnectAttemptResult r;
        r.cam = 0; r.name = "Sensor0"; r.ip = ip0; r.port = (unsigned short)port0;

        const DWORD t0 = GetTickCount64();
        r.ok = InitOneSensor(m_Sensor0, r.cam, r.name, r.ip, r.port, p);
        r.elapsedMs = GetTickCount64() - t0;
        if (!r.ok) r.errText = m_Sensor0.GetLastErrorText();

        logMgr.PushLog(Log::Result, L"SensorConnect", BuildLogLine(r));
        okAll &= r.ok;
        results.push_back(std::move(r));
    }

    // 3) Sensor1 connect attempt (모드 1/2에서만)
    if (NeedSensor1(mode))
    {
        std::string ip1 = AppStore::Get().GetParameterAsString("Sensor", "Sensor2_IP");
        short port1 = (short)AppStore::Get().GetParameterAsInt("Sensor", "Sensor2_Port");

        ConnectAttemptResult r;
        r.cam = 1; r.name = "Sensor1"; r.ip = ip1; r.port = (unsigned short)port1;

        const DWORD t0 = GetTickCount64();
        r.ok = InitOneSensor(m_Sensor1, r.cam, r.name, r.ip, r.port, p);
        r.elapsedMs = GetTickCount64() - t0;
        if (!r.ok) r.errText = m_Sensor1.GetLastErrorText();

        logMgr.PushLog(Log::Result, L"SensorConnect", BuildLogLine(r));
        okAll &= r.ok;
        results.push_back(std::move(r));
    }

    // 4) 실패 시 롤백 옵션
    if (!okAll && rollbackOnFail)
    {
        if (m_Sensor0.IsConnected()) m_Sensor0.Disconnect();
        if (m_Sensor1.IsConnected()) m_Sensor1.Disconnect();
        logMgr.PushLog(Log::Result, L"SensorConnect", L"Rollback on fail: disconnected");
    }

    // 5) UI 반영
    const bool connectedNow =
        (NeedSensor0(mode) && m_Sensor0.IsConnected()) ||
        (NeedSensor1(mode) && m_Sensor1.IsConnected());

    UIHelper::InitIconButton(_btnConnect, L"", connectedNow ? L"Connect.png" : L"DisConnect.png",
        28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);

    // 6) 사용자 메시지(요약 1회)
    const std::wstring summary = BuildSummaryMessage(results);
    AfxMessageBox(summary.c_str(), MB_OK | (okAll ? MB_ICONINFORMATION : MB_ICONERROR));

    EnableBtn();
}
void CSmartRayViewerDlg::OnBnClickedButtonLoadMat()
{
    // txt만 보이게 (필요하면 *.* 추가)
    CFileDialog dlg(
        TRUE,                       // TRUE=Open
        L"txt",                     // default ext
        nullptr,
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        L"Matrix Files (*.txt)|*.txt|All Files (*.*)|*.*||",
        this
    );

    if (dlg.DoModal() != IDOK)
        return;

    CStringW cpath = dlg.GetPathName();
    std::wstring path(cpath.GetString());

    // ✅ Result 인스턴스에 로드
    // (예: 멤버가 m_result 라고 가정)
    if (!m_result.LoadBottomMatrixFromFile(path))
    {
        LogManager::GetInstance().PushLog(Log::Main, L"LoadMatrix", L"Matrix load FAILED");
        AfxMessageBox(L"매트릭스 파일 로드에 실패했습니다.\n형식(4x4 숫자 16개)을 확인하세요.");
        return;
    }

    LogManager::GetInstance().PushLog(Log::Main, L"LoadMatrix", L"Matrix load OK: " + path);
    AfxMessageBox(L"매트릭스 파일 로드 완료!");
}



// vtk view controls
void CSmartRayViewerDlg::OnBnClickedButtonTopView()
{ 
    m_vtkView1.ViewTop();
    m_vtkView2.ViewTop();
}
void CSmartRayViewerDlg::OnBnClickedButtonFrontView() 
{ 
    m_vtkView1.ViewFront();
    m_vtkView2.ViewFront();
}
void CSmartRayViewerDlg::OnBnClickedButtonSideLeftView() 
{ 
    m_vtkView1.ViewSide();
    m_vtkView2.ViewSide();
}

// ============================================================================
// scroll (zmap vmin/vmax)
// ============================================================================
void CSmartRayViewerDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (!pScrollBar)
    {
        CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
        return;
    }

    int id = pScrollBar->GetDlgCtrlID();
    if (id == IDC_SLIDER_VMIN) m_vmin = (uint16_t)m_sliderVmin.GetPos();
    if (id == IDC_SLIDER_VMAX) m_vmax = (uint16_t)m_sliderVmax.GetPos();

    // min/max 역전 방지
    if (m_vmin >= m_vmax)
    {
        if (id == IDC_SLIDER_VMIN) m_vmin = (uint16_t)(m_vmax - 1);
        else                      m_vmax = (uint16_t)(m_vmin + 1);

        m_sliderVmin.SetPos((int)m_vmin);
        m_sliderVmax.SetPos((int)m_vmax);
    }

    UpdateZMapValueLabels();

    bool update =
        (nSBCode == SB_ENDSCROLL) ||
        (nSBCode == SB_THUMBPOSITION) ||
        (nSBCode == SB_THUMBTRACK) ||
        (nSBCode == SB_LINELEFT) || (nSBCode == SB_LINERIGHT) ||
        (nSBCode == SB_PAGELEFT) || (nSBCode == SB_PAGERIGHT);

    if (update) UpdateZMapJet();

    CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}

// ============================================================================
// Result → UI custom message handlers
// ============================================================================
LRESULT CSmartRayViewerDlg::OnPcFrameReady(WPARAM, LPARAM lParam)
{
    auto* pFramePtr = reinterpret_cast<std::shared_ptr<PcFrame>*>(lParam);
    std::shared_ptr<PcFrame> frame = (pFramePtr ? *pFramePtr : nullptr);
    delete pFramePtr;

    if (!frame || frame->points.empty())
        return 0;

    // cam0 pointcloud 표시
    if (frame->camIndex == 0)
        m_vtkView1.UpdatePointCloud(frame->points);
    else if (frame->camIndex == 1)
        m_vtkView2.UpdatePointCloud(frame->points);

    if (m_roiAutoUpdate)
        ComputeRoiStats_AndFillGrid();

    LogManager& logMgr = LogManager::GetInstance();
    logMgr.PushLog(Log::Main, L"OnPcFrameReady",
        L"PointCloud Update / frameNo=" + std::to_wstring(frame->frameNo) +
        L" count=" + std::to_wstring(frame->points.size()));

    return 0;
}

LRESULT CSmartRayViewerDlg::OnPcThicknessReady(WPARAM, LPARAM lParam)
{
    auto* pFramePtr = reinterpret_cast<std::shared_ptr<PcFrame>*>(lParam);
    std::shared_ptr<PcFrame> frame = (pFramePtr ? *pFramePtr : nullptr);
    delete pFramePtr;

    if (!frame || frame->points.empty())
        return 0;

    // thickness(pc) 표시 (현재는 같은 VTK에 표시)
    m_vtkView1.UpdatePointCloud(frame->points);

    if (m_roiAutoUpdate)
        ComputeRoiStats_AndFillGrid();

    LogManager::GetInstance().PushLog(
        Log::Main, L"OnPcThicknessReady",
        L"Thickness Update / frameNo=" + std::to_wstring(frame->frameNo) +
        L" count=" + std::to_wstring(frame->points.size()));

    return 0;
}

LRESULT CSmartRayViewerDlg::OnZMapReady(WPARAM, LPARAM lParam)
{
    auto* pZPtr = reinterpret_cast<std::shared_ptr<ZMapFrame>*>(lParam);
    std::shared_ptr<ZMapFrame> zf = (pZPtr ? *pZPtr : nullptr);
    delete pZPtr;

    if (!zf || zf->z.empty() || zf->w <= 0 || zf->h <= 0)
        return 0;

    // CZMapRenderer가 raw u16을 받는 API 필요
    if (!m_zmap.SetFromRawU16(zf->w, zf->h, zf->z.data()))
        return 0;

    uint16_t dataMin = 1, dataMax = 65535;
    uint16_t invalidValue = 0;
    if (!m_zmap.GetDataMinMax(dataMin, dataMax, invalidValue))
        return 0;
    InitZMapSliders(dataMin, dataMax);

    CaptureBaseVminVmax();

    //InitZMapSliders();

    const int w = m_zmap.Width();
    const int h = m_zmap.Height();
    const bool sizeChanged = (w != m_lastZImgW) || (h != m_lastZImgH);

    if (sizeChanged)
    {
        _image.Init(w, h, 24);
        m_lastZImgW = w;
        m_lastZImgH = h;
    }

    m_hasZmap = true;
    UpdateZMapJet();

    // ======================
    // Text formatting (English)
    // ======================

    double dYScale = AppStore::Get().GetParameterAsDouble("Sensor", "Y_Scale");
    int profilesCnt = AppStore::Get().GetParameterAsInt("Sensor", "S1_NumberOfProfiles");
    double dRange = dYScale * profilesCnt;

    std::wstringstream ss;
    ss << L"- Inspect Frame: " << zf->frameNo << L" ea" << L"\n";     // ✅ 프레임 번호
    ss << L"- ScanRange: " << dRange << L" mm" << L"\n";

    _vLabelViewInfo.SetText(ss.str());
    _vLabelViewInfo.Draw();

    return 0;
}

// ============================================================================
// ROI stats
// ============================================================================
void CSmartRayViewerDlg::ComputeRoiStats_AndFillGrid()
{
    ClearGridData();

    const int roiCnt = _image.GetCountTrackerROI();
    if (roiCnt <= 0) return;

    if (m_roiSource == RoiStatSource::ZMap16)
    {
        if (!m_hasZmap)
            return;

        const uint16_t invalidValue = 0;
        ComputeRoiStats_FromZMap(invalidValue);
        return;
    }

    int nType = AppStore::Get().GetParameterAsInt("System", "Thickness_View");
    int nDataSelete = 0;
    if (nType == 2) {
        nDataSelete = 2;
    }

    auto frame = m_result.GetLastPcFrame(nDataSelete);
    if (!frame || frame->points.empty())
        return;

    const bool rotateCW90 = true;
    ComputeRoiStats_FromPointCloud(*frame, rotateCW90);

    // ✅ (추가) 2센서 모드면 단차행 추가
    AppendStepRowsIfDualMode();
}

bool CSmartRayViewerDlg::ComputeRoiStats_FromZMap(uint16_t invalidValue)
{
    const int roiCnt = _image.GetCountTrackerROI();
    if (roiCnt <= 0) return false;

    bool any = false;
    for (int i = 0; i < roiCnt; ++i)
    {
        CRect roi = _image.GetTrackerROI(i);

        RoiInfoData st;
        if (m_zmap.GetStatsInRoi(roi, st, invalidValue))
        {
            AddGridMeasureZ(i, st);
            any = true;
        }
    }
    return any;
}

bool CSmartRayViewerDlg::ComputeRoiStats_FromPointCloud(const PcFrame& frame, bool rotateCW90)
{
    const int roiCnt = _image.GetCountTrackerROI();
    if (roiCnt <= 0) return false;

    const int W = m_zmap.Width();
    const int H = m_zmap.Height();
    if (W <= 0 || H <= 0) return false;

    const float X_Scale = (float)AppStore::Get().GetParameterAsDouble("Sensor", "X_Scale");
    const float Y_Scale = (float)AppStore::Get().GetParameterAsDouble("Sensor", "Y_Scale");
    if (X_Scale <= 0.f || Y_Scale <= 0.f) return false;

    // xmin/ymin 기준점 (min 값)
    float xmin = 0.f, ymin = 0.f;
    bool firstXY = true;
    for (const auto& p : frame.points)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) continue;
        if (firstXY) { xmin = p.x; ymin = p.y; firstXY = false; }
        else { if (p.x < xmin) xmin = p.x; if (p.y < ymin) ymin = p.y; }
    }
    if (firstXY) return false;

    bool any = false;

    for (int i = 0; i < roiCnt; ++i)
    {
        CRect roi = _image.GetTrackerROI(i);

        double sum = 0.0;
        double mn = 0.0, mx = 0.0;
        long long cnt = 0;
        bool first = true;

        for (const auto& p : frame.points)
        {
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) continue;

            int ix = (int)std::lround((p.x - xmin) / X_Scale);
            int iy = (int)std::lround((p.y - ymin) / Y_Scale);

            // y flip
            iy = (H - 1) - iy;

            // CW90
            if (rotateCW90)
            {
                int rx = (H - 1) - iy;
                int ry = ix;
                ix = rx;
                iy = ry;
            }

            if (ix < roi.left || ix >= roi.right || iy < roi.top || iy >= roi.bottom)
                continue;

            const double z = (double)p.z;

            if (first) { mn = mx = z; first = false; }
            else { if (z < mn) mn = z; if (z > mx) mx = z; }

            sum += z;
            ++cnt;
        }

        if (cnt == 0) continue;

        RoiInfoData st{};
        st.ok = true;
        st.mean = sum / (double)cnt;
        st.minv = mn;
        st.maxv = mx;
        st.count = cnt;

        AddGridMeasureZ(i, st);
        any = true;
    }

    return any;
}

void CSmartRayViewerDlg::AddGridMeasureZ(int RoiNo, const RoiInfoData& st)
{
    int nRow = _vGridResult.GetRowCount();
    _vGridResult.SetRowCount(nRow + 1);

    CString cols[5];
    cols[0].Format(L"%d", RoiNo);
    cols[1].Format(L"%.4f", st.mean);
    cols[2].Format(L"%.4f", st.maxv);
    cols[3].Format(L"%.4f", st.minv);
    cols[4].Format(L"%lld", (long long)st.count);

    for (int c = 0; c < 2; ++c)
    {
        _vGridResult.SetItemFormat(nRow, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        _vGridResult.SetItemText(nRow, c, cols[c]);
        _vGridResult.SetItemFgColour(nRow, c, RGB(0, 0, 0));
        _vGridResult.SetItemState(nRow, c, GVIS_READONLY);
    }

    _vGridResult.SetRowHeight(nRow, 50);
    _vGridResult.Invalidate();
}

// ============================================================================
// Sensor / Result pipeline
// ============================================================================
void CSmartRayViewerDlg::InitSensor()
{
    const SensorMode mode = GetSensorMode();

    // 기본
    m_result.SetZMapEnabled(true);

    if (mode == SensorMode::Thickness)
    {
        m_result.SetThicknessEnabled(true);
        m_result.SetPCEnabled(false); // 두께 실패 대비용으로 유지 권장
    }
    else
    {
        m_result.SetThicknessEnabled(false);
        m_result.SetPCEnabled(true);
    }

    LogManager::GetInstance().PushLog(
        Log::Main, L"InitSensor",
        L"Result config: " + ModeToText(mode));

    m_result.SetSaveRoots(L"D:\\Data", L"E:\\Data");
    m_result.SetSaveEnabled(false);
    m_result.SetThicknessSaveEnabled(false);
    m_result.SetZMapSaveEnabled(false);

    BindResultCallbacks();
}


bool CSmartRayViewerDlg::InitOneSensor(
    SmartRaySensor& sensor,
    int camIndex,
    const std::string& name,
    const std::string& ip,
    unsigned short port,
    const SensorParams& /*p*/) // <= 여기 p는 이제 Connect에서 필요없으면 지워도 됨
{
    sensor.Configure(name, ip, port, camIndex);

    // ✅ Base(필수)만
    ApplyBaseToSensor(sensor, camIndex);

    // ✅ callback → Result (이건 Connect 전/후 상관 없음)
    sensor.SetFrameCallback([this](std::shared_ptr<PcFrame> f) {
        m_result.OnPointCloud(std::move(f));
        });

    // ✅ Connect(heavy config 1회는 SmartRaySensor 내부에서 수행)
    if (!sensor.Connect(60))
        return false;

    m_result.RegisterAnySensorHandle(sensor.GetHandle());
    return true;
}


// SmartRayViewerDlg.cpp
bool CSmartRayViewerDlg::StartAcquisition()
{
    auto& log = LogManager::GetInstance();
    const SensorMode mode = GetSensorMode();

    // 1) 혹시 돌고 있으면 먼저 정지
    if (NeedSensor0(mode)) m_Sensor0.Stop();
    if (NeedSensor1(mode)) m_Sensor1.Stop();

    // 2) run state reset
    if (NeedSensor0(mode)) m_Sensor0.ResetRunState(true, true);
    if (NeedSensor1(mode)) m_Sensor1.ResetRunState(true, true);

    // 3) Result run state reset
    m_result.ResetRunState(true);

    // 4) Start
    bool ok0 = true, ok1 = true;
    if (NeedSensor0(mode)) ok0 = m_Sensor0.Start();
    if (NeedSensor1(mode)) ok1 = m_Sensor1.Start();

    // 5) 로그
    std::wstring msg = L"StartAcquisition: " + ModeToText(mode);
    if (NeedSensor0(mode)) msg += L", S0=" + std::wstring(ok0 ? L"OK" : L"FAIL");
    if (NeedSensor1(mode)) msg += L", S1=" + std::wstring(ok1 ? L"OK" : L"FAIL");
    log.PushLog(Log::Main, L"StartAcquisition", msg);

    // 6) 부분 성공이면 안전 정리
    const bool allOk = (!NeedSensor0(mode) || ok0) && (!NeedSensor1(mode) || ok1);
    if (!allOk)
    {
        if (NeedSensor0(mode)) m_Sensor0.Stop();
        if (NeedSensor1(mode)) m_Sensor1.Stop();
        return false;
    }

    return true;
}

void CSmartRayViewerDlg::StopAcquisition()
{
    auto& log = LogManager::GetInstance();
    const SensorMode mode = GetSensorMode();

    if (NeedSensor0(mode)) m_Sensor0.Stop();
    if (NeedSensor1(mode)) m_Sensor1.Stop();

    const bool stillRunning =
        (NeedSensor0(mode) && m_Sensor0.IsRunning()) ||
        (NeedSensor1(mode) && m_Sensor1.IsRunning());

    log.PushLog(Log::Main, L"StopAcquisition",
        std::wstring(L"StopAcquisition: ") + ModeToText(mode) +
        (stillRunning ? L" [FAIL] still running" : L" [OK] stopped"));
}

void CSmartRayViewerDlg::BindResultCallbacks()
{
    // Result thread → UI thread
    m_result.SetPointCloudCallback([this](std::shared_ptr<PcFrame> frame) {
        PostMessage(WM_PCFRAME_READY, 0, (LPARAM)new std::shared_ptr<PcFrame>(std::move(frame)));
        });

    m_result.SetZMapCallback([this](std::shared_ptr<ZMapFrame> zf) {
        PostMessage(WM_ZMAP_READY, 0, (LPARAM)new std::shared_ptr<ZMapFrame>(std::move(zf)));
        });

    m_result.SetThicknessCallback([this](std::shared_ptr<PcFrame> frame) {
        PostMessage(WM_PC_THICKNESS_READY, 0, (LPARAM)new std::shared_ptr<PcFrame>(std::move(frame)));
        });
}

// ============================================================================
// Grid / Image init helpers
// ============================================================================
void CSmartRayViewerDlg::ClearGridData()
{
    int nRowCount = _vGridResult.GetRowCount();

    // 0행은 헤더, 1행부터 삭제
    for (int i = nRowCount - 1; i >= 1; --i)
        _vGridResult.DeleteRow(i);

    _vGridResult.Refresh();
}

void CSmartRayViewerDlg::InitImage()
{
    _image.SetUseTracker(true);
    _image.SetResizeTracker(false);
    _image.SetUIControlAttached(true);
    _image.FitImageToScreen();
    _image.ClearObjAll();
}

void CSmartRayViewerDlg::InitLayout()
{
    UIHelper::InitLabel(_vLabelLogo, L"", eLabelAlignH::Center, eLabelAlignV::Center, 20, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"Logo.png", 0);
    UIHelper::InitLabel(_vLabelLogo2, L"", eLabelAlignH::Center, eLabelAlignV::Center, 20, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"Logo_SmartRay.png", 0);
    UIHelper::InitLabel(_vLabelTitle, L"3D Inspection with.", eLabelAlignH::Center, eLabelAlignV::Center, 56, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
    UIHelper::InitLabel(_vLabelTime, L"2026/01/21 17:00:00", eLabelAlignH::Center, eLabelAlignV::Center, 30, AppColor::RGB_WHITE, AppColor::RGB_BLACK, L"", 0);
    UIHelper::InitLabel(_vLabelPCInfo, L"PC Info", eLabelAlignH::Left, eLabelAlignV::Top, 26, AppColor::RGB_WHITE, AppColor::RGB_GRAY, L"", 0);
    UIHelper::InitLabel(_vLabelVersion, L"Program Ver. 1.1.0  Developer by Forvis.Inc", eLabelAlignH::Right, eLabelAlignV::Center, 18, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
    UIHelper::InitLabel(_vLabelViewInfo, L"View Info", eLabelAlignH::Left, eLabelAlignV::Top, 26, AppColor::RGB_WHITE, AppColor::RGB_GRAY, L"", 0);

    UIHelper::InitIconButton(_vBtnMinimize, L"", L"Pgm_Minimize.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_vBtnExit, L"", L"Pgm_Off.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);

    UIHelper::InitLabel(_labelConnectSensor1, L"Sensor #1", eLabelAlignH::Left, eLabelAlignV::Center, 24, AppColor::RGB_WHITE, AppColor::RGB_GRAY, L"", 0);
    UIHelper::InitLabel(_labelConnectSensor2, L"Sensor #2", eLabelAlignH::Left, eLabelAlignV::Center, 24, AppColor::RGB_WHITE, AppColor::RGB_GRAY, L"", 0);

    UIHelper::InitLabel(_vLabelZmapTitle, L"Zmap ColorMap Viewer & ROI Setting", eLabelAlignH::Center, eLabelAlignV::Center, 24, AppColor::RGB_WHITE, AppColor::RGB_BLACK, L"", 0);
    UIHelper::InitLabel(_vLabelvMin, L"vMin : 0", eLabelAlignH::Left, eLabelAlignV::Center, 16, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
    UIHelper::InitLabel(_vLabelvMax, L"vMax : 65535", eLabelAlignH::Left, eLabelAlignV::Center, 16, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
    UIHelper::InitIconButton(_btnResult, L"ROI Operation", L"", 24, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnLoadImg, L"데이터 불러오기", L"LoadFile.png", 20, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);

    UIHelper::InitLabel(_vLabel3DTile, L"Point Cloude 3D Viewer", eLabelAlignH::Center, eLabelAlignV::Center, 24, AppColor::RGB_WHITE, AppColor::RGB_BLACK, L"", 0);

    UIHelper::InitIconButton(_btnAutoRotate, L"", L"Auto_Rotate.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnTopView, L"", L"TopView.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnFrontView, L"", L"FrontView.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnSideLeftView, L"", L"LeftView.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnLoad3DData, L"", L"LoadFile.png", 20, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnSetting, L"", L"Setting.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnOpenFolder, L"", L"Folder_Open.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);

    UIHelper::InitIconButton(_btnConnect, L"", L"DisConnect.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnStart, L"", L"Start.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnFwdMove, L"CW", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnBwdMove, L"CCW", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);

    UIHelper::InitIconButton(_btnSaveFile, L"", L"Save.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
    UIHelper::InitIconButton(_btnLoadMat, L"", L"Load_Mat.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);

    UIHelper::InitIconButton(_btnAutoRange, L"Start Auto Change Aim", L"", 28, true,AppColor::RGB_WHITE, AppColor::RGB_BLUE, AppColor::BUTTON_DOWN_RGB);

    _vLabelLine.SetTransparent(false);
    _vLabelLine.SetBackground(AppColor::RGB_WHITE);
    _vLabelLine.SetBgMargin(0, 6, 0, 6);

    InitToolTips();
}

void CSmartRayViewerDlg::InitClass()
{
    // 로그 다이얼로그
    CWnd* pArea = GetDlgItem(IDC_LOG_AREA);
    if (pArea)
    {
        CRect rc;
        pArea->GetWindowRect(&rc);
        ScreenToClient(&rc);

        // 2) DlgLog를 메인 다이얼로그의 Child로 생성
        //    (중요: parent를 this로)
        _dlgLog.Create(IDD_DLG_LOG, this);
        LogManager::GetInstance().SetLogDialog(&_dlgLog);  // LogManager에서 DlgLog 접근 가능

        // 3) 자리 위치에 딱 맞게 배치 + 표시
        _dlgLog.MoveWindow(rc);
        _dlgLog.ShowWindow(SW_SHOW);

        // (선택) 자리 컨트롤은 숨김 처리
        pArea->ShowWindow(SW_HIDE);
    }

    // 파라미터 다이얼로그
    if (!_dlgParam) {
        _dlgParam = new DlgParam();
        _dlgParam->Create(IDD_DLG_PARAM, this);
    }

    // I/O 연결
    DioDeviceManager& dio = DioDeviceManager::Instance();
    if (!dio.Init("DIO000")) {
        LogManager::GetInstance().PushLog(Log::Main, L"InitClass", L"Init I/O Fail");
        AfxMessageBox(L"I/O연결에 실패하였습니다.");
    }
}


void CSmartRayViewerDlg::InitGrid()
{
    CFont font;
    font.CreatePointFont(100, L"SUIT SemiBold ");
    _vGridResult.SetFont(&font); // GridCtrl 전체에 적용

    _vGridResult.SetFixedRowCount(1);
    _vGridResult.SetRowCount(1);		//헤더만 생성

    _vGridResult.SetColumnCount(2);		//항목 5개


    //가로 폭 조절
    CRect rt;
    ((CWnd*)(GetDlgItem(IDC_CUSTOM_RESULT_GRID)))->GetWindowRect(&rt);
    rt.SetRect(rt.left, rt.top, rt.right, rt.bottom);
    int totalWidth = rt.Width() - 20;
    int firstCellWidth = 80;
    int nCellWidth = totalWidth - firstCellWidth; //(totalWidth / _vGridResult.GetColumnCount()) - firstCellWidth;

    for (int i = 0; i < _vGridResult.GetColumnCount(); i++)
        if(i == 0)
            _vGridResult.SetColumnWidth(i, firstCellWidth);
        else
            _vGridResult.SetColumnWidth(i, nCellWidth);


    // 헤더
    const CStringW headers[5] = { L"No.", L"Thickness" };

    for (int i = 0; i < _vGridResult.GetColumnCount(); i++) {
        GV_ITEM it{};
        it.mask = GVIF_TEXT | GVIF_FORMAT;
        it.row = 0; it.col = i;
        it.nFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        it.strText = headers[i];
        _vGridResult.SetItem(&it);
        _vGridResult.SetItemState(0, i, _vGridResult.GetItemState(0, i) | GVIS_READONLY);
        _vGridResult.SetItemBkColour(0, i, AppColor::RGB_SUB_LABEL_BK_COLOR);
    }

    for (int i = 0; i < _vGridResult.GetRowCount(); ++i)
        _vGridResult.SetRowHeight(i, 30); // 25픽셀

    _vGridResult.SetHeaderSort(FALSE);
    _vGridResult.SetEditable(TRUE);				//FALSE : 내용 수정 못함, TRUE : 내용 수정 가능
    _vGridResult.SetColumnResize(FALSE);		//FALSE : 사용자 변경 불가, TRUE : 사용자 변경 가능
    _vGridResult.SetRowResize(FALSE);			//FALSE : 사용자 변경 불가, TRUE : 사용자 변경 가능

    _vGridResult.SetFixedBkColor(RGB(240, 240, 240));
    _vGridResult.SetBkColor(RGB(255, 255, 255));
    _vGridResult.SetTextBkColor(RGB(255, 255, 255));

    _vGridResult.SetListMode(TRUE);

}

// ============================================================================
// ZMap slider / render
// ============================================================================
//void CSmartRayViewerDlg::InitZMapSliders(uint16_t mn, uint16_t mx)
//{
//    m_vmin = mn;
//    m_vmax = mx;
//
//    m_vmin = 0;
//    m_vmax = 65536;
//
//    m_sliderVmin.SetRange((int)mn, (int)mx, TRUE);
//    m_sliderVmax.SetRange((int)mn, (int)mx, TRUE);
//
//    m_sliderVmin.SetPos((int)m_vmin);
//    m_sliderVmax.SetPos((int)m_vmax);
//
//    UpdateZMapValueLabels();
//}

void CSmartRayViewerDlg::InitZMapSliders(uint16_t mn, uint16_t mx)
{
    constexpr int SL_MIN = 0;
    constexpr int SL_MAX = 65535;

    // ✅ Range는 무조건 고정
    m_sliderVmin.SetRange(SL_MIN, SL_MAX, TRUE);
    m_sliderVmax.SetRange(SL_MIN, SL_MAX, TRUE);

    // ✅ 최초 기본값도 전체 범위로
    m_vmin = mn;
    m_vmax = mx;

    m_sliderVmin.SetPos((int)m_vmin);
    m_sliderVmax.SetPos((int)m_vmax);

    UpdateZMapValueLabels();
}

void CSmartRayViewerDlg::UpdateZMapValueLabels()
{
    CString s;

    s.Format(L"Vmin : %u", m_vmin);
    _vLabelvMin.SetText(std::wstring(s.GetString()));

    s.Format(L"Vmax : %u", m_vmax);
    _vLabelvMax.SetText(std::wstring(s.GetString()));

    _vLabelvMin.Draw();
    _vLabelvMax.Draw();
}

void CSmartRayViewerDlg::UpdateZMapJet()
{
    LogManager& logMgr = LogManager::GetInstance();

    if (!m_hasZmap) return;

    if (m_vmin >= m_vmax)
    {
        m_vmax = (uint16_t)(m_vmin + 1);
        m_sliderVmax.SetPos((int)m_vmax);
    }

    if (!m_zmap.RenderJetTo(_image, m_vmin, m_vmax, 0, cv::Scalar(80, 80, 80)))
    {
        OutputDebugString(L"[ZMAP] RenderJetTo failed\n");
        logMgr.PushLog(Log::Main, L"UpdateZMapJet", L"[ZMAP] RenderJetTo failed");
        return;
    }
    //_image.SaveImg(L"D:\\Image\\TestImg.bmp");
    _image.ClearObjAll();
    _image.DrawImage(eImageZoomType::NotResizeDraw);
    _image.DrawObjAll();
}

// ============================================================================
// Color bar (OnPaint overlay)
// ============================================================================
void CSmartRayViewerDlg::DrawZMapColorBar(CDC* pDC)
{
    // ZMap view 기준으로 위치 잡기
    CRect rc;
    GetDlgItem(IDC_IMAGE_VIEW)->GetWindowRect(&rc);
    ScreenToClient(&rc);

    int barHeight = 25;
    CRect barRect(rc.left, rc.bottom + 10, rc.right, rc.bottom + 10 + barHeight);

    // 화면 밖이면 skip
    CRect client;
    GetClientRect(&client);
    if (barRect.bottom > client.bottom)
        return;

    int width = barRect.Width();
    if (width <= 1) return;

    for (int x = 0; x < width; ++x)
    {
        double t = (double)x / (width - 1);
        COLORREF color = GetJetColor(t);

        pDC->FillSolidRect(barRect.left + x, barRect.top, 1, barRect.Height(), color);
    }

    pDC->SetBkMode(TRANSPARENT);
    pDC->SetTextColor(RGB(255, 255, 255));
}

COLORREF CSmartRayViewerDlg::GetJetColor(double t)
{
    t = max(0.0, min(1.0, t));

    double r = 0.0, g = 0.0, b = 0.0;

    if (t < 0.5)
    {
        double u = t / 0.5;
        r = 0.0; g = u; b = 1.0 - u;
    }
    else
    {
        double u = (t - 0.5) / 0.5;
        r = u; g = 1.0 - u; b = 0.0;
    }

    return RGB((int)(r * 255), (int)(g * 255), (int)(b * 255));
}

// ============================================================================
// UI helpers (monitoring signal)
// ============================================================================
void CSmartRayViewerDlg::DrawMonitoringSignalOnOff(int nCtrlID, COLORREF color)
{
    CDC* pDC = GetDC();

    CRect rt, rt1;
    GetWindowRect(&rt1);
    ((CWnd*)(GetDlgItem(nCtrlID)))->GetWindowRect(&rt);
    rt.OffsetRect(-rt1.left, -rt1.top);
    rt1.SetRect(rt.left, rt.top, rt.right, rt.bottom);

    CPen pen, pen1;
    CBrush brush, * oldBrush, brush1;

    brush1.CreateSolidBrush(AppColor::RGB_GRAY);
    brush.CreateSolidBrush(color);

    pDC->SelectObject(&brush1);

    pen.CreatePen(PS_SOLID, 1, color);
    pen1.CreatePen(PS_SOLID, 1, AppColor::RGB_GRAY);
    CPen* oldPen = pDC->SelectObject(&pen1);

    pDC->Rectangle(&rt1);

    pDC->SelectObject(&pen);
    oldBrush = pDC->SelectObject(&brush);

    int nSize = 10;
    rt1.DeflateRect((int)(rt1.Width() / nSize), (int)(rt1.Height() / nSize));
    pDC->Ellipse(&rt1);

    pDC->SelectObject(oldPen);
    pDC->SelectObject(oldBrush);

    DeleteObject(pen);
    DeleteObject(pen1);
    DeleteObject(brush);
    DeleteObject(brush1);

    ReleaseDC(pDC);
}

BOOL CSmartRayViewerDlg::PreTranslateMessage(MSG* pMsg)
{
    if (m_toolTip.GetSafeHwnd())
        m_toolTip.RelayEvent(pMsg);

    return CDialogEx::PreTranslateMessage(pMsg);
}

void CSmartRayViewerDlg::InitToolTips()
{
    // 이미 생성되어 있으면 스킵
    if (m_toolTip.GetSafeHwnd())
        return;

    m_toolTip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
    m_toolTip.Activate(TRUE);

    m_toolTipFont.CreatePointFont(140, L"SUIT SemiBold");
    m_toolTip.SetFont(&m_toolTipFont);

    // (선택) 딜레이 조절
    m_toolTip.SetDelayTime(TTDT_INITIAL, 200);   // 0.2초 후 표시
    m_toolTip.SetDelayTime(TTDT_AUTOPOP, 4000);  // 4초 유지
    m_toolTip.SetDelayTime(TTDT_RESHOW, 100);

    // ✅ 핵심: “컨트롤 HWND”에 붙인다
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_SENSOR_CONNECT), L"센서 연결/해제");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_START), L"측정 시작/정지");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_DATA_SAVE), L"데이터 저장 ON/OFF");

    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_LOAD_IMG), L"ZMap 이미지 불러오기");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_LOAD_MAT), L"매트릭스 파일 불러오기");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_LOAD_3D_DATA), L"포인트클라우드 파일 불러오기");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_OPEN_FOLDER), L"오늘 날짜 폴더 열기");

    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_SETTING), L"파라미터 창 열기/닫기");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_AUTO_ROTATE), L"3D View Auto Rotate");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_TOP_VIEW), L"Top View");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_FRONT_VIEW), L"Front View");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_SIDE_LEFT_VIEW), L"Side View");

    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_JOGFWD), L"모터 정방향 이동");
    m_toolTip.AddTool(GetDlgItem(IDC_BUTTON_JOGBWD), L"모터 역방향 이동");
}

void CSmartRayViewerDlg::OnBnClickedButtonJogfwd()
{

    if (m_jogBwdOn) {
        AfxMessageBox(L"역방향 이동중입니다.\n역방향 중지후 다시 시도해주세요.");
        return;
    }

    if (!m_jogFwdOn)
    {
        // 인터락
        DioDeviceManager::Instance().OutBit(DioOutputBit::MoveBwd, false);
        DioDeviceManager::Instance().OutBit(DioOutputBit::MoveFwd, true);
        Sleep(500);
        DioDeviceManager::Instance().OutBit(DioOutputBit::MovePowerOff, true);

        m_jogFwdOn = true;
        m_jogBwdOn = false;

        UIHelper::InitIconButton(_btnFwdMove, L"CW", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_RED, AppColor::BUTTON_DOWN_RGB);
        LogManager::GetInstance().PushLog(Log::Main, L"JOG", L"FWD ON / BWD OFF");
    }
    else
    {
        Jog_AllOff();
        UIHelper::InitIconButton(_btnFwdMove, L"CW", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
        LogManager::GetInstance().PushLog(Log::Main, L"JOG", L"FWD OFF / BWD OFF");
    }
}

void CSmartRayViewerDlg::OnBnClickedButtonJogbwd()
{

    if (m_jogFwdOn) {
        AfxMessageBox(L"정방향 이동중입니다.\n역방향 중지후 다시 시도해주세요.");
        return;
    }

    if (!m_jogBwdOn)
    {
        DioDeviceManager::Instance().OutBit(DioOutputBit::MoveFwd, false);
        DioDeviceManager::Instance().OutBit(DioOutputBit::MoveBwd, true);
        Sleep(500);
        DioDeviceManager::Instance().OutBit(DioOutputBit::MovePowerOff, true);

        m_jogBwdOn = true;
        m_jogFwdOn = false;

        UIHelper::InitIconButton(_btnBwdMove, L"CCW", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_RED, AppColor::BUTTON_DOWN_RGB);
        LogManager::GetInstance().PushLog(Log::Main, L"JOG", L"FWD OFF / BWD ON");
    }
    else
    {
        Jog_AllOff();

        UIHelper::InitIconButton(_btnBwdMove, L"CCW", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
        LogManager::GetInstance().PushLog(Log::Main, L"JOG", L"FWD OFF / BWD OFF");
    }
}


void CSmartRayViewerDlg::Jog_AllOff()
{
    DioDeviceManager::Instance().OutBit(DioOutputBit::MoveFwd, false);
    DioDeviceManager::Instance().OutBit(DioOutputBit::MoveBwd, false);
    Sleep(500);
    DioDeviceManager::Instance().OutBit(DioOutputBit::MovePowerOff, false);

    m_jogFwdOn = false;
    m_jogBwdOn = false;
}

void CSmartRayViewerDlg::OnBnClickedButtonAutoRotate()
{
    if (!m_vtkView1.IsAutoRotating() && !m_vtkView2.IsAutoRotating())
    {
        m_vtkView1.StartAutoRotate(40.0); // 초당 40도
        m_vtkView2.StartAutoRotate(40.0); // 초당 40도

        m_lastTick = ::GetTickCount64();
        m_timerRotate = SetTimer(TimerID::AutoRotate, 16, nullptr); // 60fps 느낌(대충)

        UIHelper::InitIconButton(_btnAutoRotate, L"", L"Save_stop.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);

    }
    else
    {
        m_vtkView1.StopAutoRotate();
        m_vtkView2.StopAutoRotate();
        if (m_timerRotate) { KillTimer(TimerID::AutoRotate); m_timerRotate = 0; }

        UIHelper::InitIconButton(_btnAutoRotate, L"", L"Auto_Rotate.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
    }

}

void CSmartRayViewerDlg::OnBnClickedButtonAutoRange()
{
    if (!m_zWinAnimOn) {
        StartZMapWindowAnim();
    }
    else {
        StopZMapWindowAnim();
    }


    //if (!m_hasZmap)
    //{
    //    AfxMessageBox(L"ZMap 데이터가 없습니다.\n먼저 ZMap을 불러오거나 측정을 진행하세요.");
    //    return;
    //}

    //// invalidValue는 프로젝트 기준에 맞게 (현재 RenderJetTo에서도 0을 invalid로 쓰고 있음)
    //const uint16_t invalidValue = 0;

    //uint16_t dataMin = 0, dataMax = 0, inv = invalidValue;

    //// ✅ ZMap 전체에서 유효값 기준 min/max 계산
    //if (!m_zmap.GetDataMinMax(dataMin, dataMax, inv))
    //{
    //    AfxMessageBox(L"자동 범위 계산 실패");
    //    return;
    //}

    //// 유효값이 없거나 min/max가 이상하면 방어
    //if (dataMax <= dataMin)
    //{
    //    AfxMessageBox(L"유효한 Z 값이 부족해서 자동 범위를 계산할 수 없습니다.");
    //    return;
    //}

    //// ✅ 슬라이더 Range는 고정(0~65535) 그대로 두고, 위치만 자동 세팅
    //m_vmin = dataMin;
    //m_vmax = dataMax;

    //// 역전 방지
    //if (m_vmin >= m_vmax)
    //    m_vmax = (uint16_t)(m_vmin + 1);

    //m_sliderVmin.SetPos((int)m_vmin);
    //m_sliderVmax.SetPos((int)m_vmax);

    //UpdateZMapValueLabels();
    //UpdateZMapJet();

    //LogManager::GetInstance().PushLog(Log::Main, L"AutoRange",
    //    L"Auto range set: vmin=" + std::to_wstring(m_vmin) + L", vmax=" + std::to_wstring(m_vmax));
}

void CSmartRayViewerDlg::CaptureVtkHostRectsOnce()
{
    if (m_vtkHostRcCaptured) return;

    CWnd* host1 = GetDlgItem(IDC_VTK_VIEW);
    CWnd* host2 = GetDlgItem(IDC_VTK_VIEW2);
    if (!host1 || !host2) return;

    host1->GetWindowRect(&m_rcVtkHost1Orig);
    host2->GetWindowRect(&m_rcVtkHost2Orig);
    ScreenToClient(&m_rcVtkHost1Orig);
    ScreenToClient(&m_rcVtkHost2Orig);

    m_vtkHostRcCaptured = true;
}

void CSmartRayViewerDlg::ApplyVtkLayoutByMode(SensorMode mode)
{
    CWnd* host1 = GetDlgItem(IDC_VTK_VIEW);
    CWnd* host2 = GetDlgItem(IDC_VTK_VIEW2);
    if (!host1 || !host2) return;

    CaptureVtkHostRectsOnce();
    if (!m_vtkHostRcCaptured) return;

    if (mode == SensorMode::Dual)
    {
        // Dual: 원래대로 2개 보이기
        host1->ShowWindow(SW_SHOW);
        host2->ShowWindow(SW_SHOW);

        host1->MoveWindow(m_rcVtkHost1Orig);
        host2->MoveWindow(m_rcVtkHost2Orig);

        m_vtkView1.ResizeToHost();
        m_vtkView2.ResizeToHost();

        _vLabelLine.ShowWindow(SW_SHOW);
    }
    else
    {
        // Single/Thickness: 1개만 크게(두 영역 합치기)
        CRect rcUnion;
        rcUnion.UnionRect(m_rcVtkHost1Orig, m_rcVtkHost2Orig);

        host2->ShowWindow(SW_HIDE);
        host1->ShowWindow(SW_SHOW);
        host1->MoveWindow(rcUnion);

        m_vtkView1.ResizeToHost();

        _vLabelLine.ShowWindow(SW_HIDE);

        // (선택) 숨긴 뷰는 오토로테이트 끄기 등
        // m_vtkView2.StopAutoRotate();
    }
}

void CSmartRayViewerDlg::CaptureBaseVminVmax()
{
    m_baseVmin = m_vmin;
    m_baseVmax = m_vmax;

    if (m_baseVmax <= m_baseVmin)
        m_baseVmax = (uint16_t)min(65535, (int)m_baseVmin + 1);

    m_zWinShrinking = true;
}

void CSmartRayViewerDlg::TickZMapWindowAnim()
{
    if (!m_hasZmap) return;

    const int baseMin = (int)m_baseVmin;
    const int baseMax = (int)m_baseVmax;

    // 현재 표시값
    int vmin = (int)m_vmin;
    int vmax = (int)m_vmax;

    // 목표: base±range (가장 넓은 창)
    const int wideMin = baseMin - m_zWinRange;
    const int wideMax = baseMax + m_zWinRange;

    if (m_zWinShrinking)
    {
        // 넓은창 -> base로 좁히기
        vmin += m_zWinStep;
        vmax -= m_zWinStep;

        // base에 도착/초과하면 정확히 base로 고정하고 방향 전환
        if (vmin >= baseMin || vmax <= baseMax)
        {
            vmin = baseMin;
            vmax = baseMax;
            m_zWinShrinking = false; // 이제 다시 넓히기
        }
    }
    else
    {
        // base -> 넓은창으로 넓히기
        vmin -= m_zWinStep;
        vmax += m_zWinStep;

        // wide에 도착/초과하면 정확히 wide로 고정하고 방향 전환
        if (vmin <= wideMin || vmax >= wideMax)
        {
            vmin = wideMin;
            vmax = wideMax;
            m_zWinShrinking = true; // 다시 좁히기
        }
    }

    // 0~65535 clamp
    if (vmin < 0) vmin = 0;
    if (vmax > 65535) vmax = 65535;

    // 역전 방지
    if (vmax <= vmin) vmax = min(65535, vmin + 1);

    m_vmin = (uint16_t)vmin;
    m_vmax = (uint16_t)vmax;

    m_sliderVmin.SetPos((int)m_vmin);
    m_sliderVmax.SetPos((int)m_vmax);

    UpdateZMapValueLabels();
    UpdateZMapJet();
}

void CSmartRayViewerDlg::StartZMapWindowAnim()
{
    //if (!m_hasZmap) return;

    CaptureBaseVminVmax();

    int vmin = (int)m_baseVmin - m_zWinRange;
    int vmax = (int)m_baseVmax + m_zWinRange;

    if (vmin < 0) vmin = 0;
    if (vmax > 65535) vmax = 65535;
    if (vmax <= vmin) vmax = min(65535, vmin + 1);

    m_vmin = (uint16_t)vmin;
    m_vmax = (uint16_t)vmax;

    m_sliderVmin.SetPos((int)m_vmin);
    m_sliderVmax.SetPos((int)m_vmax);
    UpdateZMapValueLabels();
    UpdateZMapJet();

    m_zWinShrinking = true;   // 넓은창 -> base로 좁히기부터
    m_zWinAnimOn = true;

    UIHelper::InitIconButton(_btnAutoRange, L"Stop Auto Change Aim", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_RED, AppColor::BUTTON_DOWN_RGB);

}

void CSmartRayViewerDlg::StopZMapWindowAnim()
{
    // 이미 정지 상태면 무시
    if (!m_zWinAnimOn)
        return;

    m_zWinAnimOn = false;

    // ✅ 초기(base) 범위로 복귀
    m_vmin = m_baseVmin;
    m_vmax = m_baseVmax;

    // 안전 보정
    if (m_vmax <= m_vmin)
        m_vmax = (uint16_t)min(65535, (int)m_vmin + 1);

    // UI 반영
    m_sliderVmin.SetPos((int)m_vmin);
    m_sliderVmax.SetPos((int)m_vmax);

    UpdateZMapValueLabels();
    UpdateZMapJet();

    UIHelper::InitIconButton(_btnAutoRange, L"Start Auto Change Aim", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_BLUE, AppColor::BUTTON_DOWN_RGB);

}

// ✅ (추가) 그리드 문자열 -> double 파싱
static bool TryParseDoubleFromGrid(const CStringW& s, double& out)
{
    // 앞뒤 공백 제거
    CStringW t = s;
    t.Trim();
    if (t.IsEmpty()) return false;

    // _wtof는 실패해도 0을 줄 수 있으니, 간단히 숫자 여부 체크를 약하게라도 해줌
    // (여기선 실무 편의상 그냥 변환하고, NaN/Inf 방어만)
    out = _wtof(t);
    if (!_finite(out)) return false;
    return true;
}

// ✅ (추가) Thickness_View==2일 때, (1-2), (3-4) ... 단차행을 "맨 아래"에 추가
void CSmartRayViewerDlg::AppendStepRowsIfDualMode()
{
    const int nType = AppStore::Get().GetParameterAsInt("System", "Diff_View");
    if (nType != 1) return;

    // 현재 데이터 행 개수 (0행은 헤더)
    const int dataRows = _vGridResult.GetRowCount() - 1;
    if (dataRows < 2) return;

    // “기존 ROI 결과 행”만 기준으로 페어 계산해야 하므로 snapshot
    const int baseDataRows = dataRows;

    // (1,2), (3,4) ... 짝
    for (int r1 = 1; r1 <= baseDataRows; r1 += 2)
    {
        int r2 = r1 + 1;
        if (r2 > baseDataRows) break; // 마지막 홀수 남으면 스킵

        CStringW s1 = _vGridResult.GetItemText(r1, 1); // Thickness 컬럼
        CStringW s2 = _vGridResult.GetItemText(r2, 1);

        double v1 = 0.0, v2 = 0.0;
        if (!TryParseDoubleFromGrid(s1, v1)) continue;
        if (!TryParseDoubleFromGrid(s2, v2)) continue;

        // ✅ 단차 정의: (1행 - 2행). 원하면 반대로 바꾸면 됨.
        const double diff = (v1 - v2);

        // 새 행 추가(맨 아래)
        int newRow = _vGridResult.GetRowCount();
        _vGridResult.SetRowCount(newRow + 1);

        CStringW c0, c1;
        c0.Format(L"Δ(%d-%d)", r1 - 1, r2 - 1);    // No. 컬럼에 라벨
        c1.Format(L"%.4f", diff);          // Thickness 컬럼에 단차

        // 스타일/읽기전용
        for (int c = 0; c < 2; ++c)
        {
            _vGridResult.SetItemFormat(newRow, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            _vGridResult.SetItemState(newRow, c, GVIS_READONLY);

            // 보기 좋게 배경색 살짝 다르게 (원치 않으면 제거)
            _vGridResult.SetItemBkColour(newRow, c, RGB(245, 245, 255));
            _vGridResult.SetItemFgColour(newRow, c, RGB(0, 0, 0));
        }

        _vGridResult.SetItemText(newRow, 0, c0);
        _vGridResult.SetItemText(newRow, 1, c1);

        _vGridResult.SetRowHeight(newRow, 50);
    }

    _vGridResult.Invalidate();
}