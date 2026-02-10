#include "pch.h"
#include "UIHelper.h"

namespace UIHelper
{
	void InitLabel(vLabel& label, const std::wstring& text, eLabelAlignH hAlign, eLabelAlignV vAlign, 
		int fontSize, COLORREF fontColor, COLORREF bgColor, const std::wstring& iconPath, int iconMargin)
	{
		std::wstring wfont = L"SUIT SemiBold";

		//폰트 및 얼라인 설정
		label.SetTextType(fontSize, fontColor, wfont, hAlign, vAlign);
		if (hAlign == eLabelAlignH::Left)
		{
			label.SetTextOffset(20, 0);
		}
		label.SetTransparent(false);
		label.SetBackground(bgColor);
		label.SetText(text.c_str());

		//아이콘 경로가 존재할때 얼라인
		if (!iconPath.empty()) {
			namespace fs = std::filesystem;
			fs::path exePath = fs::current_path();

			fs::path iconfullPath = exePath / L"icon" / iconPath;

			label.SetIconAlign(eLabelAlignH::Center, eLabelAlignV::Center);
			label.SetIconMargin(iconMargin);
			label.SetPngIcon(iconfullPath.c_str());
		}

		label.Draw();
	}

	void InitIconButton(vIconButton& btn, const std::wstring& text, const std::wstring& iconPath, 
		int fontSize, bool isFocus, COLORREF fontColor, COLORREF bgColor, COLORREF bgFocusColor)
	{
		std::wstring wfont = L"SUIT SemiBold";

		namespace fs = std::filesystem;
		fs::path exePath = fs::current_path();

		fs::path iconfullPath = exePath / L"icon" / iconPath;

		btn.SetButtonType(eButtonType::kNormal);
		btn.SetPngIcon(eBgType::kBgNormal, iconfullPath.c_str());
		btn.SetBackground(eBgType::kBgNormal, bgColor);

		if (isFocus) {
			btn.SetBackground(eBgType::kBgFocus, bgFocusColor);
			btn.SetPngIcon(eBgType::kBgFocus, iconfullPath.c_str());
		}

		btn.SetIconAlign(eIconButtonAlignH::Left, eIconButtonAlignV::Center);
		btn.SetIconMargin(7);
		btn.SetTextType(fontSize, fontColor, wfont, eIconButtonAlignH::Center, eIconButtonAlignV::Center);
		btn.SetTextOffset(0, 0);
		btn.SetText(text.c_str());
	}
}
