#pragma once
#include "afxdialogex.h"

#include "vGridCtrl.h"
#include "vParameter.h"
#include "AppStore.h"

// DlgParam 대화 상자


static const wchar_t* kParamFilePathW = L"C:\\Parameter\\Setting.json";
static const wchar_t* kParamBackupPathW = L"C:\\Parameter\\Setting_bak.json";

// 파라미터 스펙 정의 (자료형 및 기본값 설정)
struct ParamSpec {
	DataType   type;
	int        defInt;
	double     defDouble;
	const wchar_t* defStr;
	const wchar_t* desc;
};

struct RowInfo {
	std::wstring group;
	std::wstring key;
	DataType    type;
};

// 그룹 정의
static const wchar_t* kGroups[] = { L"Sensor", L"System" };
static const size_t kGroupCount = _countof(kGroups);

// 각 그룹별 키 값 정의
//-----------------------------------------------------------------
//Sensor
static const wchar_t* kSensorKeys[] = {
	L"Sensor1_IP",
	L"Sensor1_Port",
	L"Sensor2_IP",
	L"Sensor2_Port"
};
static const size_t kSensorKeyCount = _countof(kSensorKeys);

static const ParamSpec kSensorKeySpec[kSensorKeyCount] = {
	{ DataType::TYPE_STRING		,  0		, 0		,L"192.168.178.200"		, L"Sensor #1의 IP를 입력한다."},
	{ DataType::TYPE_INT		,  40		, 0		,L""					, L"Sensor #1의 Port번호를 입력한다."},
	{ DataType::TYPE_STRING		,  0		, 0		,L"192.168.178.201"		, L"Sensor #2의 IP를 입력한다."},
	{ DataType::TYPE_INT		,  40		, 0		,L""					, L"Sensor #2의 Port번호를 입력한다."},
};
//-----------------------------------------------------------------

//-----------------------------------------------------------------
//System
static const wchar_t* kSystemKeys[] = {
	L"UpdateFrameViewer",
	L"NumberOfProfiles"
};
static const size_t kSystemKeyCount = _countof(kSystemKeys);

static const ParamSpec kSystemKeySpec[kSystemKeyCount] = {
	{ DataType::TYPE_INT		, 5		, 0		, L""		, L"Point Cloud 3D뷰어의 업데이트 프레임을 설정 합니다." },
	{ DataType::TYPE_INT		, 1000		, 0		, L""		, L"측정을 라인 갯수를 설정합니다.(10단위로 설정)" }
};
//-----------------------------------------------------------------

// ====== 유틸: 디렉터리/파일 ======
static void EnsureDirectoryOf(const CStringW& filePath)
{
	CStringW dir = filePath;
	int pos = dir.ReverseFind(L'\\');
	if (pos >= 0) {
		dir = dir.Left(pos);
		if (!PathFileExistsW(dir)) {
			SHCreateDirectoryExW(nullptr, dir, nullptr);
		}
	}
}

static bool FileExistsW_(const wchar_t* path)
{
	DWORD attr = GetFileAttributesW(path);
	return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

static std::string WStringToString(const std::wstring& ws)
{
	return std::string(ws.begin(), ws.end());
}


class DlgParam : public CDialogEx
{
	DECLARE_DYNAMIC(DlgParam)

public:
	DlgParam(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~DlgParam();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_PARAM };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonParamSave();

private:
	// ---------- 파라미터 그리드 <-> vParameter----------//
	vGridCtrl _GridParam;
	vParameter _vParam;
	std::vector<RowInfo> _rowInfos;

	void InitParamDefaults();       // 기본 키 등록(그룹/키/설명/초기값)
	void LoadParamFileIfExists();   // 초기 inspect.json이 있으면 읽고 없으면 생성
	void FillGridFromParams();      // vParameter -> 그리드
	bool PullGridToParams();        // 그리드 -> vParameter
	void SyncToAppStore();
	//--------------------------------------------------//

	// ---------------------- 유틸----------------------//
	COLORREF GetGroupColor(const std::wstring& group) const;
	//--------------------------------------------------//

};
