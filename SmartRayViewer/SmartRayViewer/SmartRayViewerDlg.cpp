
// SmartRayViewerDlg.cpp: 구현 파일
//

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


static inline bool MapPointToPixel(
	float x, float y,
	int W, int H,
	bool rotateCW90,
	int& outX, int& outY,
	float xmin, float xmax, float ymin, float ymax,
	bool looksLikePixel
)
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

	// ZMap은 좌상단 원점이므로 y flip (필요하면)
	// 일단 “이미지 기준”으로 맞추는 게 목적이니 flip 해주는 쪽이 보통 맞음
	iy = (H - 1) - iy;

	// TopView 기준 CW90 회전 적용
	if (rotateCW90)
	{
		// (x,y) -> (H-1-y, x) : CW90 in pixel space
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



// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.
static CString GetLowerExtLocal(const CString& path)
{
	int dot = path.ReverseFind(L'.');
	CString ext = (dot >= 0) ? path.Mid(dot) : L"";
	ext.MakeLower();
	return ext;
}

static bool ParseSensorAndFrameNoFromFilename(
	const std::wstring& filename,
	int& outCam,
	uint64_t& outFrameNo)
{
	outCam = 0;
	outFrameNo = 0;

	// 예: 20260212_221711738_Sensor1_00000002_Pointcloud.asc
	// Sensor1 / 00000002 파싱
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

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CSmartRayViewerDlg 대화 상자

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

	DDX_Control(pDX, IDC_LABEL_LOGO, _vLabelLogo);
	DDX_Control(pDX, IDC_LABEL_NAME, _vLabelTitle);
	DDX_Control(pDX, IDC_LABEL_TIME, _vLabelTime);
	DDX_Control(pDX, IDC_LABEL_VERSION, _vLabelVersion);

	DDX_Control(pDX, IDC_LABEL_3D_VIEW_TITLE, _vLabel3DTile);
	DDX_Control(pDX, IDC_LABEL_ZMAP_VIEW_TITLE, _vLabelZmapTitle);

	DDX_Control(pDX, IDC_BTN_MINIMIZE, _vBtnMinimize);
	DDX_Control(pDX, IDC_BTN_EXIT, _vBtnExit);

	DDX_Control(pDX, IDC_BUTTON_LOAD_IMG, _btnLoadImg);
	DDX_Control(pDX, IDC_BUTTON_RESULT, _btnResult);
	DDX_Control(pDX, IDC_BUTTON_LOAD_3D_DATA, _btnLoad3DData);
	
	DDX_Control(pDX, IDC_LABEL_SENSOR_CONNECT_1, _labelConnectSensor1);
	DDX_Control(pDX, IDC_LABEL_SENSOR_CONNECT_2, _labelConnectSensor2);
	
	DDX_Control(pDX, IDC_SLIDER_VMIN, m_sliderVmin);
	DDX_Control(pDX, IDC_SLIDER_VMAX, m_sliderVmax);
	DDX_Control(pDX, IDC_LABEL_VMIN, _vLabelvMin);
	DDX_Control(pDX, IDC_LABEL_VMAX, _vLabelvMax);

	DDX_Control(pDX, IDC_IMAGE_VIEW, _image);
	DDX_Control(pDX, IDC_CUSTOM_RESULT_GRID, _vGridResult);

	DDX_Control(pDX, IDC_BUTTON_START, _btnStart);
	DDX_Control(pDX, IDC_BUTTON_STOP, _btnStop);
	DDX_Control(pDX, IDC_BUTTON_SETTING, _btnSetting);

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
	ON_BN_CLICKED(IDC_BUTTON_LOAD_IMG, &CSmartRayViewerDlg::OnBnClickedButtonLoadImg)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_BTN_MINIMIZE, &CSmartRayViewerDlg::OnBnClickedBtnMinimize)
	ON_BN_CLICKED(IDC_BTN_EXIT, &CSmartRayViewerDlg::OnBnClickedBtnExit)
	ON_BN_CLICKED(IDC_BUTTON_RESULT, &CSmartRayViewerDlg::OnBnClickedButtonResult)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BUTTON_LOAD_3D_DATA, &CSmartRayViewerDlg::OnBnClickedButtonLoad3dData)
	ON_BN_CLICKED(IDC_BUTTON_START, &CSmartRayViewerDlg::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CSmartRayViewerDlg::OnBnClickedButtonStop)
	ON_BN_CLICKED(IDC_BUTTON_SETTING, &CSmartRayViewerDlg::OnBnClickedButtonSetting)
	ON_BN_CLICKED(IDC_BUTTON_TOP_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonTopView)
	ON_BN_CLICKED(IDC_BUTTON_FRONT_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonFrontView)
	ON_BN_CLICKED(IDC_BUTTON_SIDE_LEFT_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonSideLeftView)

	ON_MESSAGE(WM_PCFRAME_READY, &CSmartRayViewerDlg::OnPcFrameReady)
	ON_MESSAGE(WM_ZMAP_READY, &CSmartRayViewerDlg::OnZMapReady)

	ON_BN_CLICKED(IDC_BUTTON_OPEN_FOLDER, &CSmartRayViewerDlg::OnBnClickedButtonOpenFolder)
END_MESSAGE_MAP()


// CSmartRayViewerDlg 메시지 처리기

BOOL CSmartRayViewerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
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

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	InitClass();
	InitLayout();
	InitGrid();
	InitImage();
	SetTimers();

	InitSensor();

	m_vtkView.Init(GetDlgItem(IDC_VTK_VIEW)->GetSafeHwnd());
	m_vtkView.ResizeToHost();

	LogManager::GetInstance().PushLog(Log::Main, L"OnInitDialog", L"PGM START");

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

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

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CSmartRayViewerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();

		// ✅ 여기서부터는 "덧그리기"니까 CClientDC 사용
		CClientDC dc(this);
		DrawZMapColorBar(&dc);
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
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

	//return CDialogEx::OnEraseBkgnd(pDC);
}

void CSmartRayViewerDlg::OnOK()
{
	//CDialogEx::OnOK();
}

void CSmartRayViewerDlg::OnCancel()
{
	//CDialogEx::OnCancel();
}

void CSmartRayViewerDlg::OnDestroy()
{
	KillTimers();

	m_Sensor0.Disconnect();
	m_Sensor1.Disconnect();

	CDialogEx::OnDestroy();
}


void CSmartRayViewerDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TimerID::UpdateTime) {
		std::wstring wStrTime = _Util.MakeTimestamp(vUtil::TimeUnit::Second, false, false);

		_vLabelTime.SetText(wStrTime);
		_vLabelTime.Draw();
	}

	if (nIDEvent == TimerID::UpdateConnect) {
		if (m_Sensor0.IsConnected()) {
			DrawMonitoringSignalOnOff(IDC_SIGNAL_SENSOR_CONNECT_1, AppColor::RGB_GREEN);
		}
		else {
			DrawMonitoringSignalOnOff(IDC_SIGNAL_SENSOR_CONNECT_1, AppColor::RGB_RED);
		}

		if (m_Sensor1.IsConnected()) {
			DrawMonitoringSignalOnOff(IDC_SIGNAL_SENSOR_CONNECT_2, AppColor::RGB_GREEN);
		}
		else {
			DrawMonitoringSignalOnOff(IDC_SIGNAL_SENSOR_CONNECT_2, AppColor::RGB_RED);
		}
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CSmartRayViewerDlg::SetTimers()
{
	SetTimer(TimerID::UpdateTime, 500, NULL);
	SetTimer(TimerID::UpdateConnect, 500, NULL);
}

void CSmartRayViewerDlg::KillTimers()
{
	KillTimer(TimerID::UpdateTime);
	KillTimer(TimerID::UpdateConnect);
}

void CSmartRayViewerDlg::OnBnClickedBtnMinimize()
{
	ShowWindow(SW_MINIMIZE);
}

void CSmartRayViewerDlg::OnBnClickedBtnExit()
{
	// 프로그램 종료 확인 메시지
	int result = AfxMessageBox(L"프로그램을 종료하시겠습니까?", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

	if (result == IDYES)
	{
		m_result.SetPointCloudCallback(nullptr);
		m_result.SetZMapCallback(nullptr);
		m_result.SetThicknessCallback(nullptr);

		// 확인 버튼이 눌러지면 프로그램 종료
		CDialogEx::OnCancel();
	}
}

void CSmartRayViewerDlg::DrawMonitoringSignalOnOff(int nCtrlID, COLORREF color)
{
	CDC* pDC = GetDC();
	CRect rt, rt1;
	int nOffset = 0;
	int nOffset2 = 0;
	int nSize = 3;
	GetWindowRect(&rt1);
	((CWnd*)(GetDlgItem(nCtrlID)))->GetWindowRect(&rt);
	rt.OffsetRect(-rt1.left, -rt1.top);
	//	rt1.SetRect(rt.left,rt.top,rt.right,rt.bottom);
	rt1.SetRect(rt.left - nOffset, rt.top - nOffset - nOffset2, rt.right - nOffset, rt.bottom - nOffset - nOffset2);

	CPen pen, pen1;
	CBrush brush, * oldBrush, brush1;


	brush1.CreateSolidBrush(AppColor::RGB_WEAK_BK_COLOR);
	brush.CreateSolidBrush(color);
	pDC->SelectObject(&brush1);
	//pDC->SetBkMode(TRANSPARENT);
	pen.CreatePen(PS_SOLID, 1, color);
	pen1.CreatePen(PS_SOLID, 1, AppColor::RGB_WEAK_BK_COLOR);
	CPen* oldPen = pDC->SelectObject(&pen1);

	pDC->Rectangle(&rt1);
	nSize = 10;

	pDC->SelectObject(&pen);
	oldBrush = pDC->SelectObject(&brush);
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

void CSmartRayViewerDlg::InitLayout()
{
	UIHelper::InitLabel(_vLabelLogo, L"", eLabelAlignH::Center, eLabelAlignV::Center, 20, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"Logo.png", 0);
	UIHelper::InitLabel(_vLabelTitle, L"SmartRay 3D Viewer", eLabelAlignH::Center, eLabelAlignV::Center, 56, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitLabel(_vLabelTime, L"2026/01/21 17:00:00", eLabelAlignH::Right, eLabelAlignV::Center, 30, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitLabel(_vLabelVersion, L"Program Ver. 1.0.0  Developer by Forvis.Inc", eLabelAlignH::Right, eLabelAlignV::Center, 18, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);

	UIHelper::InitIconButton(_vBtnMinimize, L"", L"Pgm_Minimize.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_vBtnExit, L"", L"Pgm_Off.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);

	UIHelper::InitLabel(_labelConnectSensor1, L"Sensor #1", eLabelAlignH::Right, eLabelAlignV::Center, 24, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitLabel(_labelConnectSensor2, L"Sensor #2", eLabelAlignH::Right, eLabelAlignV::Center, 24, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);

	UIHelper::InitLabel(_vLabelZmapTitle, L"Zmap ColorMap Viewer & ROI Setting", eLabelAlignH::Center, eLabelAlignV::Center, 24, AppColor::RGB_WHITE, AppColor::RGB_BLACK, L"", 0);
	UIHelper::InitLabel(_vLabelvMin, L"vMin : 0", eLabelAlignH::Left, eLabelAlignV::Center, 16, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitLabel(_vLabelvMax, L"vMax : 65535", eLabelAlignH::Left, eLabelAlignV::Center, 16, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitIconButton(_btnResult, L"ROI영역 연산", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_btnLoadImg, L"데이터 불러오기", L"LoadFile.png", 20, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);

	UIHelper::InitLabel(_vLabel3DTile, L"Point Cloude 3D Viewer", eLabelAlignH::Center, eLabelAlignV::Center, 24, AppColor::RGB_WHITE, AppColor::RGB_BLACK, L"", 0);
	UIHelper::InitIconButton(_btnStart, L"", L"Start.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_btnStop, L"", L"Stop.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_btnTopView, L"", L"TopView.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_btnFrontView, L"", L"FrontView.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_btnSideLeftView, L"", L"LeftView.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_btnLoad3DData, L"", L"LoadFile.png", 20, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_btnSetting, L"", L"Setting.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_btnOpenFolder, L"", L"Folder_Open.png", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);

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
}

void CSmartRayViewerDlg::InitImage()
{
	_image.SetUseTracker(true);
	_image.SetResizeTracker(false);
	_image.SetUIControlAttached(true);
	_image.FitImageToScreen();
	_image.ClearObjAll();

}

void CSmartRayViewerDlg::InitGrid()
{
	CFont font;
	font.CreatePointFont(100, L"SUIT SemiBold ");
	_vGridResult.SetFont(&font); // GridCtrl 전체에 적용

	_vGridResult.SetFixedRowCount(1);
	_vGridResult.SetRowCount(1);		//헤더만 생성

	_vGridResult.SetColumnCount(5);		//항목 5개


	//가로 폭 조절
	CRect rt;
	((CWnd*)(GetDlgItem(IDC_CUSTOM_RESULT_GRID)))->GetWindowRect(&rt);
	rt.SetRect(rt.left, rt.top, rt.right, rt.bottom);
	int totalWidth = rt.Width() - 20;
	int nCellWidth = totalWidth / _vGridResult.GetColumnCount();

	for (int i = 0; i < _vGridResult.GetColumnCount(); i++)
		_vGridResult.SetColumnWidth(i, nCellWidth);


	// 헤더
	const CStringW headers[5] = { L"No.", L"Avg", L"Max", L"Min", L"Count" };

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

void CSmartRayViewerDlg::InitSensor()
{
	m_result.SetSaveRoots(L"D:\\Data", L"E:\\Data");
	m_result.SetSaveEnabled(true);
	m_result.SetZMapSaveEnabled(true);
	m_result.SetZMapEnabled(true);
	m_result.SetThicknessEnabled(true);

	// UI 콜백(파이프라인 -> UI)
	m_result.SetPointCloudCallback([this](std::shared_ptr<PcFrame> frame)
		{
			auto* p = new std::shared_ptr<PcFrame>(std::move(frame));
			PostMessage(WM_PCFRAME_READY, 0, (LPARAM)p);
		});

	m_result.SetZMapCallback([this](std::shared_ptr<ZMapFrame> zf)
		{
			auto* p = new std::shared_ptr<ZMapFrame>(std::move(zf));
			PostMessage(WM_ZMAP_READY, 0, (LPARAM)p);
		});

	// thickness UI도 필요하면 여기서 PostMessage나 grid 갱신
	// m_result.SetThicknessCallback([this](const ThicknessFrame& tf){ ... });

	// 센서 설정
	std::string ip0 = AppStore::Get().GetParameterAsString("Sensor", "Sensor1_IP");
	short port0 = AppStore::Get().GetParameterAsInt("Sensor", "Sensor1_Port");

	m_Sensor0.Configure("Sensor0", ip0, (unsigned short)port0, 0);
	//m_Sensor0.SetParFilePath("C:\\SmartRay\\SmartRay DevKit\\SR_API\\sr_parameter_sets\\Pars_ECCO95\\ECCO95_3D_Snapshot.par");
	m_Sensor0.SetParFilePath("C:\\SmartRay\\SmartRay DevKit\\SR_API\\sr_parameter_sets\\Pars_ECCO95\\ECCO95_3D_Repeat_Snapshot.par");

	const int profiles = AppStore::Get().GetParameterAsInt("Sensor", "NumberOfProfiles");
	m_Sensor0.SetProfilesToCapture((uint32_t)profiles);
	m_Sensor0.SetMergeExpectedProfiles((uint32_t)profiles);

	const double X_Scale = AppStore::Get().GetParameterAsDouble("Sensor", "X_Scale");
	m_Sensor0.SetXScale((float)X_Scale);

	// 센서 -> 파이프라인 (센서는 파이프라인만 호출)
	m_Sensor0.SetFrameCallback([this](std::shared_ptr<PcFrame> f)
		{
			m_result.OnPointCloud(std::move(f));
		});

	if (!m_Sensor0.Connect(60))
	{
		AfxMessageBox(CString(m_Sensor0.GetLastErrorText().c_str()));
		return;
	}

	// sensor handle을 thickness용으로 등록(라이브러리가 필요할 때)
	m_result.RegisterAnySensorHandle(m_Sensor0.GetHandle());


	// 2번째 센서도 동일
	std::string ip1 = AppStore::Get().GetParameterAsString("Sensor", "Sensor2_IP");
	short port1 = AppStore::Get().GetParameterAsInt("Sensor", "Sensor2_Port");

	m_Sensor1.Configure("Sensor1", ip1, (unsigned short)port1, 1);
	//m_Sensor1.SetParFilePath("C:\\SmartRay\\SmartRay DevKit\\SR_API\\sr_parameter_sets\\Pars_ECCO95\\ECCO95_3D_Snapshot.par");
	m_Sensor1.SetParFilePath("C:\\SmartRay\\SmartRay DevKit\\SR_API\\sr_parameter_sets\\Pars_ECCO95\\ECCO95_3D_Repeat_Snapshot.par");
	m_Sensor1.SetProfilesToCapture((uint32_t)profiles);
	m_Sensor1.SetMergeExpectedProfiles((uint32_t)profiles);

	m_Sensor1.SetXScale((float)X_Scale);

	m_Sensor1.SetFrameCallback([this](std::shared_ptr<PcFrame> f)
		{
			m_result.OnPointCloud(std::move(f));
		});

	if (!m_Sensor1.Connect(60))
	{
		AfxMessageBox(CString(m_Sensor1.GetLastErrorText().c_str()));
		return;
	}
	m_result.RegisterAnySensorHandle(m_Sensor1.GetHandle());
}

void CSmartRayViewerDlg::ClearGridData()
{
	//아래는 전체 지우는거임
	int nRowCount = _vGridResult.GetRowCount();

	// 1행부터 끝까지 지움 (0행은 헤더)
	for (int i = nRowCount - 1; i >= 1; --i)
	{
		_vGridResult.DeleteRow(i);
	}

	_vGridResult.Refresh();

}

void CSmartRayViewerDlg::OnBnClickedButtonLoadImg()
{
	LogManager& logMgr = LogManager::GetInstance();

	CFileDialog dlg(TRUE, L"png", nullptr,
		OFN_FILEMUSTEXIST,
		L"Image Files (*.bmp;*.jpg;*.jpeg;*.png)|*.bmp;*.jpg;*.jpeg;*.png||");

	if (dlg.DoModal() != IDOK)
		return;

	CString path = dlg.GetPathName();

	if (!m_zmap.Load(path)) {
		OutputDebugString(L"[ZMAP] load failed\n");
		logMgr.PushLog(Log::Main, L"OnBnClickedButtonLoadImg", L"[ZMAP] load failed");

		return;
	}

	uint16_t dataMin = 1;
	uint16_t dataMax = 65535;

	// 데이터 min/max 얻기
	uint16_t invalidValue = 0;
	if (!m_zmap.GetDataMinMax(dataMin, dataMax, invalidValue)) {
		OutputDebugString(L"[ZMAP] min/max failed\n");
		logMgr.PushLog(Log::Main, L"OnBnClickedButtonLoadImg", L"[ZMAP] min/max failed");
		return;
	}

	// vImage 초기화(원본 해상도)
	_image.Init(m_zmap.Width(), m_zmap.Height(), 24);

	InitZMapSliders(dataMin, dataMax);
	m_hasZmap = true;

	UpdateZMapJet();

}

void CSmartRayViewerDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
	if (!pScrollBar) {
		CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
		return;
	}

	int id = pScrollBar->GetDlgCtrlID();
	if (id == IDC_SLIDER_VMIN) m_vmin = (uint16_t)m_sliderVmin.GetPos();
	if (id == IDC_SLIDER_VMAX) m_vmax = (uint16_t)m_sliderVmax.GetPos();

	// min/max 역전 방지
	if (m_vmin >= m_vmax) {
		if (id == IDC_SLIDER_VMIN) m_vmin = (uint16_t)(m_vmax - 1);
		else                      m_vmax = (uint16_t)(m_vmin + 1);

		m_sliderVmin.SetPos((int)m_vmin);
		m_sliderVmax.SetPos((int)m_vmax);
	}

	UpdateZMapValueLabels(); // 여기서 즉시 반영

	bool update =
		(nSBCode == SB_ENDSCROLL) ||
		(nSBCode == SB_THUMBPOSITION) ||
		(nSBCode == SB_THUMBTRACK) ||        // 추가: 드래그 중에도 호출
		(nSBCode == SB_LINELEFT) || (nSBCode == SB_LINERIGHT) ||
		(nSBCode == SB_PAGELEFT) || (nSBCode == SB_PAGERIGHT);

	// 실시간 드래그 원하면:
	// update = update || (nSBCode == SB_THUMBTRACK);

	if (update) UpdateZMapJet();

	CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CSmartRayViewerDlg::InitZMapSliders(uint16_t mn, uint16_t mx)
{
	m_vmin = mn;
	m_vmax = mx;

	m_sliderVmin.SetRange((int)mn, (int)mx, TRUE);
	m_sliderVmax.SetRange((int)mn, (int)mx, TRUE);

	m_sliderVmin.SetPos((int)m_vmin);
	m_sliderVmax.SetPos((int)m_vmax);

	UpdateZMapValueLabels();

}

void CSmartRayViewerDlg::UpdateZMapJet()
{
	LogManager& logMgr = LogManager::GetInstance();

	if (!m_hasZmap) return;

	if (m_vmin >= m_vmax) {
		m_vmax = (uint16_t)(m_vmin + 1);
		m_sliderVmax.SetPos((int)m_vmax);
	}

	if (!m_zmap.RenderJetTo(_image, m_vmin, m_vmax, 0, cv::Scalar(80, 80, 80))) {
		OutputDebugString(L"[ZMAP] RenderJetTo failed\n");
		logMgr.PushLog(Log::Main, L"UpdateZMapJet", L"[ZMAP] RenderJetTo failed");
		return;
	}

	_image.ClearObjAll();
	_image.DrawImage(eImageZoomType::NotResizeDraw);
	_image.DrawObjAll();
}

void CSmartRayViewerDlg::UpdateZMapValueLabels()
{
	CString s;

	s.Format(L"Vmin : %u", m_vmin);
	_vLabelvMin.SetText(std::wstring(s.GetString()));

	s.Format(L"Vmax : %u", m_vmax);
	_vLabelvMax.SetText(std::wstring(s.GetString()));

	// vLabel이 "SetText만 하면 안그려지는" 타입이면 Draw까지 호출
	_vLabelvMin.Draw();
	_vLabelvMax.Draw();
}

void CSmartRayViewerDlg::OnBnClickedButtonResult()
{
	ComputeRoiStats_AndFillGrid();
}

void CSmartRayViewerDlg::DrawZMapColorBar(CDC* pDC)
{
	// ✅ ZMap 출력 컨트롤 기준으로 위치 잡기
	CRect rc;
	GetDlgItem(IDC_IMAGE_VIEW)->GetWindowRect(&rc);
	ScreenToClient(&rc);  // 중요

	// -----------------------------
	// 가로 컬러바 위치 (ZMap 아래쪽)
	// -----------------------------
	int barHeight = 25;

	CRect barRect(
		rc.left,
		rc.bottom + 10,
		rc.right,
		rc.bottom + 10 + barHeight
	);

	// 다이얼로그 밖이면 그냥 리턴(디버깅용 안전장치)
	CRect client;
	GetClientRect(&client);
	if (barRect.bottom > client.bottom)
		return;

	int width = barRect.Width();
	if (width <= 1) return;

	// ✅ 좌 -> 우 그라데이션
	for (int x = 0; x < width; ++x)
	{
		double t = (double)x / (width - 1);  // 0~1
		COLORREF color = GetJetColor(t);

		pDC->FillSolidRect(
			barRect.left + x,
			barRect.top,
			1,
			barRect.Height(),
			color
		);
	}

	// -----------------------------
	// 눈금 텍스트 (좌=Min, 우=Max)
	// -----------------------------
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(RGB(255, 255, 255));

}

COLORREF CSmartRayViewerDlg::GetJetColor(double t)
{
	t = max(0.0, min(1.0, t));

	double r = 0.0, g = 0.0, b = 0.0;

	if (t < 0.5)
	{
		// Blue -> Green
		double u = t / 0.5;   // 0~1
		r = 0.0;
		g = u;
		b = 1.0 - u;
	}
	else
	{
		// Green -> Red
		double u = (t - 0.5) / 0.5; // 0~1
		r = u;
		g = 1.0 - u;
		b = 0.0;
	}

	return RGB((int)(r * 255), (int)(g * 255), (int)(b * 255));
}

void CSmartRayViewerDlg::OnBnClickedButtonLoad3dData()
{
	if (m_Sensor0.IsRunning() || m_Sensor1.IsRunning()) {
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

	// 파일명만 뽑기
	CString fnameC = dlg.GetFileName();
	std::wstring fname = (LPCWSTR)fnameC;

	int camFromName = 0;
	uint64_t frameNoFromName = 0;
	bool okParse = ParseSensorAndFrameNoFromFilename(fname, camFromName, frameNoFromName);

	// ✅ 1) 파일 → PcFrame
	// camIndex는 "파일명 기준"으로 넣는 게 맞음 (파싱 실패 시 0)
	auto frame = m_vtkView.LoadPointCloudToFrame(path, okParse ? camFromName : 0);
	if (!frame)
	{
		AfxMessageBox(L"포인트 클라우드 로드 실패\n(확장자/포맷/데이터를 확인해 주세요)");
		return;
	}

	// ✅ 2) frameNo도 파일명 기준 (파싱 실패 시 fallback)
	if (okParse)
		frame->frameNo = frameNoFromName;
	else
		frame->frameNo = 0; // 또는 static 증가

	m_result.OnPointCloud(frame);
}

void CSmartRayViewerDlg::OnBnClickedButtonStart()
{
	if (!m_Sensor0.IsConnected() || !m_Sensor1.IsConnected()) {
		LogManager::GetInstance().PushLog(Log::Main, L"OnBnClickedButtonStart", L"센서 연결 상태를 확인해주세요.");
		AfxMessageBox(L"센서 연결 상태를 확인해주세요.");
		return;
	}

	const int profiles = AppStore::Get().GetParameterAsInt("Sensor", "NumberOfProfiles");
	m_Sensor0.SetProfilesToCapture((uint32_t)profiles);
	m_Sensor0.SetMergeExpectedProfiles((uint32_t)profiles);

	const double X_Scale = AppStore::Get().GetParameterAsDouble("Sensor", "X_Scale");
	m_Sensor0.SetXScale((float)X_Scale);

	m_Sensor1.SetProfilesToCapture((uint32_t)profiles);
	m_Sensor1.SetMergeExpectedProfiles((uint32_t)profiles);

	m_Sensor1.SetXScale((float)X_Scale);

	//m_Sensor.StopPointCloud();
	//m_Sensor2.StopPointCloud();

	// 센서 1개만 먼저 출력
	m_Sensor0.Start();
	m_Sensor1.Start();
}

void CSmartRayViewerDlg::OnBnClickedButtonStop()
{
	if (!m_Sensor0.IsConnected() || !m_Sensor1.IsConnected()) {
		LogManager::GetInstance().PushLog(Log::Main, L"OnBnClickedButtonStop", L"센서 연결 상태를 확인해주세요.");
		AfxMessageBox(L"센서 연결 상태를 확인해주세요.");
		return;
	}

	m_Sensor0.Stop();
	m_Sensor1.Stop();
}

void CSmartRayViewerDlg::OnBnClickedButtonSetting()
{
	if (_dlgParam->IsWindowVisible())
		_dlgParam->ShowWindow(SW_HIDE);
	else
		_dlgParam->ShowWindow(SW_SHOW);
}

void CSmartRayViewerDlg::OnBnClickedButtonOpenFolder()
{
	namespace fs = std::filesystem;

	const std::wstring date = _Util.MakeTimestamp(vUtil::TimeUnit::Day, true, false);
	fs::path folder = L"D:\\Data\\" + date;


	// 폴더 없으면 자동 생성
	if (!fs::exists(folder))
	{
		fs::create_directories(folder);
	}

	// 탐색기 실행
	ShellExecute(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CSmartRayViewerDlg::OnBnClickedButtonTopView()
{
	m_vtkView.ViewTop();
}

void CSmartRayViewerDlg::OnBnClickedButtonFrontView()
{
	m_vtkView.ViewFront();
}

void CSmartRayViewerDlg::OnBnClickedButtonSideLeftView()
{
	m_vtkView.ViewSide();
}


LRESULT CSmartRayViewerDlg::OnPcFrameReady(WPARAM, LPARAM lParam)
{
	auto* pFramePtr = reinterpret_cast<std::shared_ptr<PcFrame>*>(lParam);
	std::shared_ptr<PcFrame> frame = (pFramePtr ? *pFramePtr : nullptr);
	delete pFramePtr;

	if (!frame || frame->points.empty())
		return 0;

	m_vtkView.UpdatePointCloud(frame->points);

	LogManager& logMgr = LogManager::GetInstance();
	std::wstring strLog = L"PointCloud Update / frameNo=" + std::to_wstring(frame->frameNo) +
		L" count=" + std::to_wstring(frame->points.size());
	logMgr.PushLog(Log::Main, L"OnPcFrameReady", strLog);

	return 0;

}

LRESULT CSmartRayViewerDlg::OnZMapReady(WPARAM, LPARAM lParam)
{
	auto* pZPtr = reinterpret_cast<std::shared_ptr<ZMapFrame>*>(lParam);
	std::shared_ptr<ZMapFrame> zf = (pZPtr ? *pZPtr : nullptr);
	delete pZPtr;

	if (!zf || zf->z.empty() || zf->w <= 0 || zf->h <= 0)
		return 0;

	// ✅ CZMapRenderer에 raw u16을 세팅하는 함수가 필요함
	// 아래 함수가 없다면 ZMapRenderer에 추가해야 함(바로 아래에 추가 코드 줄게)
	if (!m_zmap.SetFromRawU16(zf->w, zf->h, zf->z.data()))
		return 0;

	uint16_t dataMin = 1, dataMax = 65535;
	uint16_t invalidValue = 0;
	if (!m_zmap.GetDataMinMax(dataMin, dataMax, invalidValue))
		return 0;

	InitZMapSliders(dataMin, dataMax);


	const int w = m_zmap.Width();
	const int h = m_zmap.Height();
	const bool sizeChanged = (w != m_lastZImgW) || (h != m_lastZImgH);

	if (sizeChanged)
	{
		// VTK/Grid, 이미지 버퍼 재초기화 (크기 바뀔 때만)
		_image.Init(w, h, 24);

		m_lastZImgW = w;
		m_lastZImgH = h;
	}

	m_hasZmap = true;


	UpdateZMapJet();

	if (m_roiAutoUpdate)
		ComputeRoiStats_AndFillGrid();

	return 0;
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

	for (int c = 0; c < 5; ++c)
	{
		_vGridResult.SetItemFormat(nRow, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		_vGridResult.SetItemText(nRow, c, cols[c]);
		_vGridResult.SetItemFgColour(nRow, c, RGB(0, 0, 0));
		_vGridResult.SetItemState(nRow, c, GVIS_READONLY);
	}

	_vGridResult.SetRowHeight(nRow, 50);
	_vGridResult.Invalidate();
}

void CSmartRayViewerDlg::ComputeRoiStats_AndFillGrid()
{
	ClearGridData();

	const int roiCnt = _image.GetCountTrackerROI();
	if (roiCnt <= 0) return;

	if (m_roiSource == RoiStatSource::ZMap16)
	{
		if (!m_hasZmap)
		{
			//AfxMessageBox(L"ZMap 데이터가 없습니다.\n먼저 측정하거나 ZMap을 불러오세요.");
			return;
		}

		// 프로젝트 invalid 규약에 맞춰서
		const uint16_t invalidValue = 0;
		ComputeRoiStats_FromZMap(invalidValue);
		return;
	}

	// PointCloud
	auto frame = m_result.GetLastPcFrame(0);
	if (!frame || frame->points.empty())
	{
		//AfxMessageBox(L"PointCloud 데이터가 없습니다.");
		return;
	}

	const bool rotateCW90 = true;
	ComputeRoiStats_FromPointCloud(*frame, rotateCW90);
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

	// xmin/ymin 기준점
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
