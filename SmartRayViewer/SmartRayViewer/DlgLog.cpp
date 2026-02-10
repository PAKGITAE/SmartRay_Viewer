// DlgLog.cpp: 구현 파일
//

#include "pch.h"
#include "SmartRayViewer.h"
#include "afxdialogex.h"
#include "DlgLog.h"
#include "ColorDefine.h"


// DlgLog 대화 상자

IMPLEMENT_DYNAMIC(DlgLog, CDialogEx)

DlgLog::DlgLog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_LOG, pParent)
{

}

DlgLog::~DlgLog()
{
}

void DlgLog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_LOG, _vListCtrl);
	DDX_Control(pDX, IDC_BUTTON_DELETE_LOG, _vBtnAllDelete);
	
}


BEGIN_MESSAGE_MAP(DlgLog, CDialogEx)
	ON_WM_ERASEBKGND()
	ON_BN_CLICKED(IDC_BUTTON_DELETE_LOG, &DlgLog::OnBnClickedButtonDeleteLog)
END_MESSAGE_MAP()


// DlgLog 메시지 처리기

BOOL DlgLog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  여기에 추가 초기화 작업을 추가합니다.
	InitListCtrl();

	UIHelper::InitIconButton(_vBtnAllDelete, L"로그 전체 삭제", L"Delete.png", 24, true, AppColor::RGB_WHITE, AppColor::RGB_GRAY, AppColor::BUTTON_DOWN_RGB);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 예외: OCX 속성 페이지는 FALSE를 반환해야 합니다.
}

void DlgLog::OnOK()
{
	// TODO: 여기에 특수화된 코드를 추가 및/또는 기본 클래스를 호출합니다.

	//CDialogEx::OnOK();
}

void DlgLog::OnCancel()
{
	// TODO: 여기에 특수화된 코드를 추가 및/또는 기본 클래스를 호출합니다.

	//CDialogEx::OnCancel();
}

BOOL DlgLog::OnEraseBkgnd(CDC* pDC)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
	CRect rect;
	GetClientRect(rect);
	pDC->FillSolidRect(rect, AppColor::RGB_WEAK_BK_COLOR);
	return TRUE;

	//return CDialogEx::OnEraseBkgnd(pDC);
}


void DlgLog::InitListCtrl()
{
	// 리스트 컨트롤 컬럼 설정
	_vListCtrl.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 120);
	_vListCtrl.InsertColumn(1, _T("Class"), LVCFMT_LEFT, 50);
	_vListCtrl.InsertColumn(2, _T("Function"), LVCFMT_LEFT, 70);
	_vListCtrl.InsertColumn(3, _T("Message"), LVCFMT_LEFT, 250);


	// ListView 스타일 설정
	_vListCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	// 폰트 타입과 크기 설정
	_vListCtrl.SetFontType(8, L"맑은 고딕");

	// 데이터 설정 (10000행 x 4열)
	_vListCtrl.SetDimensions(10000, 4);

	// 기본 그룹
	_vListCtrl.SetCurrentGroup(0);

	// 자동 스크롤 기능
	_vListCtrl.SetAutoScroll(true);

}

void DlgLog::AddLog(const std::wstring& textClass, const std::wstring& textFunc, const std::wstring& message)
{
	// 현재 시간 가져오기
	SYSTEMTIME st;
	GetLocalTime(&st);
	CString timeStr;
	timeStr.Format(_T("%04d-%02d-%02d %02d:%02d:%02d"),
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond);

	// 로그 항목 생성
	LogEntry entry;
	entry.time = std::wstring((LPCTSTR)timeStr);
	entry.textClass= textClass;
	entry.textFunc = textFunc;
	entry.message = message;


	_vListCtrl.AddData(
		0,
		{ entry.time, entry.textClass, entry.textFunc, entry.message },
		RGB(0, 0, 0)
	);
}

void DlgLog::OnBnClickedButtonDeleteLog()
{
	_vListCtrl.DeleteAllLogs();
}
