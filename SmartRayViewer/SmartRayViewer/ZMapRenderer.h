#pragma once
#include <afxstr.h>          // CString
#include <opencv2/opencv.hpp>
#include <cstdint>

struct ZRoiStats
{
    bool     ok = false;
    uint16_t minv = 0;
    uint16_t maxv = 0;
    double   mean = 0.0;
    int64_t  count = 0;   // 유효 픽셀 수
};


class vImage;

class CZMapRenderer
{
public:
    CZMapRenderer() = default;

    // 이미지 로드 (유니코드 경로 안전)
    bool Load(const CString& path);

    bool IsLoaded() const { return !m_z16.empty(); }

    int Width()  const { return m_z16.cols; }
    int Height() const { return m_z16.rows; }

    // 0 제외 min/max
    bool GetDataMinMax(uint16_t& outMin, uint16_t& outMax, uint16_t invalidValue = 0) const;

    // JET 컬러맵 렌더링해서 vImage에 복사
    // - vmin/vmax: 표시 범위
    // - invalidValue: invalid 판정 값(기본 0)
    // - invalidGray: invalid/범위밖 색(BGR)
    bool RenderJetTo(
        vImage& dst,
        uint16_t vmin,
        uint16_t vmax,
        uint16_t invalidValue = 0,
        cv::Scalar invalidGray = cv::Scalar(80, 80, 80)
    ) const;


    bool GetStatsInRoi(const CRect& roi, ZRoiStats& out, uint16_t invalidValue = 0) const;


private:
    static cv::Mat ImreadUnicode(const CString& cstrPath, int flags);       //Imread 유니코드용
    static bool CopyMatBgr24ToVImage(const cv::Mat& bgr24, vImage& img);    //cv::Mat -> vImage 변환

private:
    cv::Mat m_z16; // CV_16UC1
};
