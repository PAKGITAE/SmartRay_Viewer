
// SmartRayViewerDlg.h: 헤더 파일
//

#pragma once

#include "vGridCtrl.h"
#include "vUtil.h"
#include "vImage.h"

#include "UIHelper.h"
#include "ColorDefine.h"
#include "TimerDefine.h"

#include "ZMapRenderer.h"
#include "VtkPointCloudView.h"

#include "LogManager.h"
#include "DlgLog.h"
#include "DlgParam.h"

#include "SmartRaySensor.h"

constexpr UINT WM_PCFRAME_READY = WM_APP + 10;
constexpr double INVALID = -999999.0;

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
	afx_msg void OnDestroy();

	afx_msg void OnTimer(UINT_PTR nIDEvent);
	void SetTimers();
	void KillTimers();

	void InitClass();

	void InitLayout();
	void InitGrid();
	void ClearGridData();
	void AddGridMeasurePC(int RoiNo, const HeightStats& st);

private:

	CZMapRenderer m_zmap;
	vImage _image{ IMG_WIDTH, IMG_HEIGHT, eImageDepth::Color, eImageModeUI::UI };

	uint16_t m_vmin = 1;
	uint16_t m_vmax = 65535;
	bool m_hasZmap = false;

	void DrawMonitoringSignalOnOff(int nCtrlID, COLORREF color);

	// 이미지 뷰어
	void InitImage();

	//슬라이드 바
	void InitZMapSliders(uint16_t mn, uint16_t mx);
	void UpdateZMapJet();
	void UpdateZMapValueLabels();

	// 컬러바 표시
	void DrawZMapColorBar(CDC* pDC);
	COLORREF GetJetColor(double t);

private:
	vUtil _Util;
	vGridCtrl _vGridResult;

	DlgLog _dlgLog;
	DlgParam* _dlgParam;


private:
	vLabel _vLabelLogo;
	vLabel _vLabelTitle;
	vLabel _vLabelVersion;
	vLabel _vLabelTime;

	vLabel _vLabel3DTile;
	vLabel _vLabelZmapTitle;

	vIconButton _vBtnMinimize;
	vIconButton _vBtnExit;
	vIconButton _btnLoadImg;
	vIconButton _btnResult;
	vIconButton _btnLoad3DData;

	vIconButton _btnStart;
	vIconButton _btnStop;
	vIconButton _btnSetting;

	vIconButton _btnTopView;
	vIconButton _btnFrontView;
	vIconButton _btnSideLeftView;


	vLabel _labelConnectSensor1;
	vLabel _labelConnectSensor2;

	CSliderCtrl m_sliderVmin;
	CSliderCtrl m_sliderVmax;
	vLabel _vLabelvMin;
	vLabel _vLabelvMax;

private:
	CVtkPointCloudView m_vtkView;
	afx_msg LRESULT OnPcFrameReady(WPARAM, LPARAM);

private:
	void InitSensor();
	SmartRaySensor m_Sensor;
	SmartRaySensor m_Sensor2;

public:
	afx_msg void OnBnClickedButtonLoadImg();
	afx_msg void OnBnClickedBtnMinimize();
	afx_msg void OnBnClickedBtnExit();
	afx_msg void OnBnClickedButtonResult();
	afx_msg void OnBnClickedButtonLoad3dData();
	afx_msg void OnBnClickedButtonStart();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonSetting();
	afx_msg void OnBnClickedButtonTopView();
	afx_msg void OnBnClickedButtonFrontView();
	afx_msg void OnBnClickedButtonSideLeftView();
};
