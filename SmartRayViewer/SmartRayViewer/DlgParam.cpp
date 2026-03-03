// DlgParam.cpp: 구현 파일
//

#include "pch.h"
#include "SmartRayViewer.h"
#include "afxdialogex.h"
#include "DlgParam.h"


// DlgParam 대화 상자

IMPLEMENT_DYNAMIC(DlgParam, CDialogEx)

DlgParam::DlgParam(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_PARAM, pParent)
{

}

DlgParam::~DlgParam()
{
}

void DlgParam::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX, IDC_CUSTOM_GRID, _GridParam);

	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(DlgParam, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_PARAM_SAVE, &DlgParam::OnBnClickedButtonParamSave)
END_MESSAGE_MAP()


// DlgParam 메시지 처리기

BOOL DlgParam::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  여기에 추가 초기화 작업을 추가합니다.
	InitParamDefaults();
	LoadParamFileIfExists();

	FillGridFromParams();
	SyncToAppStore();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 예외: OCX 속성 페이지는 FALSE를 반환해야 합니다.
}


COLORREF DlgParam::GetGroupColor(const std::wstring& group) const
{
    if (group == L"Sensor")   return RGB(225, 241, 255);
    if (group == L"System")   return RGB(229, 248, 236);
    return RGB(240, 240, 240);
}


void DlgParam::LoadParamFileIfExists()
{
    if (FileExistsW_(kParamFilePathW))
        _vParam.LoadParam();
    else
        _vParam.SaveFile();
}


void DlgParam::InitParamDefaults()
{
    // Save path 설정
    _vParam.SetParameterPath(std::wstring(kParamFilePathW));

    EnsureDirectoryOf(CStringW(kParamFilePathW));

    for (size_t gi = 0; gi < kGroupCount; ++gi)
    {
        std::wstring group = kGroups[gi];

        const wchar_t** keys = nullptr;
        const ParamSpec* specs = nullptr;
        size_t keyCount = 0;

        if (group == L"Sensor") {
            keys = kSensorKeys;
            specs = kSensorKeySpec;
            keyCount = kSensorKeyCount;
        }
        else if (group == L"System") {
            keys = kSystemKeys;
            specs = kSystemKeySpec;
            keyCount = kSystemKeyCount;
        }


        for (size_t ki = 0; ki < keyCount; ++ki)
        {
            std::wstring key = keys[ki];
            const ParamSpec& spec = specs[ki];

            switch (spec.type)
            {
            case DataType::TYPE_INT:
                _vParam.CreateParamT<int>(group, key, DataType::TYPE_INT,
                    spec.defInt, spec.desc);
                break;

            case DataType::TYPE_DOUBLE:
                _vParam.CreateParamT<double>(group, key, DataType::TYPE_DOUBLE,
                    spec.defDouble, spec.desc);
                break;

            case DataType::TYPE_STRING:
                _vParam.CreateParamT<std::wstring>(group, key, DataType::TYPE_STRING,
                    spec.defStr, spec.desc);
                break;
            }
        }
    }

    if (!std::filesystem::exists(std::wstring(kParamFilePathW)))
        _vParam.SaveFile();
}

void DlgParam::FillGridFromParams()
{
    _rowInfos.clear();
    _rowInfos.reserve(kGroupCount * (kSensorKeyCount + kSensorKeyCount));

    const int headerRows = 1;
    const int dataRows =
        (int)(kSensorKeyCount + kSystemKeyCount);
    const int rows = headerRows + dataRows;
    const int cols = 4;

    //-----------------------------------------------------------------------------------/
    //차트 기본 설정
    CFont font;
    font.CreatePointFont(100, L"SUIT SemiBold");
    _GridParam.SetFont(&font);

    _GridParam.SetEditable(TRUE);
    _GridParam.SetRowCount(rows);
    _GridParam.SetColumnCount(cols);
    _GridParam.SetFixedRowCount(1);
    _GridParam.SetFixedColumnCount(0);

    //-----------------------------------------------------------------------------------/
    //항목란
    const CStringW headers[cols] = { L"그룹", L"파라미터", L"값", L"설명" };
    for (int c = 0; c < cols; ++c)
    {
        GV_ITEM it{};
        it.mask = GVIF_TEXT | GVIF_FORMAT;
        it.row = 0; it.col = c;
        it.nFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        it.strText = headers[c];
        _GridParam.SetItem(&it);
        _GridParam.SetItemState(0, c, _GridParam.GetItemState(0, c) | GVIS_READONLY);
        _GridParam.SetItemBkColour(0, c, RGB(230, 230, 230));
    }
    //-----------------------------------------------------------------------------------/
    //데이터 넣기
    int row = 1;
    for (size_t gi = 0; gi < kGroupCount; ++gi)
    {
        std::wstring group = kGroups[gi];
        const wchar_t** keys = nullptr;
        const ParamSpec* specs = nullptr;
        size_t keyCount = 0;

        if (group == L"Sensor") {
            keys = kSensorKeys;
            specs = kSensorKeySpec;
            keyCount = kSensorKeyCount;
        }
        else if (group == L"System") {
            keys = kSystemKeys;
            specs = kSystemKeySpec;
            keyCount = kSystemKeyCount;
        }

        for (size_t ki = 0; ki < keyCount; ++ki, ++row)
        {
            std::wstring key = keys[ki];
            const ParamSpec& spec = specs[ki];

            _rowInfos.push_back({ group, key, spec.type });

            COLORREF groupColor = GetGroupColor(group); // 색 선택 원하면 수정 가능

            // (0) 그룹
            _GridParam.SetItemText(row, 0, group.c_str());
            _GridParam.SetItemBkColour(row, 0, groupColor);
            _GridParam.SetItemState(row, 0, _GridParam.GetItemState(row, 0) | GVIS_READONLY);

            // (1) 키
            _GridParam.SetItemText(row, 1, key.c_str());
            _GridParam.SetItemBkColour(row, 1, groupColor);
            _GridParam.SetItemState(row, 1, _GridParam.GetItemState(row, 1) | GVIS_READONLY);

            // (2) 값
            PARAM p = _vParam.GetParam(group, key);
            CStringW valueStr;

            switch (spec.type)
            {
            case DataType::TYPE_INT:      valueStr.Format(L"%d", p.nValue);      break;
            case DataType::TYPE_DOUBLE:   valueStr.Format(L"%.6f", p.dValue);    break;
            case DataType::TYPE_STRING:   valueStr = p.strValue.c_str();         break;
            default: break;
            }


            if (spec.bCombo == true) {
                CStringArray comboItems;
                if (gi == 0) {
                    if (ki == 13) {
                        comboItems.Add(L"FreeRun");
                        comboItems.Add(L"Internal");
                        comboItems.Add(L"External");
                    }
                    else if (ki == 15) {
                        comboItems.Add(L"Quadrature Encoder");
                        comboItems.Add(L"Input 2");
                        comboItems.Add(L"Input 3");
                        comboItems.Add(L"Input 2 / Input 3");
                    }
                    else if (ki == 18) {
                        comboItems.Add(L"Clockwise / Forward");
                        comboItems.Add(L"Anti-Clockwise / Backward");
                        comboItems.Add(L"Both");
                    }
                    
                }
                if (gi == 1) {
                    if (ki == 1) {
                        comboItems.Add(L"단일 센서 측정");
                        comboItems.Add(L"2개 센서 측정");
                        comboItems.Add(L"2개 센서 두께 측정");
                    }
                    if (ki == 2) {
                        comboItems.Add(L"미사용");
                        comboItems.Add(L"사용");
                    }
                }
                

                uint16_t selIndex = 0;
                if (spec.type == DataType::TYPE_BOOLEAN) {
                    bool boolValue = p.nValue;
                    selIndex = boolValue ? 1 : 0;
                }
                else if (spec.type == DataType::TYPE_INT) {
                    int intValue = p.nValue;
                    selIndex = static_cast<uint16_t>(intValue);
                    // 범위 체크
                    if (selIndex >= comboItems.GetSize()) {
                        selIndex = 0;
                    }
                }

                _GridParam.SetCellTypeComboBox(row, 2, comboItems, selIndex);
            }
            else {
                _GridParam.SetItemText(row, 2, valueStr);
            }
            

            // (3) 설명
            _GridParam.SetItemText(row, 3, spec.desc);
            _GridParam.SetItemBkColour(row, 3, RGB(245, 245, 245));
            _GridParam.SetItemState(row, 3, _GridParam.GetItemState(row, 3) | GVIS_READONLY);

            //아래는 그룹별로 동일한 색상을 쓰고 싶을때
            //for (int col = 0; col < cols; ++col) {
            //    _GridParam.SetItemBkColour(row, col, groupColor);
            //    if (col != 2) {
            //        _GridParam.SetItemState(row, col, _GridParam.GetItemState(row, 3) | GVIS_READONLY);
            //    }
            //}

        }
    }
    //-----------------------------------------------------------------------------------/

    //-----------------------------------------------------------------------------------/
    // 3. 그리드 크기 가져와서 Column 폭 자동 계산
    CRect rt;
    GetDlgItem(IDC_CUSTOM_GRID)->GetWindowRect(&rt);
    ScreenToClient(&rt);

    int totalWidth = rt.Width() - 20;

    // 열마다 가중치 설정 (원하는대로 바꾸면 됨!)
    int colWeight[4] = { 1, 2, 2, 5 };

    int totalWeight = 0;
    for (int i = 0; i < 4; i++)
        totalWeight += colWeight[i];

    // 가중치 기반 폭 설정
    for (int i = 0; i < 4; i++)
    {
        int colWidth = (totalWidth * colWeight[i]) / totalWeight;
        _GridParam.SetColumnWidth(i, colWidth);
    }

    // 5. 행 높이
    for (int i = 0; i < _GridParam.GetRowCount(); ++i)
        _GridParam.SetRowHeight(i, 30);
}


bool DlgParam::PullGridToParams()
{
    const int rowCount = _GridParam.GetRowCount();
    if ((int)_rowInfos.size() != rowCount - 1)
        return false;

    bool isChanged = false;
    bool bOK = false;

    for (int row = 1; row < rowCount; ++row)
    {
        CStringW valueW = _GridParam.GetItemText(row, 2);
        RowInfo& info = _rowInfos[row - 1];

        std::wstring group = info.group;
        std::wstring key = info.key;

        PARAM cur = _vParam.GetParam(group, key);
        PARAM newParam = cur;

        switch (info.type)
        {
        case DataType::TYPE_INT:
            newParam.nDataType = DataType::TYPE_INT;
            if (row == 14) {
                if (valueW == L"FreeRun")
                    valueW = L"0";
                else if (valueW == L"Internal")
                    valueW = L"1";
                else if (valueW == L"External")
                    valueW = L"2";
            }
            else if (row == 16) {
                if (valueW == L"Quadrature Encoder")
                    valueW = L"0";
                else if (valueW == L"Input 2")
                    valueW = L"1";
                else if (valueW == L"Input 3")
                    valueW = L"2";
                else if (valueW == L"Input 2 / Input 3")
                    valueW = L"3";
            }
            else if (row == 19) {
                if (valueW == L"Clockwise / Forward")
                    valueW = L"0";
                else if (valueW == L"Anti-Clockwise / Backward")
                    valueW = L"1";
                else if (valueW == L"Both")
                    valueW = L"2";
            }
            else if (row == 21) {
                if (valueW == L"단일 센서 측정")
                    valueW = L"0";
                else if (valueW == L"2개 센서 측정")
                    valueW = L"1";
                else if (valueW == L"2개 센서 두께 측정")
                    valueW = L"2";
            }
            else if (row == 22) {
                if (valueW == L"미사용")
                    valueW = L"0";
                else if (valueW == L"사용")
                    valueW = L"1";
            }
            newParam.nValue = _wtoi(valueW);
            break;
        case DataType::TYPE_DOUBLE:
            newParam.nDataType = DataType::TYPE_DOUBLE;
            newParam.dValue = _wtof(valueW);
            break;
        case DataType::TYPE_STRING:
            newParam.nDataType = DataType::TYPE_STRING;
            newParam.strValue = std::wstring(valueW).c_str();
            break;
        }

        if (memcmp(&cur, &newParam, sizeof(PARAM)) != 0)
        {
            _vParam.SetParam(group, key, newParam);
            isChanged = true;
        }
    }

    if (isChanged)
        bOK = _vParam.SaveFile();

    return bOK;
}

void DlgParam::OnBnClickedButtonParamSave()
{
    // (1) 최신 파라미터 반영
    bool bReturn = PullGridToParams();

    if (bReturn == true) {
        SyncToAppStore();
        AfxMessageBox(L"저장 완료!!!");
    }
    else {
        AfxMessageBox(L"저장 실패.", MB_ICONSTOP);
    }

}

// 모든 파라미터 + ROI를 AppStore에 등록
void DlgParam::SyncToAppStore()
{
    auto pushGroup = [&](const wchar_t* wgroup,
        const wchar_t** wkeys,
        size_t keyCount,
        const ParamSpec* specs)
        {
            std::string group = WStringToString(wgroup);

            for (size_t i = 0; i < keyCount; ++i)
            {
                std::string key = WStringToString(wkeys[i]);
                PARAM p = _vParam.GetParam(wgroup, wkeys[i]);

                switch (specs[i].type)
                {
                case DataType::TYPE_INT:
                    AppStore::Get().SetParameterInt(group, key, p.nValue);
                    break;

                case DataType::TYPE_DOUBLE:
                    AppStore::Get().SetParameterDouble(group, key, p.dValue);
                    break;

                case DataType::TYPE_STRING:
                    std::string value = WStringToString(p.strValue);
                    AppStore::Get().SetParameterString(group, key, value);
                    break;
                }
            }
        };

    pushGroup(L"Sensor", kSensorKeys, kSensorKeyCount, kSensorKeySpec);
    pushGroup(L"System", kSystemKeys, kSystemKeyCount, kSystemKeySpec);
}