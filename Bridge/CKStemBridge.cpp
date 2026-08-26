#include <windows.h>
#include <shellapi.h>
#include <ole2.h>
#include <UIAutomation.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
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

bool hasStemEffectTitle(HWND window)
{
    wchar_t title[512]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    std::wstring text(title);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    return text.find(L"ck stem splitter") != std::wstring::npos;
}

BOOL CALLBACK findEffectWindow(HWND window, LPARAM parameter)
{
    auto* result = reinterpret_cast<EffectSearch*>(parameter);
    if (result->window != nullptr || !IsWindowVisible(window) || !isAuditionWindow(window)) return TRUE;
    if (hasStemEffectTitle(window))
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

bool sendRealMouseClick(int x, int y)
{
    POINT previous{};
    GetCursorPos(&previous);
    if (!SetCursorPos(x, y)) return false;
    INPUT click[2]{};
    click[0].type = INPUT_MOUSE;
    click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    click[1].type = INPUT_MOUSE;
    click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    const auto sent = SendInput(2, click, sizeof(INPUT)) == 2;
    SetCursorPos(previous.x, previous.y);
    return sent;
}

bool clickEffectApply(HWND effectWindow)
{
    ButtonSearch search;
    EnumChildWindows(effectWindow, findApplyButton, reinterpret_cast<LPARAM>(&search));
    if (search.button != nullptr)
    {
        RECT buttonRect{};
        if (!GetWindowRect(search.button, &buttonRect)) return false;
        SetForegroundWindow(effectWindow);
        return sendRealMouseClick((buttonRect.left + buttonRect.right) / 2,
                                  (buttonRect.top + buttonRect.bottom) / 2);
    }

    if (!hasStemEffectTitle(effectWindow)) return false;
    RECT rect{};
    if (!GetWindowRect(effectWindow, &rect)) return false;
    const auto width = rect.right - rect.left;
    const auto height = rect.bottom - rect.top;
    if (width < 300 || height < 180) return false;

    SetForegroundWindow(effectWindow);
    return sendRealMouseClick(rect.right - 140, rect.bottom - 28);
}

struct PluginChildSearch
{
    HWND window = nullptr;
    long bestDifference = LONG_MAX;
};

BOOL CALLBACK findPluginChild(HWND child, LPARAM parameter)
{
    auto* result = reinterpret_cast<PluginChildSearch*>(parameter);
    if (!IsWindowVisible(child)) return TRUE;
    RECT rect{};
    GetClientRect(child, &rect);
    const auto width = rect.right - rect.left;
    const auto height = rect.bottom - rect.top;
    if (width < 580 || height < 380) return TRUE;
    const auto difference = std::labs(width - 620) + std::labs(height - 430);
    if (difference < result->bestDifference)
    {
        result->window = child;
        result->bestDifference = difference;
    }
    return TRUE;
}

bool clickPreparedStemControl(HWND effectWindow)
{
    PluginChildSearch search;
    EnumChildWindows(effectWindow, findPluginChild, reinterpret_cast<LPARAM>(&search));
    HWND target = search.window != nullptr ? search.window : effectWindow;
    RECT rect{};
    GetClientRect(target, &rect);
    const auto width = rect.right - rect.left;
    const auto height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return false;
    const auto x = width / 2;
    const auto y = search.window != nullptr ? 235 : std::min(height - 100L, 290L);
    const auto point = MAKELPARAM(x, y);
    SendMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(target, WM_LBUTTONUP, 0, point);
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
    std::vector<wchar_t> modulePath(32768);
    const auto moduleLength = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (moduleLength == 0 || moduleLength >= modulePath.size()) return false;
    const auto root = std::filesystem::path(std::wstring(modulePath.data(), moduleLength))
        .parent_path().parent_path();
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

void writeAutomationResult(int result)
{
    const auto stateDir = userStateDirectory();
    if (stateDir.empty()) return;
    std::filesystem::create_directories(stateDir);
    std::wofstream output(stateDir / L"automation-log.txt", std::ios::app);
    SYSTEMTIME time{};
    GetLocalTime(&time);
    output << time.wYear << L'-' << time.wMonth << L'-' << time.wDay << L' '
           << time.wHour << L':' << time.wMinute << L':' << time.wSecond
           << L" result=" << result << L"\n";
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

template <typename T>
void releaseCom(T*& pointer)
{
    if (pointer != nullptr)
    {
        pointer->Release();
        pointer = nullptr;
    }
}

IUIAutomationElement* findMenuItem(IUIAutomation* automation,
                                   IUIAutomationElement* root,
                                   const wchar_t* name)
{
    VARIANT nameValue{};
    nameValue.vt = VT_BSTR;
    nameValue.bstrVal = SysAllocString(name);
    IUIAutomationCondition* nameCondition = nullptr;
    automation->CreatePropertyCondition(UIA_NamePropertyId, nameValue, &nameCondition);
    VariantClear(&nameValue);

    VARIANT typeValue{};
    typeValue.vt = VT_I4;
    typeValue.lVal = UIA_MenuItemControlTypeId;
    IUIAutomationCondition* typeCondition = nullptr;
    automation->CreatePropertyCondition(UIA_ControlTypePropertyId, typeValue, &typeCondition);

    IUIAutomationCondition* combined = nullptr;
    if (nameCondition != nullptr && typeCondition != nullptr)
        automation->CreateAndCondition(nameCondition, typeCondition, &combined);

    IUIAutomationElement* result = nullptr;
    if (combined != nullptr)
        root->FindFirst(TreeScope_Subtree, combined, &result);
    releaseCom(combined);
    releaseCom(typeCondition);
    releaseCom(nameCondition);
    return result;
}

bool activateMenuItem(IUIAutomationElement* element)
{
    if (element == nullptr) return false;
    IUIAutomationInvokePattern* invoke = nullptr;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke))) && invoke != nullptr)
    {
        const auto result = SUCCEEDED(invoke->Invoke());
        releaseCom(invoke);
        return result;
    }

    IUIAutomationExpandCollapsePattern* expand = nullptr;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, IID_PPV_ARGS(&expand))) && expand != nullptr)
    {
        const auto result = SUCCEEDED(expand->Expand());
        releaseCom(expand);
        return result;
    }

    IUIAutomationLegacyIAccessiblePattern* legacy = nullptr;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, IID_PPV_ARGS(&legacy))) && legacy != nullptr)
    {
        const auto result = SUCCEEDED(legacy->DoDefaultAction());
        releaseCom(legacy);
        return result;
    }
    return false;
}

bool openExactStemEffect()
{
    MainWindowSearch main;
    EnumWindows(findMainAuditionWindow, reinterpret_cast<LPARAM>(&main));
    if (main.window == nullptr) return false;
    SetForegroundWindow(main.window);
    BringWindowToTop(main.window);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const auto comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto uninitialise = SUCCEEDED(comResult);
    IUIAutomation* automation = nullptr;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&automation))) || automation == nullptr)
    {
        if (uninitialise) CoUninitialize();
        return false;
    }

    IUIAutomationElement* desktop = nullptr;
    IUIAutomationElement* audition = nullptr;
    automation->GetRootElement(&desktop);
    automation->ElementFromHandle(main.window, &audition);
    if (desktop == nullptr || audition == nullptr)
    {
        releaseCom(audition);
        releaseCom(desktop);
        releaseCom(automation);
        if (uninitialise) CoUninitialize();
        return false;
    }

    const wchar_t* path[] = { L"Effects", L"VST3", L"Commercial Kings", L"CK Stem Splitter" };
    bool opened = true;
    for (int index = 0; index < 4 && opened; ++index)
    {
        auto* searchRoot = index == 0 ? audition : desktop;
        auto* item = findMenuItem(automation, searchRoot, path[index]);
        if (item == nullptr && index == 1)
            item = findMenuItem(automation, searchRoot, L"VST 3");
        opened = activateMenuItem(item);
        releaseCom(item);
        if (opened)
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
    }

    releaseCom(audition);
    releaseCom(desktop);
    releaseCom(automation);
    if (uninitialise) CoUninitialize();
    return opened;
}

int orchestrate(const std::wstring& mode, const std::wstring& requestId, HWND suppliedPluginWindow)
{
    const auto stateDir = userStateDirectory();
    if (stateDir.empty()) return 50;
    std::filesystem::create_directories(stateDir);
    const auto stateFile = stateDir / L"last-scan.txt";
    const auto previousState = readTextFile(stateFile);

    const auto firstDeadline = Clock::now() + std::chrono::seconds(20);
    HWND firstEffect = currentEffectWindow();
    if (firstEffect == nullptr && suppliedPluginWindow != nullptr)
    {
        auto candidate = GetAncestor(suppliedPluginWindow, GA_ROOT);
        if (candidate != nullptr && IsWindow(candidate) && isAuditionWindow(candidate)
            && hasStemEffectTitle(candidate))
            firstEffect = candidate;
    }
    while (Clock::now() < firstDeadline && firstEffect == nullptr)
    {
        firstEffect = currentEffectWindow();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (firstEffect == nullptr) return 51;
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    if (!IsWindow(firstEffect) || !clickEffectApply(firstEffect)) return 62;
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
    if (!openExactStemEffect()) return 63;
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
    else if (command == L"orchestrate" && argumentCount == 5)
    {
        const auto rawWindow = static_cast<std::uintptr_t>(_wcstoui64(arguments[4], nullptr, 10));
        result = orchestrate(arguments[2], arguments[3], reinterpret_cast<HWND>(rawWindow));
    }
    if (command == L"orchestrate")
        writeAutomationResult(result);
    LocalFree(arguments);
    return result;
}

