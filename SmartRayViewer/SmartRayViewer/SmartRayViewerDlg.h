
// SmartRayViewerDlg.h: 헤더 파일
//

#pragma once

#include "vLabel.h"
#include "vIconButton.h"
#include "vUtil.h"
#include "vImage.h"

#include "UIHelper.h"
#include "ColorDefine.h"
#include "TimerDefine.h"

#include "ZMapRenderer.h"

// CSmartRayViewerDlg 대화 상자
class CSmartRayViewerDlg : public CDialogEx
{
// 생성입니다.
public:
	CSmartRayViewerDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SMARTRAYVIEWER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	afx_msg void OnTimer(UINT_PTR nIDEvent);
	void SetTimers();
	void KillTimers();

	void InitLayout();

	void InitImage();
	vImage _image{ IMG_WIDTH, IMG_HEIGHT, eImageDepth::Color, eImageModeUI::UI };


private:
	void InitZMapSliders(uint16_t mn, uint16_t mx);
	void UpdateZMapJet();
	void UpdateZMapValueLabels();

private:
	vUtil _Util;

private:
	vLabel _vLabelLogo;
	vLabel _vLabelTitle;
	vLabel _vLabelVersion;
	vLabel _vLabelTime;

	vIconButton _vBtnMinimize;
	vIconButton _vBtnExit;
	vIconButton _btnLoadImg;

	CSliderCtrl m_sliderVmin;
	CSliderCtrl m_sliderVmax;
	vLabel _vLabelvMin;
	vLabel _vLabelvMax;

private:
	CZMapRenderer m_zmap;

	uint16_t m_vmin = 1;
	uint16_t m_vmax = 65535;
	bool m_hasZmap = false;

public:
	afx_msg void OnBnClickedButtonLoadImg();
	afx_msg void OnBnClickedBtnMinimize();
	afx_msg void OnBnClickedBtnExit();
};
