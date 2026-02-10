#pragma once
#include <vector>
#include <memory>
#include <atomic>
#include <cstdint>
#include <functional>
#include <cstring>     // memcpy

#include "LogManager.h"
#include "Result.h"

#include "SR_API_public.h"

class vThreadPool;

// 콜백에서 받은 데이터의 "안전한 복사본"
struct PcFrame
{
    int camIndex = 0;
    uint64_t frameNo = 0;

    uint32_t numPoints = 0;
    uint32_t numProfiles = 0;   //콜백에 포함된 profile 개수

    std::vector<SR_3DPOINT> points;
    std::vector<unsigned short> intensity;
};

class GrabHelper
{
public:
    using FrameHandler = std::function<void(std::shared_ptr<PcFrame>)>;

    explicit GrabHelper(int workerThreads = 2);
    ~GrabHelper() = default;

    // worker thread에서 처리할 핸들러 등록(예: Inspect로 넘기기, 파일 저장 등)
    void SetFrameHandler(FrameHandler handler);

    // 콜백에서 호출: "복사본 프레임"을 넘겨주면 됨
    void AddGrabData(std::shared_ptr<PcFrame> frame);

private:
    std::shared_ptr<vThreadPool> m_pool;
    FrameHandler m_handler;
    
    std::atomic<uint64_t> m_frameCounter{ 0 };
};
