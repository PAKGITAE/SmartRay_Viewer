#pragma once

#include <vector>
#include <string>

// 시스템 문자열 상수 네임스페이스
namespace SysConstants
{
    const std::wstring RecipeFolderName = L"Recipe";
    const std::wstring SysFolderName = L"System";

    const std::wstring SysFildName = L"SysParam.json";
}

inline std::wstring GetBasePath()
{
	// exe 실제 경로
	wchar_t exePath[MAX_PATH] = { 0 };
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);

	std::wstring exeDir(exePath);
	size_t pos = exeDir.find_last_of(L"\\/");
	if (pos != std::wstring::npos)
		exeDir = exeDir.substr(0, pos);

	return exeDir;
}

inline std::wstring CombinePath(const std::wstring& base, const std::wstring& relative)
{
    if (base.empty()) return relative;
    if (relative.empty()) return base;

    std::wstring result = base;
    if (result.back() != L'\\' && result.back() != L'/')
        result += L"\\";
    if (relative.front() == L'\\' || relative.front() == L'/')
        result += relative.substr(1);
    else
        result += relative;
    return result;
}

// 폴더 내 파일 리스트를 반환하는 함수
inline std::vector<std::wstring> GetFileList(const std::wstring& folderPath)
{
    std::vector<std::wstring> fileList;
    
    if (folderPath.empty())
        return fileList;

    // 경로 끝에 \*.* 추가
    std::wstring searchPath = folderPath;
    if (searchPath.back() != L'\\' && searchPath.back() != L'/')
        searchPath += L"\\";
    searchPath += L"*.*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
        return fileList;

    do
    {
        // . 과 .. 디렉토리 제외
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
            continue;

        // 파일만 추가 (디렉토리 제외)
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            fileList.push_back(findData.cFileName);
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);
    return fileList;
}

// 파일명에서 확장자를 제거하는 함수
inline std::wstring RemoveFileExtension(const std::wstring& fileName)
{
    if (fileName.empty())
        return fileName;

    size_t lastDot = fileName.find_last_of(L".");
    size_t lastSlash = fileName.find_last_of(L"\\/");

    // 마지막 점이 마지막 슬래시 이후에 있고, 점이 존재하는 경우
    if (lastDot != std::wstring::npos && (lastSlash == std::wstring::npos || lastDot > lastSlash))
    {
        return fileName.substr(0, lastDot);
    }

    // 확장자가 없는 경우 원본 반환
    return fileName;
}