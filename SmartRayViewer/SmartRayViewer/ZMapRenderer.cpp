#include "pch.h"
#include "ZMapRenderer.h"
#include "vImage.h"          // vImage 선언/정의 포함되는 헤더

#include <fstream>
#include <vector>


cv::Mat CZMapRenderer::ImreadUnicode(const CString& cstrPath, int flags)
{
    std::wstring wpath(cstrPath);
    std::ifstream fin(wpath, std::ios::binary);
    if (!fin) return cv::Mat();

    fin.seekg(0, std::ios::end);
    std::streamoff size = fin.tellg();
    if (size <= 0) return cv::Mat();     // 핵심 가드
    fin.seekg(0, std::ios::beg);

    std::vector<uchar> buf((size_t)size);
    if (!fin.read(reinterpret_cast<char*>(buf.data()), size))
        return cv::Mat();

    return cv::imdecode(buf, flags);
}

bool CZMapRenderer::CopyMatBgr24ToVImage(const cv::Mat& bgr24, vImage& img)
{
    if (bgr24.empty() || bgr24.type() != CV_8UC3) return false;

    if ((int)img.GetWidth() != bgr24.cols ||
        (int)img.GetHeight() != bgr24.rows ||
        img.GetDepth() != 24)
        return false;

    uint8_t* dst = img.GetImagePtr();
    const uint32_t dstPitch = img.GetPitch();
    if (!dst || dstPitch == 0) return false;

    const uint8_t* src = bgr24.data;
    const uint32_t srcPitch = (uint32_t)bgr24.step;
    const uint32_t copyBytes = (uint32_t)bgr24.cols * 3;

    for (int y = 0; y < bgr24.rows; ++y)
    {
        memcpy(dst + (size_t)dstPitch * y,
            src + (size_t)srcPitch * y,
            copyBytes);

        if (dstPitch > copyBytes)
            memset(dst + (size_t)dstPitch * y + copyBytes, 0, dstPitch - copyBytes);
    }
    return true;
}


bool CZMapRenderer::Load(const CString& path)
{
    cv::Mat src = ImreadUnicode(path, cv::IMREAD_UNCHANGED);
    if (src.empty())
        return false;

    // 16-bit gray로 통일 (CV_16UC1)
    if (src.type() == CV_16UC1)
    {
        m_z16 = src;
        return true;
    }
    else if (src.type() == CV_16UC3)
    {
        cv::cvtColor(src, m_z16, cv::COLOR_BGR2GRAY);  // 보통 CV_16UC1
        return !m_z16.empty();
    }
    else if (src.type() == CV_8UC1)
    {
        src.convertTo(m_z16, CV_16U, 257.0);           // 0..255 -> 0..65535 근사
        return !m_z16.empty();
    }
    else if (src.type() == CV_8UC3)
    {
        cv::Mat g8;
        cv::cvtColor(src, g8, cv::COLOR_BGR2GRAY);
        g8.convertTo(m_z16, CV_16U, 257.0);
        return !m_z16.empty();
    }

    // 지원하지 않는 타입
    m_z16.release();
    return false;
}

bool CZMapRenderer::GetDataMinMax(uint16_t& outMin, uint16_t& outMax, uint16_t invalidValue) const
{
    if (m_z16.empty() || m_z16.type() != CV_16UC1)
        return false;

    uint16_t mn = 65535, mx = 0;
    bool any = false;

    for (int y = 0; y < m_z16.rows; ++y)
    {
        const uint16_t* row = m_z16.ptr<uint16_t>(y);
        for (int x = 0; x < m_z16.cols; ++x)
        {
            uint16_t v = row[x];
            if (v == invalidValue) continue;

            if (!any) { mn = mx = v; any = true; }
            else {
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
        }
    }

    if (!any) return false;
    if (mx <= mn) mx = (uint16_t)(mn + 1);

    outMin = mn;
    outMax = mx;
    return true;
}

bool CZMapRenderer::RenderJetTo(
    vImage& dst,
    uint16_t vmin,
    uint16_t vmax,
    uint16_t invalidValue,
    cv::Scalar invalidGray
) const
{
    if (m_z16.empty() || m_z16.type() != CV_16UC1)
        return false;

    if (vmax <= vmin) vmax = (uint16_t)(vmin + 1);

    // valid mask = (z != invalidValue) & (vmin<=z<=vmax)
    cv::Mat maskNZ, maskGE, maskLE, maskValid;
    cv::compare(m_z16, invalidValue, maskNZ, cv::CMP_NE);
    cv::compare(m_z16, vmin, maskGE, cv::CMP_GE);
    cv::compare(m_z16, vmax, maskLE, cv::CMP_LE);
    maskValid = maskNZ & maskGE & maskLE;

    // 16->8 정규화 (vmin~vmax 기준)
    const double scale = 255.0 / (double)(vmax - vmin);
    const double shift = -(double)vmin * scale;

    cv::Mat gray8;
    m_z16.convertTo(gray8, CV_8U, scale, shift);

    // valid 아닌 곳은 0으로 (컬러맵 입력 안정화)
    gray8.setTo(0, ~maskValid);

    // 컬러맵 -> BGR24
    cv::Mat bgr24;
    cv::applyColorMap(gray8, bgr24, cv::COLORMAP_JET);

    // invalid/범위밖 => 회색
    bgr24.setTo(invalidGray, ~maskValid);

    // vImage 버퍼는 dst가 이미 같은 크기/24bit로 init되어 있어야 함
    return CopyMatBgr24ToVImage(bgr24, dst);
}

bool CZMapRenderer::GetStatsInRoi(const CRect& roi, ZRoiStats& out, uint16_t invalidValue) const
{
    out = ZRoiStats{};
    if (m_z16.empty() || m_z16.type() != CV_16UC1) return false;

    // ROI 클램프 (CRect: left,top,right,bottom / right,bottom은 보통 "끝 다음" 좌표로 취급)
    int x0 = (roi.left < 0) ? 0 : roi.left;
    int y0 = (roi.top < 0) ? 0 : roi.top;

    int x1 = (roi.right > m_z16.cols) ? m_z16.cols : roi.right;
    int y1 = (roi.bottom > m_z16.rows) ? m_z16.rows : roi.bottom;

    if (x1 <= x0 || y1 <= y0) return false;

    uint16_t mn = 65535, mx = 0;
    double sum = 0.0;
    int64_t cnt = 0;

    for (int y = y0; y < y1; ++y)
    {
        const uint16_t* row = m_z16.ptr<uint16_t>(y);
        for (int x = x0; x < x1; ++x)
        {
            uint16_t v = row[x];
            if (v == invalidValue) continue;  // invalid 제외 (보통 0)

            if (v < mn) mn = v;
            if (v > mx) mx = v;
            sum += (double)v;
            ++cnt;
        }
    }

    if (cnt == 0) return false;

    out.ok = true;
    out.minv = mn;
    out.maxv = mx;
    out.mean = sum / (double)cnt;
    out.count = cnt;
    return true;
}