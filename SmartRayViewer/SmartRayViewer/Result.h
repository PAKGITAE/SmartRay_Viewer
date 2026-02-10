#pragma once
#include <Windows.h>
#include <mutex>
#include <string>
#include <memory>
#include <functional>

#include "vUtil.h"
#include "AppStore.h"

struct PcFrame;

// 나중에 여기서 PointCloud 데이터 합치는거 로직 넣어야함
// 3D Viewer도 여기서 하면 될꺼 같음

class Result
{
public:
    static Result& GetInstance()
    {
        static Result inst;
        return inst;
    }

    // UI로 프레임 전달할 콜백 등록/해제
    void SetUiFrameCallback(std::function<void(std::shared_ptr<PcFrame>)> cb);

    // GrabHelper/SmartRaySensor에서 Result로 프레임 전달할 때 호출
    void OnFrameArrived(std::shared_ptr<PcFrame> frame);

    // 저장 루트 설정 (예: D:\Data\Sensor1)
    void SetRootFolder(const std::wstring& root) { m_rootFolder = root; }

    // 프레임 1개 저장 (파일 1개 생성)
    bool SaveFrameAsAsc(const std::shared_ptr<PcFrame>& frame);

private:
    Result() = default;

    vUtil _Util;

    std::wstring m_rootFolder = L"D:\\Data\\Sensor1";
    std::mutex m_mtx; // 파일명 생성/저장 동시성 보호(멀티스레드 대비)

    std::mutex m_cbMtx;   // 콜백 보호
    std::function<void(std::shared_ptr<PcFrame>)> m_uiCb;
};
