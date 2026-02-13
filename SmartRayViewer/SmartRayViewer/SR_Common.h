 #pragma once
#include <string.h>

#define PI 3.141592653589793
#include "sr_api_types.h"

#ifdef __CUDACC__
    // NVCC(CUDA 컴파일러)가 빌드할 때
#define SR_CUDA __host__ __device__
#else
    // 일반 C++ 컴파일러가 빌드할 때 (키워드를 지워버림)
#define SR_CUDA
#endif

enum class SR_Result : int {
    Success = 0,
    Error_InvalidParameter = -1,    // 입력값이 NULL이거나 이상함
    Error_BufferTooSmall = -2,      // 사용자가 준 메모리가 부족함
    Error_FileNotFound = -3,        // 파일이 없음
    Error_SensorNotConnected = -4,
    Error_InternalException = -99   // 알 수 없는 내부 오류 (try-catch)
};

struct Transform_Mat {
    double m[4][4];
    SR_CUDA
    Transform_Mat()
    {
        m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
        m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
        m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
        m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;
    }
    Transform_Mat& operator=(const Transform_Mat& src)
    {
        memcpy(m, src.m, sizeof(double) * 16);
        return *this;
    }
};

#define API_MAX_NUMBER_OF_SPHERE_RESULTS 100
typedef struct
{
    int32_t NumberOfFoundSpheres;                                           /**< number of spheres found in the point cloud. */
    int32_t NumberOfPointsPerSphere[API_MAX_NUMBER_OF_SPHERE_RESULTS];      /**< number of scan points per sphere. */
    double  RMSPerSphere[API_MAX_NUMBER_OF_SPHERE_RESULTS];                 /**< root mean square of deviations per sphere. */
    double  RadiusPerSphere[API_MAX_NUMBER_OF_SPHERE_RESULTS];              /**< fitted radius per sphere. */
    double  CenterXPerSphere[API_MAX_NUMBER_OF_SPHERE_RESULTS];             /**< center of fitted sphere of transformed (SensorRotZ+Y) scan points  per sphere. */
    double  CenterYPerSphere[API_MAX_NUMBER_OF_SPHERE_RESULTS];             /**< center of fitted sphere of transformed (SensorRotZ+Y) scan points  per sphere. */
    double  CenterZPerSphere[API_MAX_NUMBER_OF_SPHERE_RESULTS];             /**< center of fitted sphere of transformed (SensorRotZ+Y) scan points  per sphere. */
} SphereCheckResult;

struct ZMapInfo {
    ZMapInfo(double minx, double maxx, double miny, double maxy, double minz, double maxz, int _width, int _height)
    {
        minX = minx; maxX = maxx; minY = miny; maxY = maxy; minZ = minz; maxZ = maxz; width = _width; height = _height;
    }

    ZMapInfo()
    {
        minX = maxX = minY = maxY = minZ = maxZ = width = height = 0;
    }

    ZMapInfo& operator=(const ZMapInfo& src)
    {
        minX = src.minX;
        maxX = src.maxX;
        minY = src.minY;
        maxY = src.maxY;
        minZ = src.minZ;
        maxZ = src.maxZ;
        width = src.width;
        height = src.height;
        return *this;
    }

    double minX{}, maxX{}, minY{}, maxY{}, minZ{}, maxZ{};
    int width{}, height{};
};

enum ZMapConvertMode { Top, Bottom, Mean };