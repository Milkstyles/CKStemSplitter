#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

bool isAuditionWindow(HWND window)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) return false;
    std::vector<wchar_t> path(32768);
    DWORD size = static_cast<DWORD>(path.size());
    const bool queried = QueryFullProcessImageNameW(process, 0, path.data(), &size) != FALSE;
    CloseHandle(process);
    if (!queried) return false;
    std::wstring executable(path.data(), size);
    std::transform(executable.begin(), executable.end(), executable.begin(),
                   [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    return executable.ends_with(L"adobe audition.exe");
}

struct DialogSearch { HWND window = nullptr; };

BOOL CALLBACK findDialog(HWND window, LPARAM parameter)
{
    auto* result = reinterpret_cast<DialogSearch*>(parameter);
    if (result->window != nullptr || !IsWindowVisible(window) || !IsWindowEnabled(window)) return TRUE;
    wchar_t className[64]{};
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    if (wcscmp(className, L"#32770") == 0 && isAuditionWindow(window))
    {
        result->window = window;
        return FALSE;
    }
    return TRUE;
}

HWND currentAuditionDialog()
{
    DialogSearch result;
    EnumWindows(findDialog, reinterpret_cast<LPARAM>(&result));
    return result.window;
}

struct EditSearch { HWND filename = nullptr; };

BOOL CALLBACK findFilenameEdit(HWND child, LPARAM parameter)
{
    auto* result = reinterpret_cast<EditSearch*>(parameter);
    if (!IsWindowVisible(child) || !IsWindowEnabled(child)) return TRUE;
    wchar_t className[64]{};
    GetClassNameW(child, className, static_cast<int>(std::size(className)));
    if (wcscmp(className, L"Edit") == 0) result->filename = child;
    return TRUE;
}

bool setDialogFilename(HWND dialog, const std::wstring& filename)
{
    EditSearch search;
    EnumChildWindows(dialog, findFilenameEdit, reinterpret_cast<LPARAM>(&search));
    if (search.filename == nullptr) return false;
    SendMessageW(search.filename, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(filename.c_str()));
    SendMessageW(search.filename, EM_SETSEL, 0, -1);
    return true;
}

bool pressDefaultButton(HWND dialog)
{
    HWND button = GetDlgItem(dialog, IDOK);
    if (button == nullptr || !IsWindowEnabled(button)) return false;
    SendMessageW(button, BM_CLICK, 0, 0);
    return true;
}

bool waitForStableFile(const std::filesystem::path& path, Clock::time_point deadline)
{
    std::uintmax_t previousSize = 0;
    int stableChecks = 0;
    while (Clock::now() < deadline)
    {
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error))
        {
            const auto size = std::filesystem::file_size(path, error);
            if (!error && size > 44 && size == previousSize)
            {
                if (++stableChecks >= 4) return true;
            }
            else
            {
                stableChecks = 0;
                previousSize = size;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return false;
}

int automateDialog(const std::wstring& mode, const std::filesystem::path& path)
{
    const auto deadline = Clock::now() + std::chrono::seconds(45);
    HWND mainDialog = nullptr;
    while (Clock::now() < deadline && mainDialog == nullptr)
    {
        mainDialog = currentAuditionDialog();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (mainDialog == nullptr) return 40;
    if (!setDialogFilename(mainDialog, path.wstring()) || !pressDefaultButton(mainDialog)) return 41;

    const auto secondaryDeadline = Clock::now() + std::chrono::seconds(8);
    while (Clock::now() < secondaryDeadline)
    {
        HWND dialog = currentAuditionDialog();
        if (dialog != nullptr && dialog != mainDialog)
        {
            pressDefaultButton(dialog);
            break;
        }
        if (mode == L"open" && !IsWindow(mainDialog)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (mode == L"save") return waitForStableFile(path, deadline) ? 0 : 42;
    for (int attempt = 0; attempt < 80; ++attempt)
    {
        if (!IsWindow(mainDialog))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 43;
}

bool verifyWaveFile(const std::filesystem::path& inputPath)
{
    std::ifstream input(inputPath, std::ios::binary);
    char header[12]{};
    input.read(header, sizeof(header));
    return input.gcount() == sizeof(header)
        && std::string(header, 4) == "RIFF"
        && std::string(header + 8, 4) == "WAVE";
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == nullptr || argumentCount < 3)
    {
        if (arguments != nullptr) LocalFree(arguments);
        return 2;
    }
    const std::wstring command(arguments[1]);
    int result = 3;
    if (command == L"verify" && argumentCount == 3)
        result = verifyWaveFile(arguments[2]) ? 0 : 30;
    else if (command == L"dialog" && argumentCount == 4)
        result = automateDialog(arguments[2], arguments[3]);
    LocalFree(arguments);
    return result;
}
