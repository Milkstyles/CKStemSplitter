#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
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

struct EffectSearch { HWND window = nullptr; };

BOOL CALLBACK findEffectWindow(HWND window, LPARAM parameter)
{
    auto* result = reinterpret_cast<EffectSearch*>(parameter);
    if (result->window != nullptr || !IsWindowVisible(window) || !isAuditionWindow(window)) return TRUE;
    wchar_t title[512]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    std::wstring text(title);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    if (text.find(L"ck stem splitter") != std::wstring::npos)
    {
        result->window = window;
        return FALSE;
    }
    return TRUE;
}

HWND currentEffectWindow()
{
    EffectSearch result;
    EnumWindows(findEffectWindow, reinterpret_cast<LPARAM>(&result));
    return result.window;
}

struct ButtonSearch { HWND button = nullptr; };

BOOL CALLBACK findApplyButton(HWND child, LPARAM parameter)
{
    auto* result = reinterpret_cast<ButtonSearch*>(parameter);
    if (result->button != nullptr || !IsWindowVisible(child) || !IsWindowEnabled(child)) return TRUE;
    wchar_t title[128]{};
    GetWindowTextW(child, title, static_cast<int>(std::size(title)));
    std::wstring text(title);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    if (text.find(L"apply") != std::wstring::npos || text == L"ok")
    {
        result->button = child;
        return FALSE;
    }
    return TRUE;
}

bool clickEffectApply(HWND effectWindow)
{
    ButtonSearch search;
    EnumChildWindows(effectWindow, findApplyButton, reinterpret_cast<LPARAM>(&search));
    if (search.button == nullptr) return false;
    SendMessageW(search.button, BM_CLICK, 0, 0);
    return true;
}

struct MainWindowSearch { HWND window = nullptr; };

BOOL CALLBACK findMainAuditionWindow(HWND window, LPARAM parameter)
{
    auto* result = reinterpret_cast<MainWindowSearch*>(parameter);
    if (result->window != nullptr || !IsWindowVisible(window) || !isAuditionWindow(window)) return TRUE;
    wchar_t title[512]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    std::wstring text(title);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    if (text.find(L"ck stem splitter") == std::wstring::npos)
    {
        result->window = window;
        return FALSE;
    }
    return TRUE;
}

std::filesystem::path userStateDirectory()
{
    const wchar_t* appData = _wgetenv(L"APPDATA");
    if (appData == nullptr) return {};
    return std::filesystem::path(appData) / L"Commercial Kings" / L"CK Stem Splitter";
}

std::wstring readTextFile(const std::filesystem::path& path)
{
    std::wifstream input(path);
    return { std::istreambuf_iterator<wchar_t>(input), std::istreambuf_iterator<wchar_t>() };
}

std::vector<std::wstring> splitLines(const std::wstring& text)
{
    std::vector<std::wstring> lines;
    std::wstring current;
    for (const auto character : text)
    {
        if (character == L'\r') continue;
        if (character == L'\n')
        {
            lines.push_back(current);
            current.clear();
        }
        else current.push_back(character);
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

bool runStemEngine(const std::filesystem::path& source,
                   const std::filesystem::path& outputDirectory)
{
    const wchar_t* programData = _wgetenv(L"PROGRAMDATA");
    if (programData == nullptr) return false;
    const auto root = std::filesystem::path(programData) / L"Commercial Kings" / L"CK Stem Splitter";
    const auto engine = root / L"engine" / L"ckstem-engine" / L"ckstem-engine.exe";
    const auto models = root / L"engine" / L"models";
    if (!std::filesystem::is_regular_file(engine)) return false;
    std::filesystem::create_directories(outputDirectory);

    std::wstring command = L"\"" + engine.wstring() + L"\" separate \"" + source.wstring()
        + L"\" \"" + outputDirectory.wstring()
        + L"\" --model htdemucs_ft_vocals --small --providers auto --cache-dir \""
        + models.wstring() + L"\" --karaoke --quiet";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        return false;
    WaitForSingleObject(process.hProcess, 15 * 60 * 1000);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode == 0;
}

bool waitForChangedState(const std::filesystem::path& stateFile,
                         const std::wstring& previous,
                         Clock::time_point deadline)
{
    while (Clock::now() < deadline)
    {
        const auto content = readTextFile(stateFile);
        if (!content.empty() && content != previous)
        {
            const auto end = content.find_first_of(L"\r\n");
            const auto capture = std::filesystem::path(content.substr(0, end));
            std::error_code error;
            if (std::filesystem::is_regular_file(capture, error) && std::filesystem::file_size(capture, error) > 44)
                return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

bool sendRepeatWithoutApply()
{
    MainWindowSearch main;
    EnumWindows(findMainAuditionWindow, reinterpret_cast<LPARAM>(&main));
    if (main.window == nullptr) return false;
    SetForegroundWindow(main.window);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    INPUT inputs[4]{};
    inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = 'R';
    inputs[2].type = INPUT_KEYBOARD; inputs[2].ki.wVk = 'R'; inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD; inputs[3].ki.wVk = VK_CONTROL; inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(4, inputs, sizeof(INPUT)) == 4;
}

int orchestrate(const std::wstring& mode, const std::wstring& requestId)
{
    const auto stateDir = userStateDirectory();
    if (stateDir.empty()) return 50;
    std::filesystem::create_directories(stateDir);
    const auto stateFile = stateDir / L"last-scan.txt";
    const auto previousState = readTextFile(stateFile);

    const auto firstDeadline = Clock::now() + std::chrono::seconds(20);
    HWND firstEffect = nullptr;
    while (Clock::now() < firstDeadline && firstEffect == nullptr)
    {
        firstEffect = currentEffectWindow();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (firstEffect == nullptr || !clickEffectApply(firstEffect)) return 51;
    if (!waitForChangedState(stateFile, previousState, Clock::now() + std::chrono::seconds(90))) return 52;

    const auto closeDeadline = Clock::now() + std::chrono::seconds(15);
    while (Clock::now() < closeDeadline && IsWindow(firstEffect) && IsWindowVisible(firstEffect))
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto stateLines = splitLines(readTextFile(stateFile));
    if (stateLines.size() < 2) return 57;
    const auto sourceFile = std::filesystem::path(stateLines[0]);
    const auto outputDirectory = stateDir / L"Automation" / requestId;
    if (!runStemEngine(sourceFile, outputDirectory)) return 58;
    const auto vocalsFile = outputDirectory / L"vocals.wav";
    auto instrumentalFile = outputDirectory / L"instrumental.wav";
    const auto karaokeFile = outputDirectory / L"karaoke.wav";
    if (!std::filesystem::is_regular_file(instrumentalFile) && std::filesystem::is_regular_file(karaokeFile))
    {
        std::error_code error;
        std::filesystem::rename(karaokeFile, instrumentalFile, error);
        if (error) return 59;
    }
    if (!std::filesystem::is_regular_file(vocalsFile) || !std::filesystem::is_regular_file(instrumentalFile)) return 60;

    const auto processFile = stateDir / L"automation-process.txt";
    {
        std::wofstream output(processFile, std::ios::trunc);
        output << requestId << L"\n" << mode << L"\n"
               << sourceFile.wstring() << L"\n" << stateLines[1] << L"\n"
               << vocalsFile.wstring() << L"\n" << instrumentalFile.wstring() << L"\n";
    }
    if (!sendRepeatWithoutApply()) return 53;

    HWND secondEffect = nullptr;
    const auto secondDeadline = Clock::now() + std::chrono::seconds(20);
    while (Clock::now() < secondDeadline)
    {
        auto candidate = currentEffectWindow();
        if (candidate != nullptr && IsWindowVisible(candidate))
        {
            secondEffect = candidate;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (secondEffect == nullptr) return 54;

    const auto readyFile = stateDir / L"automation-ready.txt";
    const auto readyDeadline = Clock::now() + std::chrono::minutes(15);
    while (Clock::now() < readyDeadline)
    {
        auto ready = readTextFile(readyFile);
        while (!ready.empty() && (ready.back() == L'\r' || ready.back() == L'\n')) ready.pop_back();
        if (ready == requestId)
        {
            std::error_code ignored;
            std::filesystem::remove(readyFile, ignored);
            return clickEffectApply(secondEffect) ? 0 : 56;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 55;
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
    else if (command == L"orchestrate" && argumentCount == 4)
        result = orchestrate(arguments[2], arguments[3]);
    LocalFree(arguments);
    return result;
}
