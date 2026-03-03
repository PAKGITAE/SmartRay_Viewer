// VtkPointCloudView.cpp
#include "pch.h"
#include "VtkPointCloudView.h"

// VTK AutoInit 반드시 cpp에서!
#define vtkRenderingCore_AUTOINIT 3(vtkRenderingOpenGL2,vtkInteractionStyle,vtkRenderingFreeType)
#define vtkRenderingContext2D_AUTOINIT 1(vtkRenderingContextOpenGL2)
#include <vtkAutoInit.h>

// ---- VTK Core ----
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>

// ---- PolyData ----
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>
#include <vtkLookupTable.h>

// ---- IO ----
#include <vtkPLYReader.h>

// ---- Rendering ----
#include <vtkVertexGlyphFilter.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>

// ---- Orientation ----
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkCaptionActor2D.h>
#include <vtkTextActor.h>

#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

// ---- Camera ----
#include <vtkCamera.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <limits>
#include <cmath>
#include <cstring>   // memcpy

static CString GetLowerExt(const CString& path)
{
    int dot = path.ReverseFind(L'.');
    if (dot < 0) return L"";
    CString ext = path.Mid(dot);
    ext.MakeLower();
    return ext;
}

//-----------------------------------------------------
// 뷰어 방향 설정 관련
static inline bool BoundsValid(const double b[6])
{
    return std::isfinite(b[0]) && std::isfinite(b[1]) &&
        std::isfinite(b[2]) && std::isfinite(b[3]) &&
        std::isfinite(b[4]) && std::isfinite(b[5]) &&
        (b[1] >= b[0]) && (b[3] >= b[2]) && (b[5] >= b[4]);
}

static inline double SafeSize(const double b[6])
{
    double sx = b[1] - b[0];
    double sy = b[3] - b[2];
    double sz = b[5] - b[4];
    double s = std::max({ sx, sy, sz });
    return (s > 1e-9) ? s : 1.0;
}

static void ApplyView(vtkRenderer* ren, vtkRenderWindow* rw,
    double pos[3], double fp[3], double up[3])
{
    vtkCamera* cam = ren->GetActiveCamera();
    if (!cam) return;

    cam->SetFocalPoint(fp);
    cam->SetPosition(pos);
    cam->SetViewUp(up);

    ren->ResetCameraClippingRange();
    rw->Render();
}

// 빠른 3float 파서 (한 줄에서 x y z만 뽑음)
static inline bool Parse3FloatFast(const char* s, float& x, float& y, float& z)
{
    // 앞 공백 스킵
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;

    // 빈 줄/주석 라인 스킵
    if (*s == '\0' || *s == '#') return false;

    char* end = nullptr;

    x = std::strtof(s, &end);
    if (end == s) return false;
    s = end;

    y = std::strtof(s, &end);
    if (end == s) return false;
    s = end;

    z = std::strtof(s, &end);
    if (end == s) return false;

    // NaN/Inf 방어
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;

    return true;
}
//-----------------------------------------------------

// ============================================================================
// CVtkPointCloudView
// ============================================================================
bool CVtkPointCloudView::Init(HWND hWndHost)
{
    if (!hWndHost) return false;
    m_hHost = hWndHost;

    InitializeVTKWindow(hWndHost);
    ResizeToHost();
    Render();
    return true;
}

void CVtkPointCloudView::InitializeVTKWindow(HWND hWndHost)
{
    if (!m_vtkRenderWindow)
        m_vtkRenderWindow = vtkSmartPointer<vtkRenderWindow>::New();

    if (!m_interactor)
        m_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();

    if (!m_style)
        m_style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();

    m_interactor->SetInteractorStyle(m_style);

    m_vtkRenderer = vtkSmartPointer<vtkRenderer>::New();
    m_vtkRenderer->SetBackground(0.1, 0.2, 0.3);

    m_vtkRenderWindow->SetParentId(hWndHost);
    m_vtkRenderWindow->SetInteractor(m_interactor);
    m_vtkRenderWindow->AddRenderer(m_vtkRenderer);

    // Axes
    m_axesActor = vtkSmartPointer<vtkAxesActor>::New();
    m_axesActor->SetTotalLength(1.0, 1.0, 1.0);
    m_axesActor->SetShaftTypeToCylinder();
    m_axesActor->SetCylinderRadius(0.03);

    m_axesActor->GetXAxisCaptionActor2D()->GetTextActor()->SetTextScaleModeToNone();
    m_axesActor->GetYAxisCaptionActor2D()->GetTextActor()->SetTextScaleModeToNone();
    m_axesActor->GetZAxisCaptionActor2D()->GetTextActor()->SetTextScaleModeToNone();

    m_axesWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    m_axesWidget->SetOrientationMarker(m_axesActor);
    m_axesWidget->SetInteractor(m_interactor);
    m_axesWidget->SetViewport(0.85, 0.01, 0.99, 0.15);
    m_axesWidget->SetEnabled(1);
    m_axesWidget->InteractiveOff();

    m_vtkRenderWindow->Render();

    // 파이프라인 1회 생성
    if (!m_pipelineReady)
        EnsurePipelineReady();
}

void CVtkPointCloudView::EnsurePipelineReady()
{
    if (m_pipelineReady) return;
    if (!m_vtkRenderer) return;

    m_poly = vtkSmartPointer<vtkPolyData>::New();
    m_points = vtkSmartPointer<vtkPoints>::New();
    m_points->SetDataTypeToFloat();

    m_coords3f = vtkSmartPointer<vtkFloatArray>::New();
    m_coords3f->SetNumberOfComponents(3);
    m_coords3f->SetName("PointsXYZ");

    m_scalarsZ = vtkSmartPointer<vtkFloatArray>::New();
    m_scalarsZ->SetNumberOfComponents(1);
    m_scalarsZ->SetName("HeightZ");

    m_points->SetData(m_coords3f);
    m_poly->SetPoints(m_points);
    m_poly->GetPointData()->SetScalars(m_scalarsZ);

    m_glyph = vtkSmartPointer<vtkVertexGlyphFilter>::New();
    m_glyph->SetInputData(m_poly);

    // ✅ (추가) TopView 화면 기준 CW90 = RotateZ(-90)
    m_tf90cw = vtkSmartPointer<vtkTransform>::New();
    m_tf90cw->Identity();
    m_tf90cw->RotateZ(-90.0);

    m_tfFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
    m_tfFilter->SetTransform(m_tf90cw);
    m_tfFilter->SetInputConnection(m_glyph->GetOutputPort());

    m_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_mapper->SetInputConnection(m_tfFilter->GetOutputPort());

    vtkNew<vtkLookupTable> lut;
    lut->SetNumberOfTableValues(256);
    lut->SetHueRange(0.667, 0.0);
    lut->Build();
    m_mapper->SetLookupTable(lut);
    m_mapper->ScalarVisibilityOn();

    if (!m_vtkActor) m_vtkActor = vtkSmartPointer<vtkActor>::New();
    m_vtkActor->SetMapper(m_mapper);
    m_vtkActor->GetProperty()->SetRepresentationToPoints();
    m_vtkActor->GetProperty()->SetPointSize(1.0);
    m_vtkActor->GetProperty()->LightingOff();
    m_vtkRenderer->AddActor(m_vtkActor);

    m_pipelineReady = true;
    m_cameraInitialized = false;
}

void CVtkPointCloudView::ResizeToHost()
{
    if (!m_hHost) return;
    CRect rc;
    ::GetClientRect(m_hHost, &rc);
    Resize(rc.Width(), rc.Height());
}

void CVtkPointCloudView::Resize(int w, int h)
{
    if (!m_vtkRenderWindow) return;
    if (w <= 0 || h <= 0) return;
    m_vtkRenderWindow->SetSize(w, h);
}

void CVtkPointCloudView::Render()
{
    if (m_vtkRenderWindow)
        m_vtkRenderWindow->Render();
}

// ============================================================================
// View helpers
// ============================================================================
void CVtkPointCloudView::ViewTop()
{
    if (!m_vtkRenderer || !m_vtkRenderWindow) return;

    double b[6];
    m_vtkRenderer->ComputeVisiblePropBounds(b);
    if (!BoundsValid(b)) return;

    const double cx = (b[0] + b[1]) * 0.5;
    const double cy = (b[2] + b[3]) * 0.5;
    const double cz = (b[4] + b[5]) * 0.5;
    const double size = SafeSize(b);
    const double dist = size * 2.5;

    double fp[3] = { cx, cy, cz };
    double pos[3] = { cx, cy, cz + dist };
    double up[3] = { 0, 1, 0 };

    ApplyView(m_vtkRenderer, m_vtkRenderWindow, pos, fp, up);
}

void CVtkPointCloudView::ViewFront()
{
    if (!m_vtkRenderer || !m_vtkRenderWindow) return;

    double b[6];
    m_vtkRenderer->ComputeVisiblePropBounds(b);
    if (!BoundsValid(b)) return;

    const double cx = (b[0] + b[1]) * 0.5;
    const double cy = (b[2] + b[3]) * 0.5;
    const double cz = (b[4] + b[5]) * 0.5;
    const double size = SafeSize(b);
    const double dist = size * 2.5;

    double fp[3] = { cx, cy, cz };
    double pos[3] = { cx, cy - dist, cz };
    double up[3] = { 0, 0, 1 };

    ApplyView(m_vtkRenderer, m_vtkRenderWindow, pos, fp, up);
}

void CVtkPointCloudView::ViewSide()
{
    if (!m_vtkRenderer || !m_vtkRenderWindow) return;

    double b[6];
    m_vtkRenderer->ComputeVisiblePropBounds(b);
    if (!BoundsValid(b)) return;

    const double cx = (b[0] + b[1]) * 0.5;
    const double cy = (b[2] + b[3]) * 0.5;
    const double cz = (b[4] + b[5]) * 0.5;
    const double size = SafeSize(b);
    const double dist = size * 2.5;

    double fp[3] = { cx, cy, cz };
    double pos[3] = { cx - dist, cy, cz };
    double up[3] = { 0, 0, 1 };

    ApplyView(m_vtkRenderer, m_vtkRenderWindow, pos, fp, up);
}

void CVtkPointCloudView::ViewIso()
{
    if (!m_vtkRenderer || !m_vtkRenderWindow) return;

    double b[6];
    m_vtkRenderer->ComputeVisiblePropBounds(b);
    if (!BoundsValid(b)) return;

    const double cx = (b[0] + b[1]) * 0.5;
    const double cy = (b[2] + b[3]) * 0.5;
    const double cz = (b[4] + b[5]) * 0.5;
    const double size = SafeSize(b);
    const double dist = size * 1.3;

    double fp[3] = { cx, cy, cz };
    double pos[3] = { cx + dist, cy + dist, cz + dist };
    double up[3] = { 0, 0, 1 };

    ApplyView(m_vtkRenderer, m_vtkRenderWindow, pos, fp, up);
}


// ============================================================================
// File load -> PcFrame (+ optional render)
// ============================================================================
// NOTE:
// 현재 ASCII(x y z) 텍스트 포맷만 지원
// ply(binary)는 여기서 로드되지 않음
std::shared_ptr<PcFrame> CVtkPointCloudView::LoadPointCloudToFrame(
    const CString& path,
    int camIndex
)
{
    CStringA aPath(path);
    std::ifstream ifs(aPath.GetString(), std::ios::binary);
    if (!ifs.is_open()) return nullptr;

    auto frame = std::make_shared<PcFrame>();
    frame->camIndex = camIndex;

    // reserve(대충 추정)로 재할당 줄이기
    // frame->points.reserve(estPts);

    std::string line;
    line.reserve(256);

    float x, y, z;
    while (std::getline(ifs, line))
    {
        if (!Parse3FloatFast(line.c_str(), x, y, z))
            continue;

        SR_3DPOINT p{};
        p.x = x; p.y = y; p.z = z;
        frame->points.push_back(p);
    }

    if (frame->points.empty()) return nullptr;

    frame->numPoints = (uint32_t)frame->points.size();
    frame->numProfiles = 0;
    return frame;

}

// ============================================================================
// Fast update
// ============================================================================

bool CVtkPointCloudView::UpdatePointCloud(const std::vector<SR_3DPOINT>& pts)
{
    return UpdatePointCloud(pts.data(), pts.size());
}

bool CVtkPointCloudView::UpdatePointCloud(const SR_3DPOINT* pts, size_t nPts)
{
    if (!m_vtkRenderer || !m_vtkRenderWindow) return false;

    EnsurePipelineReady();
    if (!m_pipelineReady) return false;

    if (!pts || nPts == 0) return false;

    const vtkIdType N = (vtkIdType)nPts;

    // ✅ SR_3DPOINT가 float3인지 강제
    static_assert(sizeof(SR_3DPOINT) == sizeof(float) * 3,
        "SR_3DPOINT must be exactly 3 floats (x,y,z)");

    // ------------------------------------------------------------
    // 1) 좌표: vtkPoints 내부 버퍼에 한 번에 복사 (SetPoint 루프 제거)
    // ------------------------------------------------------------
    m_coords3f->SetNumberOfTuples(N);

    float* xyz = static_cast<float*>(m_coords3f->WritePointer(0, 3 * N));
    std::memcpy(xyz, pts, (size_t)N * sizeof(SR_3DPOINT));

    // vtkPoints는 m_coords3f를 참조 중이라, 수정 알림만
    m_points->Modified();

    // ------------------------------------------------------------
    // 2) 스칼라(Z): 1번 루프는 필요 (zmin/zmax도 여기서)
    // ------------------------------------------------------------
    m_scalarsZ->SetNumberOfTuples(N);
    float* zbuf = static_cast<float*>(m_scalarsZ->WritePointer(0, N));

    float zmin = FLT_MAX;
    float zmax = -FLT_MAX;

    for (vtkIdType i = 0; i < N; ++i)
    {
        const float z = pts[i].z;
        zbuf[i] = z;
        if (z < zmin) zmin = z;
        if (z > zmax) zmax = z;
    }

    if (zmin == zmax) zmax = zmin + 1.0f;

    m_scalarsZ->Modified();
    m_poly->Modified();

    // glyph/mapper 갱신
    m_glyph->Modified();
    // m_glyph->Update();  // ✅ 굳이 매번 Update() 강제 안 해도 보통 Render 때 갱신됨(필요하면 유지)

    m_mapper->ScalarVisibilityOn();
    m_mapper->SetScalarRange((double)zmin, (double)zmax);

    if (!m_cameraInitialized)
    {
        m_vtkRenderer->ResetCamera();
        m_cameraInitialized = true;
    }

    if (!m_cameraInitialized)
    {
        m_vtkRenderer->ResetCamera();
        m_cameraInitialized = true;

        // 초기 1회만 기본 뷰 세팅
        ViewIso();
        UpdateOrbitFromVisibleBounds();
    }
    else
    {
        // AutoRotate OFF일 때만 ViewTop 유지(원할 때)
        //if (!m_autoRotate)
        //    ViewFront();
        //else
        ViewIso();
        UpdateOrbitFromVisibleBounds(); // 데이터 범위가 바뀌면 궤도 중심만 갱신
    }

    return true;
}

void CVtkPointCloudView::StartAutoRotate(double degPerSec)
{
    m_autoRotate = true;
    m_rotateDegPerSec = (degPerSec > 0.0) ? degPerSec : 30.0;

    // 첫 시작 시 현재 보이는 bounds로 궤도 기준 잡기
    UpdateOrbitFromVisibleBounds();
}

void CVtkPointCloudView::StopAutoRotate()
{
    m_autoRotate = false;
}

void CVtkPointCloudView::ToggleAutoRotate(double degPerSec)
{
    if (m_autoRotate) StopAutoRotate();
    else StartAutoRotate(degPerSec);
}

void CVtkPointCloudView::UpdateOrbitFromVisibleBounds()
{
    if (!m_vtkRenderer) { m_orbitReady = false; return; }

    double b[6];
    m_vtkRenderer->ComputeVisiblePropBounds(b);
    if (!BoundsValid(b)) { m_orbitReady = false; return; }

    const double cx = (b[0] + b[1]) * 0.5;
    const double cy = (b[2] + b[3]) * 0.5;
    const double cz = (b[4] + b[5]) * 0.5;

    m_orbitFocal[0] = cx;
    m_orbitFocal[1] = cy;
    m_orbitFocal[2] = cz;

    const double size = SafeSize(b);
    m_orbitRadius = size * 2.5; // ViewTop에서 dist=2.5 썼던 감각과 동일

    m_orbitReady = true;
}

void CVtkPointCloudView::TickAutoRotate(double dtSec)
{
    if (!m_autoRotate) return;
    if (!m_vtkRenderer || !m_vtkRenderWindow) return;

    vtkCamera* cam = m_vtkRenderer->GetActiveCamera();
    if (!cam) return;

    // 데이터가 바뀌었거나 첫 회전이면 focal/radius 보정
    if (!m_orbitReady)
        UpdateOrbitFromVisibleBounds();

    // focal을 bounds 중심으로 고정
    cam->SetFocalPoint(m_orbitFocal);

    const double dAngle = m_rotateDegPerSec * dtSec; // deg
    cam->Azimuth(dAngle); // 수평 회전

    if (!m_useYawOnly)
    {
        // 아주 약하게 상하 흔들림(선택)
        cam->Elevation(0.15 * dAngle);
    }

    m_vtkRenderer->ResetCameraClippingRange();
    m_vtkRenderWindow->Render();
}