#pragma once
#include <afxwin.h>
#include <vtkSmartPointer.h>

#include <vector>

// forward declarations
class vtkRenderWindow;
class vtkRenderer;
class vtkActor;
class vtkAxesActor;
class vtkOrientationMarkerWidget;
class vtkCamera;

struct HeightStats
{
    bool valid = false;
    double mean = 0.0;
    double minv = 0.0;
    double maxv = 0.0;
    long long count = 0;
};

class CVtkPointCloudView
{
public:
    // 초기 설정
    bool Init(HWND hWndHost);
    void ResizeToHost();
    void Resize(int w, int h);

    // 그리기 관련
    void Render();
    bool UpdatePointCloudXYZ(const std::vector<float>& xyz);
    void ViewTop();    // +Z에서 내려다봄 (XY 평면)
    void ViewFront();  // +Y에서 바라봄 (XZ 평면)
    void ViewSide();   // +X에서 바라봄 (YZ 평면)
    void ViewIso();    // 등각(Isometric) 느낌

    // 파일 로드
    bool LoadPointCloud(const CString& path);
    void SetGridSize(int w, int h);     // ZMap의 width/height를 뷰어에 알려줌 (ROI 매핑용)

    // 두께값 연산 관련
    bool GetHeightStatsInRoiPixel(const CRect& roiPx, HeightStats& out) const;  // ✅ ROI(픽셀)로 포인트클라우드 Z 통계 계산

private:
    void InitializeVTKWindow(HWND hWndHost);

    // 포인트클라우드에서 ZGrid 생성
    void BuildZGridFromPoints();

private:
    HWND m_hHost = nullptr;

    vtkSmartPointer<vtkRenderWindow> m_vtkRenderWindow;

    vtkSmartPointer<vtkRenderer> m_vtkRenderer;
    vtkSmartPointer<vtkActor>    m_vtkActor;

    vtkSmartPointer<vtkAxesActor> m_axesActor;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_axesWidget;

    bool m_hasHomeCamera = false;


    // ✅ 로드된 포인트를 저장(ROI 통계용)
    std::vector<double> m_pcX;
    std::vector<double> m_pcY;
    std::vector<double> m_pcZ;

    // ✅ ZMap 크기(픽셀)
    int m_gridW = 0;
    int m_gridH = 0;

    // ✅ 픽셀 그리드의 Z값(없으면 NaN)
    std::vector<double> m_zGrid; // size = m_gridW*m_gridH

};
