#pragma once

#include "vLabel.h"
#include "vIconButton.h"
#include <string>

#include <filesystem>

// 전역 UI 헬퍼 함수들
namespace UIHelper
{
	// Label 초기화 함수
	void InitLabel(vLabel& label, const std::wstring& text, eLabelAlignH hAlign, eLabelAlignV vAlign, 
		int fontSize, COLORREF fontColor, COLORREF bgColor, const std::wstring& iconPath, int iconMargin);

	// IconButton 초기화 함수
	void InitIconButton(vIconButton& btn, const std::wstring& text, const std::wstring& iconPath, 
		int fontSize, bool isFocus, COLORREF fontColor, COLORREF bgColor, COLORREF bgFocusColor);
}
