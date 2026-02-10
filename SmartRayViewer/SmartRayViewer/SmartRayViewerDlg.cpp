
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

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

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

	ON_MESSAGE(WM_PCFRAME_READY, &CSmartRayViewerDlg::OnPcFrameReady)
	ON_BN_CLICKED(IDC_BUTTON_TOP_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonTopView)
	ON_BN_CLICKED(IDC_BUTTON_FRONT_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonFrontView)
	ON_BN_CLICKED(IDC_BUTTON_SIDE_LEFT_VIEW, &CSmartRayViewerDlg::OnBnClickedButtonSideLeftView)
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

	LogManager::GetInstance().PushLog(Log::System, L"OnInitDialog", L"PGM START");

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

	m_Sensor.Disconnect();
	m_Sensor2.Disconnect();

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
		if (m_Sensor.IsConnected()) {
			DrawMonitoringSignalOnOff(IDC_SIGNAL_SENSOR_CONNECT_1, AppColor::RGB_GREEN);
		}
		else {
			DrawMonitoringSignalOnOff(IDC_SIGNAL_SENSOR_CONNECT_1, AppColor::RGB_RED);
		}

		if (m_Sensor2.IsConnected()) {
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
		Result::GetInstance().SetUiFrameCallback(nullptr);

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
	UIHelper::InitLabel(_vLabelvMin, L"vMin : 0", eLabelAlignH::Right, eLabelAlignV::Center, 16, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitLabel(_vLabelvMax, L"vMax : 65535", eLabelAlignH::Right, eLabelAlignV::Center, 16, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
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
	Result::GetInstance().SetRootFolder(L"D:\\Data");

	std::string strIP = AppStore::Get().GetParameterAsString("Sensor", "Sensor1_IP");
	short sPort = AppStore::Get().GetParameterAsInt("Sensor", "Sensor1_Port");

	m_Sensor.Configure("Sensor1", strIP, sPort, 0);

	if (!m_Sensor.Connect(60))
	{
		LogManager::GetInstance().PushLog(Log::System, L"InitSensor", L"Sensor#1 Connect Fail");

		CString msg(m_Sensor.GetLastErrorText().c_str());
		AfxMessageBox(msg);
		return;
	}

	strIP = AppStore::Get().GetParameterAsString("Sensor", "Sensor2_IP");
	sPort = AppStore::Get().GetParameterAsInt("Sensor", "Sensor2_Port");

	m_Sensor2.Configure("Sensor2", strIP, sPort, 1);

	if (!m_Sensor2.Connect(60))
	{
		LogManager::GetInstance().PushLog(Log::System, L"InitSensor", L"Sensor#2 Connect Fail");

		CString msg(m_Sensor2.GetLastErrorText().c_str());
		AfxMessageBox(msg);
		return;
	}

	Result::GetInstance().SetUiFrameCallback([this](std::shared_ptr<PcFrame> frame)
		{
			// worker thread → UI thread
			auto* p = new std::shared_ptr<PcFrame>(std::move(frame));
			PostMessage(WM_PCFRAME_READY, 0, (LPARAM)p);
		});
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
		logMgr.PushLog(Log::System, L"OnBnClickedButtonLoadImg", L"[ZMAP] load failed");

		return;
	}

	uint16_t dataMin = 1;
	uint16_t dataMax = 65535;

	// 데이터 min/max 얻기
	uint16_t invalidValue = 0;
	if (!m_zmap.GetDataMinMax(dataMin, dataMax, invalidValue)) {
		OutputDebugString(L"[ZMAP] min/max failed\n");
		logMgr.PushLog(Log::System, L"OnBnClickedButtonLoadImg", L"[ZMAP] min/max failed");
		return;
	}

	// vImage 초기화(원본 해상도)
	_image.Init(m_zmap.Width(), m_zmap.Height(), 24);

	InitZMapSliders(dataMin, dataMax);
	m_hasZmap = true;

	m_vtkView.SetGridSize(m_zmap.Width(), m_zmap.Height());

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
		logMgr.PushLog(Log::System, L"UpdateZMapJet", L"[ZMAP] RenderJetTo failed");
		return;
	}

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

void CSmartRayViewerDlg::AddGridMeasurePC(int RoiNo, const HeightStats& st)
{
	int nRow = _vGridResult.GetRowCount();
	_vGridResult.SetRowCount(nRow + 1);

	CString cols[5];
	cols[0].Format(L"%d", RoiNo);
	cols[1].Format(L"%.3f", st.mean);
	cols[2].Format(L"%.3f", st.maxv);
	cols[3].Format(L"%.3f", st.minv);
	cols[4].Format(L"%lld", st.count);

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

void CSmartRayViewerDlg::OnBnClickedButtonResult()
{
	ClearGridData();

	for (int i = 0; i < _image.GetCountTrackerROI(); i++)
	{
		CRect roi = _image.GetTrackerROI(i);

		HeightStats stPC;
		if (m_vtkView.GetHeightStatsInRoiPixel(roi, stPC))
		{
			AddGridMeasurePC(i, stPC);
		}
		else
		{
			// ROI에 유효 포인트가 없으면 빈 값 혹은 0으로 넣고 싶다면 여기서 처리
			OutputDebugString(L"[PC ROI] stats failed\n");
		}
	}

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
	if (m_Sensor.IsRunning() || m_Sensor2.IsRunning()) {
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

	if (!m_vtkView.LoadPointCloud(path))
	{
		AfxMessageBox(L"포인트 클라우드 로드 실패\n(확장자/포맷/데이터를 확인해 주세요)");
		return;
	}
}

void CSmartRayViewerDlg::OnBnClickedButtonStart()
{
	if (!m_Sensor.IsConnected() || !m_Sensor2.IsConnected()) {
		LogManager::GetInstance().PushLog(Log::System, L"OnBnClickedButtonStart", L"센서 연결 상태를 확인해주세요.");
		AfxMessageBox(L"센서 연결 상태를 확인해주세요.");
		return;
	}

	// 센서 1개만 먼저 출력
	m_Sensor.StartPointCloud();
	m_Sensor2.StartPointCloud();
}

void CSmartRayViewerDlg::OnBnClickedButtonStop()
{
	if (!m_Sensor.IsConnected() || !m_Sensor2.IsConnected()) {
		LogManager::GetInstance().PushLog(Log::System, L"OnBnClickedButtonStop", L"센서 연결 상태를 확인해주세요.");
		AfxMessageBox(L"센서 연결 상태를 확인해주세요.");
		return;
	}

	m_Sensor.StopPointCloud();
	m_Sensor2.StopPointCloud();
}

void CSmartRayViewerDlg::OnBnClickedButtonSetting()
{
	if (_dlgParam->IsWindowVisible())
		_dlgParam->ShowWindow(SW_HIDE);
	else
		_dlgParam->ShowWindow(SW_SHOW);
}

LRESULT CSmartRayViewerDlg::OnPcFrameReady(WPARAM, LPARAM lParam)
{
	auto* pFramePtr = reinterpret_cast<std::shared_ptr<PcFrame>*>(lParam);
	std::shared_ptr<PcFrame> frame = (pFramePtr ? *pFramePtr : nullptr);
	delete pFramePtr;

	if (!frame || frame->points.empty())
		return 0;

	std::vector<float> xyz;
	xyz.reserve(frame->points.size() * 3);
	for (const auto& pt : frame->points)
	{
		if (pt.x == INVALID || pt.y == INVALID || pt.z == INVALID)
			continue;

		xyz.push_back((float)pt.x);
		xyz.push_back((float)pt.y);
		xyz.push_back((float)pt.z);
	}

	m_vtkView.UpdatePointCloudXYZ(xyz);

	return 0;
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
