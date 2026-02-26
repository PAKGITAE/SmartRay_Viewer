#pragma once
#include <afxwin.h>   // MFC 프로그램이면 이거! (UINT 선언 포함)

// 타이머 ID 모음
// NOTE : 충돌 방지를 위해 1000번대 이상 권장

namespace TimerID
{
    static const UINT UpdateTime = 1001; // 시간
    static const UINT UpdateConnect = 1002; // UI 실시간 상태 업데이트
    static const UINT AutoRotate = 1003; // 3D뷰어 자동 회전

}