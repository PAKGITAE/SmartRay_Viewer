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

// ---- Camera ----
#include <vtkCamera.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#include <cmath> // std::isfinite, std::round
#include <limits>

static inline bool Finite(double v) { return std::isfinite(v); }


// ============================================================================
// Helpers (원본 그대로)
// ============================================================================
static CString GetLowerExt(const CString& path)
{
    int dot = path.ReverseFind(L'.');
    if (dot < 0) return L"";
    CString ext = path.Mid(dot);
    ext.MakeLower();
    return ext;
}

static void ApplyHeightColorByZ(vtkPolyData* poly, vtkPolyDataMapper* mapper)
{
    if (!poly || !mapper) return;
    vtkPoints* pts = poly->GetPoints();
    if (!pts) return;

    vtkIdType n = pts->GetNumberOfPoints();
    if (n <= 0) return;

    // ---- Z 모으기 (finite만) ----
    std::vector<double> zs;
    zs.reserve((size_t)n);

    double p[3];
    for (vtkIdType i = 0; i < n; ++i) {
        pts->GetPoint(i, p);
        if (std::isfinite(p[2])) zs.push_back(p[2]);
    }
    if (zs.empty()) return;

    // ---- 퍼센타일 범위(예: 2% ~ 98%) ----
    auto percentile = [&](double q)->double {
        // q: 0~1
        size_t k = (size_t)std::clamp(q, 0.0, 1.0) * (zs.size() - 1);
        std::nth_element(zs.begin(), zs.begin() + k, zs.end());
        return zs[k];
        };

    const double zLow = percentile(0.02);  // 2%
    const double zHigh = percentile(0.98);  // 98%
    double zmin = zLow;
    double zmax = zHigh;
    if (zmin == zmax) zmax = zmin + 1.0;

    // ---- 스칼라 넣기 ----
    vtkNew<vtkFloatArray> scalars;
    scalars->SetName("HeightZ");
    scalars->SetNumberOfComponents(1);
    scalars->SetNumberOfTuples(n);

    for (vtkIdType i = 0; i < n; ++i) {
        pts->GetPoint(i, p);
        float v = std::isfinite(p[2]) ? (float)p[2] : (float)zmin;
        scalars->SetValue(i, v);
    }
    poly->GetPointData()->SetScalars(scalars);

    // ---- LUT: 낮음=파랑, 높음=빨강 ----
    vtkNew<vtkLookupTable> lut;
    lut->SetNumberOfTableValues(256);
    lut->SetHueRange(0.667, 0.0);          // Blue -> Red
    //lut->SetRampToLinear();              // 기본 linear (원하면 명시)
    lut->Build();

    mapper->SetLookupTable(lut);

    // ✅ 여기서 “데이터 기준 재맵핑”이 일어남
    mapper->SetScalarRange(zmin, zmax);

    mapper->ScalarVisibilityOn();
    mapper->SetColorModeToMapScalars();
    mapper->SetScalarModeToUsePointData();
}

//static void ApplyHeightColorByZ(vtkPolyData* poly, vtkPolyDataMapper* mapper)
//{
//    if (!poly || !mapper) return;
//
//    vtkPoints* pts = poly->GetPoints();
//    if (!pts) return;
//
//    vtkIdType n = pts->GetNumberOfPoints();
//    if (n <= 0) return;
//
//    // Z min/max 계산
//    double p[3];
//    pts->GetPoint(0, p);
//    double zmin = p[2], zmax = p[2];
//
//    for (vtkIdType i = 1; i < n; ++i)
//    {
//        pts->GetPoint(i, p);
//        zmin = std::min(zmin, p[2]);
//        zmax = std::max(zmax, p[2]);
//    }
//    if (zmin == zmax) zmax = zmin + 1.0; // 안전장치
//
//    // 각 포인트의 Z를 스칼라로 저장
//    vtkNew<vtkFloatArray> scalars;
//    scalars->SetName("HeightZ");
//    scalars->SetNumberOfComponents(1);
//    scalars->SetNumberOfTuples(n);
//
//    for (vtkIdType i = 0; i < n; ++i)
//    {
//        pts->GetPoint(i, p);
//        scalars->SetValue(i, static_cast<float>(p[2]));
//    }
//
//    poly->GetPointData()->SetScalars(scalars);
//
//    // 컬러맵 (LUT)
//    vtkNew<vtkLookupTable> lut;
//    lut->SetNumberOfTableValues(256);
//
//    // ✅ 낮음(Blue) -> 높음(Red)
//    lut->SetHueRange(0.667, 0.0);
//    lut->SetSaturationRange(1.0, 1.0);
//    lut->SetValueRange(1.0, 1.0);
//
//    lut->Build();
//
//    mapper->SetLookupTable(lut);
//    mapper->SetScalarRange(zmin, zmax);
//
//    mapper->ScalarVisibilityOn();
//    mapper->SetColorModeToMapScalars();
//    mapper->SetScalarModeToUsePointData();
//}


enum class GridRot
{
    None,
    Rot90CW,
    Rot180,
    Rot270CW
};

static bool MapPixel(int x, int y, int W, int H, GridRot rot, int& ox, int& oy)
{
    // 입력 x,y는 0..W-1 / 0..H-1
    switch (rot)
    {
    default:
    case GridRot::None:
        ox = x; oy = y;
        break;

    case GridRot::Rot90CW:
        // (x,y) -> (H-1-y, x)
        ox = (H - 1) - y;
        oy = x;
        break;

    case GridRot::Rot180:
        ox = (W - 1) - x;
        oy = (H - 1) - y;
        break;

    case GridRot::Rot270CW:
        // (x,y) -> (y, W-1-x)
        ox = y;
        oy = (W - 1) - x;
        break;
    }

    // 범위 체크
    if (ox < 0 || ox >= W || oy < 0 || oy >= H)
        return false;

    return true;
}

//-----------------------------------------------------
//뷰어 방향 설정 관련
static inline bool BoundsValid(const double b[6])
{
    // bounds가 비정상(아무것도 없거나 NaN)일 때 방어
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
    return (s > 1e-9) ? s : 1.0; // 너무 작으면 1로
}

static void ApplyView(vtkRenderer* ren, vtkRenderWindow* rw,
    double pos[3], double fp[3], double up[3])
{
    vtkCamera* cam = ren->GetActiveCamera();
    if (!cam) return;

    cam->SetFocalPoint(fp);
    cam->SetPosition(pos);
    cam->SetViewUp(up);

    // 방향 지정 후, 줌/클리핑만 안정적으로 맞추기
    ren->ResetCameraClippingRange();
    rw->Render();
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

    // Interactor + Style
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetInteractorStyle(
        vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New()
    );

    // Renderer
    m_vtkRenderer = vtkSmartPointer<vtkRenderer>::New();
    m_vtkRenderer->SetBackground(0.1, 0.2, 0.3);

    // RenderWindow attach
    m_vtkRenderWindow->SetParentId(hWndHost);
    m_vtkRenderWindow->SetInteractor(interactor);
    m_vtkRenderWindow->AddRenderer(m_vtkRenderer);

    // ===============================
    // 방향 표시용 Axes (X/Y/Z)
    // ===============================
    m_axesActor = vtkSmartPointer<vtkAxesActor>::New();
    m_axesActor->SetTotalLength(1.0, 1.0, 1.0);
    m_axesActor->SetShaftTypeToCylinder();
    m_axesActor->SetCylinderRadius(0.03);

    // 텍스트 스케일 고정 (원본 유지)
    m_axesActor->GetXAxisCaptionActor2D()->GetTextActor()->SetTextScaleModeToNone();
    m_axesActor->GetYAxisCaptionActor2D()->GetTextActor()->SetTextScaleModeToNone();
    m_axesActor->GetZAxisCaptionActor2D()->GetTextActor()->SetTextScaleModeToNone();

    // ===============================
    // Orientation Marker Widget
    // ===============================
    m_axesWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    m_axesWidget->SetOrientationMarker(m_axesActor);
    m_axesWidget->SetInteractor(interactor);

    // 우하단 위치 (원본 유지)
    m_axesWidget->SetViewport(0.85, 0.01, 0.99, 0.15);
    m_axesWidget->SetEnabled(1);
    m_axesWidget->InteractiveOff();

    m_vtkRenderWindow->Render();
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
    if (w <= 0 || h <= 0) return;
    m_vtkRenderWindow->SetSize(w, h);
}

void CVtkPointCloudView::Render()
{
    m_vtkRenderWindow->Render();
}

bool CVtkPointCloudView::LoadPointCloud(const CString& path)
{
    if (!m_vtkRenderer)
        return false;

    // ✅ 이전 데이터 초기화
    m_pcX.clear();
    m_pcY.clear();
    m_pcZ.clear();

    // grid도 size는 유지하되 값만 초기화
    if (m_gridW > 0 && m_gridH > 0)
        m_zGrid.assign((size_t)m_gridW * (size_t)m_gridH,
            std::numeric_limits<double>::quiet_NaN());


    CString ext = GetLowerExt(path);

    vtkSmartPointer<vtkPolyData> poly = vtkSmartPointer<vtkPolyData>::New();

    if (ext == L".ply")
    {
        CStringA aPath(path);
        vtkNew<vtkPLYReader> reader;
        reader->SetFileName(aPath.GetString());
        reader->Update();
        poly->ShallowCopy(reader->GetOutput());
    }
    else if (ext == L".asc" || ext == L".xyz" || ext == L".txt")
    {
        CStringA aPath(path);
        std::ifstream ifs(aPath.GetString());
        if (!ifs.is_open())
            return false;

        vtkNew<vtkPoints> pts;

        std::string line;
        double x, y, z;

        while (std::getline(ifs, line))
        {
            if (line.empty())
                continue;

            // 주석/헤더 라인 스킵
            if (!line.empty() && (line[0] == '#'))
                continue;

            std::istringstream iss(line);
            if (!(iss >> x >> y >> z))
                continue;

            pts->InsertNextPoint(x, y, z);

            // ✅ ROI 통계용 저장
            m_pcX.push_back(x);
            m_pcY.push_back(y);
            m_pcZ.push_back(z);
        }

        poly->SetPoints(pts);
    }
    else
    {
        return false; // 지원하지 않는 확장자
    }

    const vtkIdType nPts = poly->GetNumberOfPoints();
    if (nPts <= 0)
        return false;

    if (!m_pcX.empty())
    {
        double xmin = 1e100, xmax = -1e100;
        double ymin = 1e100, ymax = -1e100;

        int nearIntX = 0;
        int nearIntY = 0;

        for (size_t i = 0; i < m_pcX.size(); ++i)
        {
            double x = m_pcX[i];
            double y = m_pcY[i];

            if (x < xmin) xmin = x;
            if (x > xmax) xmax = x;
            if (y < ymin) ymin = y;
            if (y > ymax) ymax = y;

            double rx = fabs(x - floor(x + 0.5));
            double ry = fabs(y - floor(y + 0.5));

            if (rx < 1e-3) nearIntX++;
            if (ry < 1e-3) nearIntY++;
        }

        wchar_t buf[256];
        swprintf_s(buf,
            L"[DEBUG] X:[%.3f ~ %.3f]  Y:[%.3f ~ %.3f]  nearIntX=%.1f%%  nearIntY=%.1f%%\n",
            xmin, xmax, ymin, ymax,
            100.0 * nearIntX / m_pcX.size(),
            100.0 * nearIntY / m_pcY.size());

        OutputDebugString(buf);
    }

    // point-only 포함 모든 케이스에서 verts 생성
    vtkNew<vtkVertexGlyphFilter> glyph;
    glyph->SetInputData(poly);
    glyph->Update();

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(glyph->GetOutputPort());

    ApplyHeightColorByZ(poly, mapper);

    // ✅ grid size가 이미 설정되어 있으면 ZGrid 생성
    if (m_gridW > 0 && m_gridH > 0 && !m_pcZ.empty())
        BuildZGridFromPoints();

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    // 점 클라우드 설정
    actor->GetProperty()->SetRepresentationToPoints();
    actor->GetProperty()->SetPointSize(0.5);
    actor->GetProperty()->LightingOff();

    // 기존 Actor 제거 후 교체
    if (m_vtkActor)
        m_vtkRenderer->RemoveActor(m_vtkActor);

    m_vtkActor = actor;
    m_vtkRenderer->AddActor(m_vtkActor);

    m_vtkRenderer->ResetCamera();
    m_vtkRenderWindow->Render();

    // 디버그 출력 (원하면 유지)
    auto* scal = poly->GetPointData()->GetScalars();
    OutputDebugString(scal ? L"[Cloud] has scalars(color)\n" : L"[Cloud] no scalars\n");

    wchar_t buf[256];
    swprintf_s(buf, L"[Cloud] ext=%s points=%lld cells=%lld\n",
        ext.GetString(), (long long)poly->GetNumberOfPoints(), (long long)poly->GetNumberOfCells());
    OutputDebugString(buf);

    return true;
}

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
    double pos[3] = { cx, cy, cz + dist };   // +Z 위에서 내려다봄
    double up[3] = { 0, 1, 0 };             // 화면 위쪽 = +Y

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
    double pos[3] = { cx, cy - dist, cz };   // ✅ 정면 (-Y에서 바라봄)
    double up[3] = { 0, 0, 1 };             // 화면 위쪽 = +Z

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
    double pos[3] = { cx - dist, cy, cz };   // ✅ 왼쪽 사이드뷰 (-X에서 바라봄)
    double up[3] = { 0, 0, 1 };             // 화면 위쪽 = +Z

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

    // 등각 느낌: x,y,z 방향으로 동일 거리에서 바라보기
    const double dist = size * 2.8;

    double fp[3] = { cx, cy, cz };
    double pos[3] = { cx + dist, cy + dist, cz + dist };
    double up[3] = { 0, 0, 1 }; // 보통 Z-up 유지

    ApplyView(m_vtkRenderer, m_vtkRenderWindow, pos, fp, up);
}


//----------------------------------------------------
//Zmap ROI영역에서 Z값 구하는거
//미완성(이미지 사이즈와 데이터 사이즈가 안맞는 문제가 있음)

void CVtkPointCloudView::SetGridSize(int w, int h)
{
    m_gridW = w;
    m_gridH = h;

    if (m_gridW > 0 && m_gridH > 0)
    {
        m_zGrid.assign((size_t)m_gridW * (size_t)m_gridH,
            std::numeric_limits<double>::quiet_NaN());
        // 이미 포인트가 로드되어 있으면 바로 그리드 갱신
        if (!m_pcZ.empty())
            BuildZGridFromPoints();
    }
}

void CVtkPointCloudView::BuildZGridFromPoints()
{
    if (m_gridW <= 0 || m_gridH <= 0) return;
    if (m_pcX.empty() || m_pcY.empty() || m_pcZ.empty()) return;

    m_zGrid.assign((size_t)m_gridW * (size_t)m_gridH,
        std::numeric_limits<double>::quiet_NaN());

    // x/y 범위 계산
    double xmin = m_pcX[0], xmax = m_pcX[0];
    double ymin = m_pcY[0], ymax = m_pcY[0];

    for (size_t i = 1; i < m_pcX.size(); ++i)
    {
        double vx = m_pcX[i];
        double vy = m_pcY[i];

        xmin = (vx < xmin) ? vx : xmin;
        xmax = (vx > xmax) ? vx : xmax;
        ymin = (vy < ymin) ? vy : ymin;
        ymax = (vy > ymax) ? vy : ymax;
    }

    // ✅ 픽셀형 데이터인지 감지
    const bool looksLikePixel =
        xmin >= -0.5 && ymin >= -0.5 &&
        xmax <= (double)(m_gridW - 1) + 0.5 &&
        ymax <= (double)(m_gridH - 1) + 0.5;

    const double dx = (xmax - xmin);
    const double dy = (ymax - ymin);

    for (size_t i = 0; i < m_pcZ.size(); ++i)
    {
        const double x = m_pcX[i];
        const double y = m_pcY[i];
        const double z = m_pcZ[i];

        if (!Finite(z)) continue;

        int ix = 0;
        int iy = 0;

        if (looksLikePixel)
        {
            // x/y가 이미 픽셀 인덱스 의미
            ix = (int)std::lround(x);

            int rawY = (int)std::lround(y);        // 3D: 좌하단이 0
            iy = (m_gridH - 1) - rawY;             // ✅ ZMap: 좌상단이 0 으로 맞춤
        }
        else
        {
            // 범위를 0~W-1 / 0~H-1 로 정규화
            if (dx == 0.0 || dy == 0.0) continue;

            const double nx = (x - xmin) / dx; // 0..1
            const double ny = (y - ymin) / dy; // 0..1

            ix = (int)std::lround(nx * (m_gridW - 1));

            int rawIy = (int)std::lround(ny * (m_gridH - 1));
            iy = (m_gridH - 1) - rawIy;             // ✅ y flip
        }

        if (ix < 0 || ix >= m_gridW || iy < 0 || iy >= m_gridH)
            continue;

        const size_t idx = (size_t)iy * (size_t)m_gridW + (size_t)ix;
        m_zGrid[idx] = z;
    }
}

bool CVtkPointCloudView::GetHeightStatsInRoiPixel(const CRect& roiPx, HeightStats& out) const
{
    out = HeightStats{};

    if (m_gridW <= 0 || m_gridH <= 0) return false;
    if (m_zGrid.empty()) return false;

    // ROI 정리(클램프)
    int l = roiPx.left;
    int t = roiPx.top;
    int r = roiPx.right;
    int b = roiPx.bottom;

    // swap 방지
    if (l > r) { int tmp = l; l = r; r = tmp; }
    if (t > b) { int tmp = t; t = b; b = tmp; }

    // clamp
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > m_gridW) r = m_gridW;
    if (b > m_gridH) b = m_gridH;

    if (l >= r || t >= b) return false;

    bool first = true;
    double mn = 0.0, mx = 0.0, sum = 0.0;
    long long cnt = 0;

    GridRot rot = GridRot::None; // 너가 화면 맞추려고 270 돌렸다면 여기 맞춰 설정

    for (int y = t; y < b; ++y)
    {
        for (int x = l; x < r; ++x)
        {
            int xx = 0, yy = 0;
            if (!MapPixel(x, y, m_gridW, m_gridH, rot, xx, yy))
                continue;

            const double z = m_zGrid[(size_t)yy * (size_t)m_gridW + (size_t)xx];
            if (!Finite(z)) continue;

            if (first) { mn = mx = z; first = false; }
            else
            {
                mn = (z < mn) ? z : mn;
                mx = (z > mx) ? z : mx;
            }

            sum += z;
            ++cnt;
        }
    }

    if (cnt == 0) return false;

    out.valid = true;
    out.count = cnt;
    out.minv = mn;
    out.maxv = mx;
    out.mean = sum / (double)cnt;
    return true;
}

bool CVtkPointCloudView::UpdatePointCloudXYZ(const std::vector<float>& xyz)
{
    if (!m_vtkRenderer) return false;
    if (xyz.empty() || (xyz.size() % 3) != 0) return false;

    // ✅ ROI 통계용 초기화
    m_pcX.clear();
    m_pcY.clear();
    m_pcZ.clear();

    if (m_gridW > 0 && m_gridH > 0)
        m_zGrid.assign((size_t)m_gridW * (size_t)m_gridH,
            std::numeric_limits<double>::quiet_NaN());

    vtkSmartPointer<vtkPolyData> poly = vtkSmartPointer<vtkPolyData>::New();
    vtkNew<vtkPoints> pts;
    pts->SetDataTypeToFloat();

    const vtkIdType n = (vtkIdType)(xyz.size() / 3);
    pts->SetNumberOfPoints(n);

    for (vtkIdType i = 0; i < n; ++i)
    {
        const float x = xyz[i * 3 + 0];
        const float y = xyz[i * 3 + 1];
        const float z = xyz[i * 3 + 2];

        pts->SetPoint(i, x, y, z);

        m_pcX.push_back((double)x);
        m_pcY.push_back((double)y);
        m_pcZ.push_back((double)z);
    }

    poly->SetPoints(pts);

    vtkNew<vtkVertexGlyphFilter> glyph;
    glyph->SetInputData(poly);
    glyph->Update();

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(glyph->GetOutputPort());

    ApplyHeightColorByZ(poly, mapper);

    if (m_gridW > 0 && m_gridH > 0 && !m_pcZ.empty())
        BuildZGridFromPoints();

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetRepresentationToPoints();
    actor->GetProperty()->SetPointSize(1.0);
    actor->GetProperty()->LightingOff();

    if (m_vtkActor)
        m_vtkRenderer->RemoveActor(m_vtkActor);

    m_vtkActor = actor;
    m_vtkRenderer->AddActor(m_vtkActor);

    m_vtkRenderer->ResetCamera();
    m_vtkRenderer->ResetCameraClippingRange();

    m_vtkRenderWindow->Render();

    return true;
}
