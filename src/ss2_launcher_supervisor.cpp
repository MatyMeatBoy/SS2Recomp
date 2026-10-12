#include "ss2_launcher_profile.h"
#include "multiplayer_launch.h"
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <sstream>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace {
std::wstring quote(const std::wstring& value) {
    std::wstring out = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        out += c; slashes = 0;
    }
    out.append(slashes * 2, L'\\');
    return out + L"\"";
}
std::wstring wide(const std::string& ascii) { return {ascii.begin(), ascii.end()}; }
}
#endif
namespace ss2 {
int supervise_launcher_recovery(const gbarecomp::GbaNetplayLaunch& launch,
    const gbarecomp::GbaNetplayBoot& boot) {
#if defined(_WIN32)
    gbarecomp::validate_gba_netplay_launch(launch);
    auto root = boot.config_directory.parent_path();
    auto script = root / "tools" / "run_recoverable.ps1";
    if (!std::filesystem::exists(script)) {
        root = root.parent_path();
        script = root / "tools" / "run_recoverable.ps1";
    }
    if (!std::filesystem::exists(script))
        throw std::runtime_error("Missing tools/run_recoverable.ps1; extract the complete release ZIP.");
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH)) throw std::runtime_error("Cannot locate PowerShell.");
    auto powershell = std::filesystem::path(system) / "WindowsPowerShell/v1.0/powershell.exe";
    wchar_t current_exe[32768]{};
    auto exe_length = GetModuleFileNameW(nullptr, current_exe, 32768);
    if (!exe_length || exe_length == 32768) throw std::runtime_error("Cannot locate game executable.");
    auto logs = boot.config_directory / "netplay/recovery" /
        ("room-" + std::to_string(launch.session_id)) / ("seat-" + std::to_string(launch.local_seat));
    if (boot.finish_tick && std::getenv("SS2_RECOVERY_TEST"))
        if (const char* test_dir = std::getenv("SS2_RECOVERY_DIRECTORY")) logs = test_dir;
    static const wchar_t* views[] = {L"native", L"16:9", L"21:9", L"32:9", L"adaptive"};
    std::vector<std::wstring> args = {powershell.wstring(), L"-NoProfile", L"-ExecutionPolicy", L"Bypass",
        L"-File", script.wstring(), L"-RecoveryLogDirectory", logs.wstring(),
        L"-GameExecutable", current_exe, L"--no-launcher", L"--bios", boot.bios_path.wstring(),
        L"--rom", boot.rom_path.wstring(), L"--save-path", boot.local_save.wstring(),
        L"--netplay-players", std::to_wstring(launch.player_count), L"--netplay-seat", std::to_wstring(launch.local_seat),
        L"--netplay-session", std::to_wstring(launch.session_id), L"--netplay-bind", wide(launch.bind_endpoint),
        L"--netplay-view", views[static_cast<unsigned>(launch.view)], L"--volume", std::to_wstring(boot.volume),
        L"--scale", std::to_wstring(boot.scale), L"--fullscreen=" + std::to_wstring(boot.fullscreen),
        L"--linear-filter", boot.linear_filter ? L"1" : L"0", L"--sharp-filter", boot.sharp_filter ? L"1" : L"0",
        L"--affine-filter", boot.affine_filter ? L"1" : L"0"};
    if (launch.local_seat) { args.push_back(L"--netplay-peer"); args.push_back(wide(launch.peer_endpoint)); }
    if (boot.finish_tick) { args.push_back(L"--frames"); args.push_back(std::to_wstring(boot.finish_tick)); }
    std::wstring command;
    for (const auto& arg : args) { if (!command.empty()) command += L' '; command += quote(arg); }
    std::filesystem::create_directories(logs);
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE log = CreateFileW((logs / "supervisor.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        &attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        throw std::runtime_error("Cannot open recovery supervisor log.");
    }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = startup.hStdError = log; startup.hStdInput = input;
    PROCESS_INFORMATION process{};
    // Hidden launcher qualification must also keep its gameplay worker hidden.
    if (boot.finish_tick && std::getenv("SS2_RECOVERY_TEST"))
        SetEnvironmentVariableW(L"SDL_VIDEODRIVER", L"dummy");
    BOOL created = CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, root.c_str(), &startup, &process);
    CloseHandle(log); CloseHandle(input);
    if (!created) throw std::runtime_error("Cannot start recovery supervisor.");
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code); CloseHandle(process.hProcess);
    if (code && !std::getenv("SS2_RECOVERY_TEST")) {
        std::ifstream errors(logs / "game.stderr.log");
        std::ostringstream detail;
        detail << errors.rdbuf();
        std::string message = "No se pudo iniciar la sala.\n\n";
        message += detail.str().substr(0, 2000);
        message += "\nRegistros: " + logs.string();
        MessageBoxA(nullptr, message.c_str(), "ShiningSoul2Recomp - Error de sala", MB_OK | MB_ICONERROR);
    }
    return static_cast<int>(code);
#else
    throw std::runtime_error("Experimental recovery is available on Windows only.");
#endif
}
}
