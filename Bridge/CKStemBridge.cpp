#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t fourcc(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a))
         | (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8)
         | (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16)
         | (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

std::uint32_t readU32(const unsigned char* data)
{
    return static_cast<std::uint32_t>(data[0])
         | (static_cast<std::uint32_t>(data[1]) << 8)
         | (static_cast<std::uint32_t>(data[2]) << 16)
         | (static_cast<std::uint32_t>(data[3]) << 24);
}

bool isRiffWave(const std::vector<unsigned char>& bytes)
{
    return bytes.size() >= 12
        && readU32(bytes.data()) == fourcc('R', 'I', 'F', 'F')
        && readU32(bytes.data() + 8) == fourcc('W', 'A', 'V', 'E');
}

bool readFile(const std::filesystem::path& path, std::vector<unsigned char>& bytes)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        return false;

    const auto size = input.tellg();
    if (size <= 0)
        return false;

    const auto byteCount = static_cast<std::streamsize>(size);
    bytes.resize(static_cast<std::size_t>(byteCount));
    input.seekg(0, std::ios::beg);
    return static_cast<bool>(input.read(reinterpret_cast<char*>(bytes.data()), byteCount));
}

bool writeFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    return output && static_cast<bool>(output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())));
}

bool openClipboardWithRetry()
{
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        if (OpenClipboard(nullptr))
            return true;
        Sleep(50);
    }
    return false;
}

int captureClipboardWave(const std::filesystem::path& outputPath)
{
    if (!openClipboardWithRetry())
        return 10;

    // Audition commonly uses CF_RIFF for 32-bit float selections and CF_WAVE
    // for standard PCM. Both contain complete RIFF/WAVE file bytes.
    HANDLE handle = GetClipboardData(CF_RIFF);
    if (handle == nullptr)
        handle = GetClipboardData(CF_WAVE);
    if (handle == nullptr)
    {
        CloseClipboard();
        return 11;
    }

    const auto byteCount = GlobalSize(handle);
    const auto* memory = static_cast<const unsigned char*>(GlobalLock(handle));
    if (memory == nullptr || byteCount < 12)
    {
        if (memory != nullptr)
            GlobalUnlock(handle);
        CloseClipboard();
        return 12;
    }

    std::vector<unsigned char> bytes(memory, memory + byteCount);
    GlobalUnlock(handle);
    CloseClipboard();

    if (!isRiffWave(bytes))
        return 13;

    return writeFile(outputPath, bytes) ? 0 : 14;
}

int publishClipboardWave(const std::filesystem::path& inputPath)
{
    std::vector<unsigned char> bytes;
    if (!readFile(inputPath, bytes) || !isRiffWave(bytes))
        return 20;

    if (!openClipboardWithRetry())
        return 21;

    if (!EmptyClipboard())
    {
        CloseClipboard();
        return 22;
    }

    HGLOBAL memoryHandle = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (memoryHandle == nullptr)
    {
        CloseClipboard();
        return 23;
    }

    void* memory = GlobalLock(memoryHandle);
    if (memory == nullptr)
    {
        GlobalFree(memoryHandle);
        CloseClipboard();
        return 24;
    }

    CopyMemory(memory, bytes.data(), bytes.size());
    GlobalUnlock(memoryHandle);

    if (SetClipboardData(CF_RIFF, memoryHandle) == nullptr)
    {
        GlobalFree(memoryHandle);
        CloseClipboard();
        return 25;
    }

    // Also advertise CF_WAVE for hosts that only request the older standard
    // format. The clipboard owns each successfully published memory handle.
    HGLOBAL waveHandle = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (waveHandle != nullptr)
    {
        void* waveMemory = GlobalLock(waveHandle);
        if (waveMemory != nullptr)
        {
            CopyMemory(waveMemory, bytes.data(), bytes.size());
            GlobalUnlock(waveHandle);
            if (SetClipboardData(CF_WAVE, waveHandle) == nullptr)
                GlobalFree(waveHandle);
        }
        else
        {
            GlobalFree(waveHandle);
        }
    }

    CloseClipboard();
    return 0;
}

int verifyWaveFile(const std::filesystem::path& inputPath)
{
    std::vector<unsigned char> bytes;
    return readFile(inputPath, bytes) && isRiffWave(bytes) ? 0 : 30;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == nullptr || argumentCount != 3)
    {
        if (arguments != nullptr)
            LocalFree(arguments);
        return 2;
    }

    const std::wstring command(arguments[1]);
    const std::filesystem::path path(arguments[2]);
    LocalFree(arguments);

    if (command == L"capture")
        return captureClipboardWave(path);
    if (command == L"publish")
        return publishClipboardWave(path);
    if (command == L"verify")
        return verifyWaveFile(path);
    return 3;
}
