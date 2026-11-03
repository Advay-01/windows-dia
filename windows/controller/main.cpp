#include "target.h"
#include "channel.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include "feature_state.h"

static volatile LONG interrupted = 0;
static BOOL WINAPI interrupt(DWORD) { InterlockedExchange(&interrupted, 1); return TRUE; }
static void usage() {
    std::wcout << L"DiaTweaks Windows prototype (x64)\n"
        L"Usage: diatweaks-probe [--pid PID] [--check]\n"
        L"Open Dia yourself first. --check only validates a target; it does not attach.\n"
        L"Interactive: tree [filter], layout ID, inspect ID, pulse ID SECONDS (1..30), settings ID, overlay ID, features, restore, quit\n"
        L"Feature: enable KEY / disable KEY (this session only); --log PATH records unwrapped diagnostics\n"
        L"IDs come from tree, not raw memory addresses. Pulse is a temporary developer test.\n";
}
int wmain(int argc, wchar_t** argv) {
    using namespace dt;
    DWORD requestedPid = 0;
    bool checkOnly = false;
    std::filesystem::path logPath;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg(argv[i]);
        if (arg == L"--help" || arg == L"-h") { usage(); return 0; }
        if (arg == L"--check") checkOnly = true;
        else if (arg == L"--log" && i + 1 < argc) logPath = argv[++i];
        else if (arg == L"--pid" && i + 1 < argc) {
            wchar_t* end = nullptr;
            auto pid = wcstoul(argv[++i], &end, 10);
            if (!pid || *end) { usage(); return 2; }
            requestedPid = pid;
        } else { usage(); return 2; }
    }
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    int exitCode = 0;
    const wchar_t* stage = L"initialize COM";
    try {
        check(com);
        stage = L"find Dia UI process";
        const auto target = find_target(requestedPid);
        std::wcout << L"Dia UI process: " << target.pid << L"\nDiagnostics: " << target.framework.c_str() << L'\n';
        if (!checkOnly) {
            wchar_t executable[32768]{};
            if (!GetModuleFileNameW(nullptr, executable, 32768)) throw last_error();
            auto dll = std::filesystem::path(executable).parent_path() / L"DiaTweaksTap.dll";
            if (!std::filesystem::is_regular_file(dll)) throw HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            stage = L"create controller IPC channel";
            Channel channel(target.pid);
            stage = L"attach WinUI diagnostics TAP";
            check(attach(target, dll, channel.name));
            stage = L"wait for TAP handshake";
            const auto deadline = GetTickCount64() + 10000;
            while (read_flag(channel.shared->ready) == 0 && GetTickCount64() < deadline) Sleep(25);
            if (read_flag(channel.shared->ready) != 1) {
                check(channel.shared->startup);
                throw HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            }
            SetConsoleCtrlHandler(interrupt, TRUE);
            std::ofstream log;
            if (!logPath.empty()) { log.open(logPath, std::ios::binary | std::ios::app); if (!log) throw E_ACCESSDENIED; }
            usage();
            std::wcout << L"Attached. Type tree to inspect controls. No override is enabled.\n";
            std::wstring line;
            while (!read_flag(interrupted) && std::wcout << L"> " && std::getline(std::wcin, line)) {
                std::wistringstream input(line);
                std::wstring verb, extra;
                input >> verb;
                if (verb == L"quit" || verb == L"exit") break;
                if (verb.empty()) continue;
                Request request{};
                if (verb == L"tree") { request.command = Command::Tree; std::getline(input >> std::ws, extra); }
                else if (verb == L"restore") request.command = Command::Restore;
                else if (verb == L"features") request.command = Command::Features;
                else if (verb == L"enable" || verb == L"disable") {
                    std::wstring key;
                    if (!(input >> key) || feature_id(key) == FeatureId::Count) {
                        std::wcout << L"Unknown toggle key.\n"; continue;
                    }
                    request = {Command::SetFeature, static_cast<uint64_t>(feature_id(key)), verb == L"enable" ? 1U : 0U};
                }
                else if (verb == L"inspect" || verb == L"pulse" || verb == L"settings" || verb == L"layout" || verb == L"overlay") {
                    request.command = verb == L"overlay" ? Command::Overlay : verb == L"layout" ? Command::Layout : (verb == L"settings" ? Command::Settings : (verb == L"inspect" ? Command::Inspect : Command::Pulse));
                    if (!(input >> request.handle) || !request.handle ||
                        (verb == L"pulse" && (!(input >> request.seconds) || request.seconds < 1 || request.seconds > 30))) {
                        std::wcout << L"Expected an ID from tree and, for pulse, 1..30 seconds.\n"; continue;
                    }
                } else { usage(); continue; }
                if (verb != L"tree" && input >> extra) { std::wcout << L"Unexpected argument.\n"; continue; }
                std::wstring response;
                const auto hr = channel.call(request, response);
                if (log.is_open()) {
                    const auto entry = L"> " + line + L"\n" + response + L"\n";
                    const int size = WideCharToMultiByte(CP_UTF8, 0, entry.c_str(), static_cast<int>(entry.size()), nullptr, 0, nullptr, nullptr);
                    std::string utf8(size, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, entry.c_str(), static_cast<int>(entry.size()), utf8.data(), size, nullptr, nullptr);
                    log.write(utf8.data(), utf8.size()); log.flush();
                }
                std::wistringstream rows(response);
                std::wstring row;
                while (std::getline(rows, row)) if (verb != L"tree" || extra.empty() || row.find(extra) != std::wstring::npos) std::wcout << row << L'\n';
                std::wcout << L"HRESULT=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec << L'\n';
                if (hr == HRESULT_FROM_WIN32(ERROR_TIMEOUT)) break;
            }
            std::wstring restored;
            const auto hr = channel.call({Command::Restore, 0, 0}, restored);
            std::wcout << restored;
            if (FAILED(hr)) { std::wcerr << L"Restore not confirmed. The UI timer will retry; restart Dia if needed.\n"; exitCode = 1; }
            SetConsoleCtrlHandler(interrupt, FALSE);
            std::wcout << L"Session ending. Restart Dia before attaching another probe build.\n";
        }
    } catch (HRESULT hr) {
        std::wcerr << L"Probe failed during " << stage << L": 0x" << std::hex << static_cast<unsigned long>(hr) << L'\n';
        if (hr == E_ACCESSDENIED) std::wcerr << L"Access denied. The process was found, but the operation above was refused.\n";
        else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) std::wcerr << L"Close other probes and restart Dia before attaching again.\n";
        exitCode = 1;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; exitCode = 1; }
    if (SUCCEEDED(com)) CoUninitialize();
    return exitCode;
}
