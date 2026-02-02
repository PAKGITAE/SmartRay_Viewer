
// SmartRayViewerDlg.h: 헤더 파일
//

#pragma once

#include "vLabel.h"
#include "vIconButton.h"
#include "vGridCtrl.h"
#include "vUtil.h"
#include "vLog.h"
#include "vImage.h"

#include "UIHelper.h"
#include "ColorDefine.h"
#include "TimerDefine.h"

#include "ZMapRenderer.h"

#define vtkRenderingCore_AUTOINIT 3(vtkRenderingOpenGL2,vtkInteractionStyle, vtkRenderingFreeType)
#define vtkRenderingContext2D_AUTOINIT 1(vtkRenderingContextOpenGL2)

#include <vtkAutoInit.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkPLYReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkProperty.h>

#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkCaptionActor2D.h>
#include <vtkTextActor.h>

#include <vtkCamera.h>

#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkLookupTable.h>


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

	void InitLog();
	void WriteLog(std::wstring logKey, std::wstring logMsg) { _logMain.PushLog(logKey, logMsg); }

	void InitLayout();
	void InitGrid();
	void ClearGridData();
	void AddGridMeasure(int RoiNo, ZRoiStats ResultData);

private:

	CZMapRenderer m_zmap;
	vImage _image{ IMG_WIDTH, IMG_HEIGHT, eImageDepth::Color, eImageModeUI::UI };

	uint16_t m_vmin = 1;
	uint16_t m_vmax = 65535;
	bool m_hasZmap = false;

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
	vLog _logMain;

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
	vIconButton _btnDefaultPos;

	CSliderCtrl m_sliderVmin;
	CSliderCtrl m_sliderVmax;
	vLabel _vLabelvMin;
	vLabel _vLabelvMax;

private:
	vtkNew<vtkRenderWindow> m_vtkRenderWindow;

	// ✅ Renderer/Actor는 1개만 유지해서 누적(AddRenderer) 문제 방지
	vtkSmartPointer<vtkRenderer> m_vtkRenderer;
	vtkSmartPointer<vtkActor>    m_vtkActor;   // 현재 표시 중인 Actor(교체용)

	vtkSmartPointer<vtkAxesActor> m_axesActor;
	vtkSmartPointer<vtkOrientationMarkerWidget> m_axesWidget;

	vtkSmartPointer<vtkCamera> m_homeCamera;  // 초기 시점 저장용
	bool m_hasHomeCamera = false;
	void SaveHomeCamera();
	void RestoreHomeCamera();

	// 파일 확장자에 따라 PLY / ASC(XYZ 텍스트) 자동 로딩
	bool LoadPointCloudAuto(const CString& path);

	void InitializeVTKWindow(void* hWnd);
	void ResizeVTKWindow();

public:
	afx_msg void OnBnClickedButtonLoadImg();
	afx_msg void OnBnClickedBtnMinimize();
	afx_msg void OnBnClickedBtnExit();
	afx_msg void OnBnClickedButtonResult();
	afx_msg void OnBnClickedButtonLoad3dData();
	afx_msg void OnBnClickedButtonReset3dPos();
};
