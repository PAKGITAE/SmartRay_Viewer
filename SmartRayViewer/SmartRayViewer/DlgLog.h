#pragma once
#include "afxdialogex.h"
#include "vListCtrl.h"
#include "UIHelper.h"

// 로그 타입 enum
enum class LogTabType
{
	System = 0,
	Sensor,
	Copy,
	Viewer,
};

// 로그 항목 구조체
struct LogEntry
{
	std::wstring time;
	std::wstring textClass;
	std::wstring textFunc;
	std::wstring message;
};

// DlgLog 대화 상자

class DlgLog : public CDialogEx
{
	DECLARE_DYNAMIC(DlgLog)

public:
	DlgLog(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~DlgLog();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_LOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()

private:
	vListCtrl _vListCtrl;

	vIconButton _vBtnAllDelete;

public:
	virtual BOOL OnInitDialog();
	void InitListCtrl();
	void AddLog(const std::wstring& textClass, const std::wstring& textFunc, const std::wstring& message = L"");

	virtual void OnOK();
	virtual void OnCancel();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnBnClickedButtonDeleteLog();
};
