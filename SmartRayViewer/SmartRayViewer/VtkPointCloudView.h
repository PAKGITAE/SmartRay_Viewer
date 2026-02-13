#pragma once
#include <afxwin.h>

#include <vtkSmartPointer.h>
#include <vector>
#include <memory>

#include "Types.h"

// -----------------------------------------------------------------------------
// CVtkPointCloudView
// - MFC 다이얼로그 안에 VTK 렌더링 윈도우를 붙여서 Point Cloud를 표시하는 뷰어 클래스
// - 입력 데이터는 SR_3DPOINT(x,y,z float 3개) 배열/벡터 기반
// - 성능 최적화:
//   1) VTK 파이프라인은 1회 생성 후 재사용
//   2) vtkPoints 내부 좌표 버퍼(vtkFloatArray)를 직접 WritePointer + memcpy 로 갱신
//   3) Z 스칼라는 최소 1회 루프( min/max 갱신 필요 )
// -----------------------------------------------------------------------------

struct PcFrame;

// ---- Forward declarations (헤더에서 VTK include 최소화) -----------------------
class vtkRenderWindow;
class vtkRenderer;
class vtkActor;
class vtkAxesActor;
class vtkOrientationMarkerWidget;

class vtkPolyData;
class vtkPoints;
class vtkVertexGlyphFilter;
class vtkPolyDataMapper;
class vtkFloatArray;

class vtkTransform;
class vtkTransformPolyDataFilter;

class CVtkPointCloudView
{
public:
    // -----------------------------------------------------------------------------
    // Life-cycle / Window
    // -----------------------------------------------------------------------------
    // Host 윈도우(IDC_VTK_VIEW 등)에 VTK RenderWindow를 attach 하고 파이프라인 준비
    bool Init(HWND hWndHost);

    // Host 사이즈에 맞춰 RenderWindow 리사이즈
    void ResizeToHost();
    void Resize(int w, int h);

    // 강제 렌더 1회 (필요할 때만 호출)
    void Render();

    // -----------------------------------------------------------------------------
    // Data Update
    // -----------------------------------------------------------------------------
    // 외부에서 가장 많이 쓰는 API (vector 입력)
    // 내부적으로 포인터 버전으로 위임됨
    bool UpdatePointCloud(const std::vector<SR_3DPOINT>& pts);

    // -----------------------------------------------------------------------------
    // Camera Presets (버튼 이벤트에서 호출)
    // - Note: ViewTop/ViewFront... 은 bounds 기반이라 "한 번" 정렬용으로 쓰는게 안정적
    // -----------------------------------------------------------------------------
    void ViewTop();     // +Z에서 내려다봄 (XY 평면)
    void ViewFront();   // -Y에서 바라봄 (XZ 평면)
    void ViewSide();    // -X에서 바라봄 (YZ 평면)
    void ViewIso();     // 등각 느낌

    // -----------------------------------------------------------------------------
    // File Load
    // - 현재 구현: ASCII(x y z) 포맷(asc/xyz/txt)을 PcFrame으로 로드
    // - (주의) ply를 지원하려면 cpp에서 분기 추가해야 함
    // -----------------------------------------------------------------------------
    std::shared_ptr<PcFrame> LoadPointCloudToFrame(const CString& path, int camIndex);

private:
    // -----------------------------------------------------------------------------
    // Internal: initialization
    // -----------------------------------------------------------------------------
    void InitializeVTKWindow(HWND hWndHost);

    // VTK PolyData/Points/Glyph/Mapper/Actor를 1회 생성하여 재사용
    void EnsurePipelineReady();

    // -----------------------------------------------------------------------------
    // Internal: fast update core
    // - pts: SR_3DPOINT 포인터
    // - nPts: 포인트 개수
    // -----------------------------------------------------------------------------
    bool UpdatePointCloud(const SR_3DPOINT* pts, size_t nPts);

private:
    // -----------------------------------------------------------------------------
    // Host window handle
    // -----------------------------------------------------------------------------
    HWND m_hHost = nullptr;

    // -----------------------------------------------------------------------------
    // VTK window + renderer + main actor
    // -----------------------------------------------------------------------------
    vtkSmartPointer<vtkRenderWindow> m_vtkRenderWindow;
    vtkSmartPointer<vtkRenderer>     m_vtkRenderer;
    vtkSmartPointer<vtkActor>        m_vtkActor;

    // -----------------------------------------------------------------------------
    // Orientation marker (axes widget)
    // -----------------------------------------------------------------------------
    vtkSmartPointer<vtkAxesActor>               m_axesActor;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_axesWidget;

    // -----------------------------------------------------------------------------
    // Pipeline (reused)
    // -----------------------------------------------------------------------------
    vtkSmartPointer<vtkPolyData>          m_poly;
    vtkSmartPointer<vtkPoints>            m_points;
    vtkSmartPointer<vtkVertexGlyphFilter> m_glyph;
    vtkSmartPointer<vtkPolyDataMapper>    m_mapper;

    // -----------------------------------------------------------------------------
    // Reusable CPU-side buffers for vtkPoints / scalars
    // - m_coords3f: xyz(3-component) 튜플 버퍼
    // - m_scalarsZ: z(1-component) 스칼라 버퍼
    // -----------------------------------------------------------------------------
    vtkSmartPointer<vtkFloatArray> m_coords3f;   // (x,y,z) float[3] * N
    vtkSmartPointer<vtkFloatArray> m_scalarsZ;   // z float[1] * N

    // -----------------------------------------------------------------------------
    // Flags
    // -----------------------------------------------------------------------------
    bool m_pipelineReady = false;  // 파이프라인 생성 완료 여부
    bool m_cameraInitialized = false;  // 최초 1회 카메라 초기화 수행 여부

    vtkSmartPointer<vtkTransform>              m_tf90cw;     // TopView 기준 CW90 transform
    vtkSmartPointer<vtkTransformPolyDataFilter> m_tfFilter;  // polydata transform filter
};
