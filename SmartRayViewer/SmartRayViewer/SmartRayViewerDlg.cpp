
// SmartRayViewerDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "SmartRayViewer.h"
#include "SmartRayViewerDlg.h"
#include "afxdialogex.h"

#include <fstream>
#include <vector>


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

	DDX_Control(pDX, IDC_BTN_MINIMIZE, _vBtnMinimize);
	DDX_Control(pDX, IDC_BTN_EXIT, _vBtnExit);

	DDX_Control(pDX, IDC_BUTTON_LOAD_IMG, _btnLoadImg);

	DDX_Control(pDX, IDC_SLIDER_VMIN, m_sliderVmin);
	DDX_Control(pDX, IDC_SLIDER_VMAX, m_sliderVmax);
	DDX_Control(pDX, IDC_LABEL_VMIN, _vLabelvMin);
	DDX_Control(pDX, IDC_LABEL_VMAX, _vLabelvMax);

	DDX_Control(pDX, IDC_IMAGE_VIEW, _image);

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
	InitLayout();
	SetTimers();
	InitImage();

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

void CSmartRayViewerDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TimerID::UpdateTime) {
		std::wstring wStrTime = _Util.MakeTimestamp(vUtil::TimeUnit::Second, false, false);

		_vLabelTime.SetText(wStrTime);
		_vLabelTime.Draw();
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CSmartRayViewerDlg::SetTimers()
{
	SetTimer(TimerID::UpdateTime, 500, NULL);

}

void CSmartRayViewerDlg::KillTimers()
{
	KillTimer(TimerID::UpdateTime);
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
		// 확인 버튼이 눌러지면 프로그램 종료
		CDialogEx::OnCancel();
	}
}

void CSmartRayViewerDlg::InitLayout()
{
	UIHelper::InitLabel(_vLabelLogo, L"", eLabelAlignH::Center, eLabelAlignV::Center, 20, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"Logo.png", 0);
	UIHelper::InitLabel(_vLabelTitle, L"SmartRay 3D Viewer", eLabelAlignH::Center, eLabelAlignV::Center, 56, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitLabel(_vLabelTime, L"2026/01/21 17:00:00", eLabelAlignH::Right, eLabelAlignV::Center, 30, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitLabel(_vLabelVersion, L"Program Ver. 1.0.0  Developer by Forvis.Inc", eLabelAlignH::Right, eLabelAlignV::Center, 18, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);

	UIHelper::InitLabel(_vLabelvMin, L"vMin : 0", eLabelAlignH::Right, eLabelAlignV::Center, 16, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);
	UIHelper::InitLabel(_vLabelvMax, L"vMax : 65535", eLabelAlignH::Right, eLabelAlignV::Center, 16, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, L"", 0);


	UIHelper::InitIconButton(_vBtnMinimize, L"ㅡ", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);
	UIHelper::InitIconButton(_vBtnExit, L"X", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_WEAK_BK_COLOR, AppColor::BUTTON_DOWN_RGB);

	UIHelper::InitIconButton(_btnLoadImg, L"데이터 불러오기", L"", 28, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);
}

void CSmartRayViewerDlg::InitImage()
{
	_image.SetUseTracker(true);
	_image.SetResizeTracker(false);
	_image.SetUIControlAttached(true);
	_image.FitImageToScreen();
	_image.ClearObjAll();

}

void CSmartRayViewerDlg::OnBnClickedButtonLoadImg()
{
	CFileDialog dlg(TRUE, L"png", nullptr,
		OFN_FILEMUSTEXIST,
		L"Image Files (*.bmp;*.jpg;*.jpeg;*.png)|*.bmp;*.jpg;*.jpeg;*.png||");

	if (dlg.DoModal() != IDOK)
		return;

	CString path = dlg.GetPathName();

	if (!m_zmap.Load(path)) {
		OutputDebugString(L"[ZMAP] load failed\n");
		return;
	}

	uint16_t dataMin = 1;
	uint16_t dataMax = 65535;

	// 데이터 min/max 얻기
	if (!m_zmap.GetDataMinMax(dataMin, dataMax)) {
		OutputDebugString(L"[ZMAP] min/max failed\n");
		return;
	}

	// vImage 초기화(원본 해상도)
	_image.Init(m_zmap.Width(), m_zmap.Height(), 24);

	InitZMapSliders(dataMin, dataMax);
	m_hasZmap = true;

	UpdateZMapJet();

	// 일단 전체 범위로 렌더
	if (!m_zmap.RenderJetTo(_image, dataMin, dataMax, 0, cv::Scalar(80, 80, 80))) {
		OutputDebugString(L"[ZMAP] render failed\n");
		return;
	}

	_image.DrawImage(eImageZoomType::NotResizeDraw);
	_image.DrawObjAll();


	CRect roi1 = _image.GetTrackerROI(0);

	CZMapRenderer::ZRoiStats st;
	if (m_zmap.GetStatsInRoi(roi1, st, /*invalidValue=*/0))
	{
		wchar_t buf[256];
		swprintf_s(buf, L"[ROI] cnt=%lld min=%u max=%u mean=%.2f\n",
			st.count, st.minv, st.maxv, st.mean);
		OutputDebugString(buf);

		// 라벨에도 띄우고 싶으면 SetText/Draw
	}
	else
	{
		OutputDebugString(L"[ROI] stats failed (empty roi or all invalid)\n");
	}

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
	if (!m_hasZmap) return;

	if (m_vmin >= m_vmax) {
		m_vmax = (uint16_t)(m_vmin + 1);
		m_sliderVmax.SetPos((int)m_vmax);
	}

	if (!m_zmap.RenderJetTo(_image, m_vmin, m_vmax, 0, cv::Scalar(80, 80, 80))) {
		OutputDebugString(L"[ZMAP] RenderJetTo failed\n");
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
