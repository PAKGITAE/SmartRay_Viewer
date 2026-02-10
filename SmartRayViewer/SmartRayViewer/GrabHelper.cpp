#include "pch.h"
#include "GrabHelper.h"
#include "vThreadPool.h"

GrabHelper::GrabHelper(int workerThreads)
{
    m_pool = std::make_shared<vThreadPool>(workerThreads);
}

void GrabHelper::SetFrameHandler(FrameHandler handler)
{
    m_handler = std::move(handler);
}

void GrabHelper::AddGrabData(std::shared_ptr<PcFrame> frame)
{
    if (!frame) return;
    frame->frameNo = ++m_frameCounter;

    m_pool->AddJob([this, frame]()
        {
            if (m_handler)
                m_handler(frame);
        });
}