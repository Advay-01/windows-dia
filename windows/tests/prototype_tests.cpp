#include "fake_service.h"
#include "channel.h"
#include "runtime.h"
#include <future>
#include <iostream>
#include <filesystem>

using namespace dt;
using namespace dt::test;
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void dependency_tests() {
    const auto square = feature_bit(FeatureId::Square), edge = feature_bit(FeatureId::Edge);
    require(requested(square, FeatureId::Square) && !effective(square, FeatureId::Square), "child request waits for its parent");
    require(effective(square | edge, FeatureId::Square), "parent enables child without discarding its requested state");
    const auto hover = feature_bit(FeatureId::Hover), toolbar = feature_bit(FeatureId::Toolbar), sidebar = feature_bit(FeatureId::Sidebar);
    require(!effective(hover, FeatureId::Hover) && effective(hover | toolbar, FeatureId::Hover), "hover requires hidden toolbar");
    require(!effective(sidebar | toolbar, FeatureId::Sidebar) && effective(sidebar, FeatureId::Sidebar), "toolbar suppresses individual toggle and restores it afterwards");
    require(feature_id(L"not-a-patch") == FeatureId::Count, "unknown feature key rejected");
}
static void property_tests() {
    auto service = Microsoft::WRL::Make<FakeService>();
    VisibilityLease lease;
    require(lease.pulse(service.Get(), 42, 0) == E_INVALIDARG, "reject zero duration");
    require(lease.pulse(service.Get(), 42, 31) == E_INVALIDARG, "bound duration");
    require(lease.pulse(service.Get(), 42, 1) == S_OK && service->collapsed, "collapse default");
    require(FAILED(lease.pulse(service.Get(), 42, 1)), "refuse overlapping override");
    require(lease.restore(service.Get()) == S_OK && !service->local && service->clears == 1, "restore by clearing default override");
    service->local = true;
    require(lease.pulse(service.Get(), 42, 1) == S_OK, "collapse local literal");
    require(lease.restore(service.Get()) == S_OK && service->local && !service->collapsed, "restore local literal");
    service->numeric = true;
    require(lease.pulse(service.Get(), 42, 1) == S_OK && service->collapsed, "collapse numeric local visibility");
    require(lease.restore(service.Get()) == S_OK && service->local && !service->collapsed, "restore numeric local visibility");
    service->local = false;
    require(lease.pulse(service.Get(), 42, 1) == S_OK && service->collapsed, "collapse numeric default visibility");
    require(lease.restore(service.Get()) == S_OK && !service->local && !service->collapsed, "clear numeric default override");
    require(lease.enable(service.Get(), 42) == S_OK && !lease.expired(), "session override has no pulse deadline");
    require(lease.restore(service.Get()) == S_OK && !service->local, "session override restores source");
    service->local = false; service->metadata = IsValueBindingExpression;
    require(lease.pulse(service.Get(), 42, 1) == E_NOTIMPL, "preserve binding");
    const auto diagnostic = describe_visibility(service.Get(), 42);
    require(diagnostic.find(L"metadata=0x10") != std::wstring::npos &&
        diagnostic.find(L"Microsoft.UI.Xaml.Visibility") != std::wstring::npos, "describe refused visibility without changing it");
    service->metadata = 0; service->source = Animation;
    require(lease.pulse(service.Get(), 42, 1) == E_NOTIMPL, "preserve animation");
    service->source = BaseValueSourceDefault; service->type = L"UnexpectedEnum";
    require(lease.pulse(service.Get(), 42, 1) == E_NOTIMPL, "refuse unknown property type");
    service->type = L"Microsoft.UI.Xaml.Visibility"; service->collapsed = true;
    require(lease.pulse(service.Get(), 42, 1) == S_FALSE && !lease.active(), "leave existing collapsed state alone");
    service->collapsed = false; service->ignoreWrites = true;
    require(FAILED(lease.pulse(service.Get(), 42, 1)) && !lease.active(), "verify readback");
    service->ignoreWrites = false; service->failWriteOnce = true;
    require(FAILED(lease.pulse(service.Get(), 42, 1)) && !service->local, "rollback partially failed write");
    require(lease.pulse(service.Get(), 42, 1) == S_OK, "set before app update");
    service->collapsed = false;
    require(lease.restore(service.Get()) == S_FALSE && service->local, "preserve app's newer value");
    service->local = false;
    require(lease.pulse(service.Get(), 42, 1) == S_OK, "set before element removal");
    const int writes = service->sets;
    lease.removed(42);
    require(!lease.active() && lease.restore(service.Get()) == S_FALSE && writes == service->sets, "never write a removed handle");
}
static LRESULT CALLBACK fixture_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_APP + 1) {
        auto* callback = reinterpret_cast<IVisualTreeServiceCallback*>(lp);
        Bstr type(L"Microsoft.UI.Xaml.Controls.Button"), name(L"SidebarToggleButton");
        VisualElement element{};
        element.Handle = 42; element.Type = type.value; element.Name = name.value;
        callback->OnVisualTreeChange({0, 42, 0}, element, Add);
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
static void session_tests(bool background) {
    auto service = Microsoft::WRL::Make<FakeService>();
    service->numeric = true;
    WNDCLASSW wc{};
    wc.lpfnWndProc = fixture_proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"DiaTweaks.TestFixture";
    require(RegisterClassW(&wc) != 0, "register fixture");
    service->ui = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    require(service->ui != nullptr, "create fixture message window");
    wchar_t executable[32768]{};
    GetModuleFileNameW(nullptr, executable, 32768);
    auto dll = std::filesystem::path(executable).parent_path() / L"DiaTweaksTap.dll";
    HMODULE module = LoadLibraryW(dll.c_str());
    require(module != nullptr, "load real TAP DLL");
    using GetClass = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
    auto getClass = reinterpret_cast<GetClass>(GetProcAddress(module, "DllGetClassObject"));
    require(getClass != nullptr, "exported COM factory");
    ComPtr<IClassFactory> factory;
    check(getClass(TapClsid, IID_PPV_ARGS(&factory)));
    ComPtr<IObjectWithSite> tap;
    check(factory->CreateInstance(nullptr, IID_PPV_ARGS(&tap)));
    auto future = std::async(std::launch::async, [&] {
        check(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        {
            Channel channel(GetCurrentProcessId());
            if (background) { channel.shared->options = AutoSettings; channel.shared->initialFeatureMask = feature_bit(FeatureId::Sidebar); }
            service->initialization = channel.name;
            check(tap->SetSite(static_cast<IVisualTreeService*>(service.Get())));
            const auto deadline = GetTickCount64() + 5000;
            while (!read_flag(channel.shared->ready) && GetTickCount64() < deadline) Sleep(10);
            require(read_flag(channel.shared->ready) == 1, "session handshake");
            if (background) {
                std::wstring response;
                require(channel.call({Command::Features, 0, 0}, response) == S_OK && service->collapsed, "background startup applies saved switches");
                require(static_cast<uint32_t>(read_flag(channel.shared->currentFeatureMask)) == feature_bit(FeatureId::Sidebar), "publish requested switches for persistence");
                require(SUCCEEDED(channel.call({Command::Restore, 0, 0}, response)) && !service->collapsed, "background reset restores baseline");
            }
            bool duplicateRejected = false;
            try { Channel duplicate(GetCurrentProcessId()); }
            catch (HRESULT hr) { duplicateRejected = hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS); }
            require(duplicateRejected, "one controller per target");
            std::wstring response;
            require(SUCCEEDED(channel.call({Command::Tree, 0, 0}, response)) && response.find(L"SidebarToggleButton") != std::wstring::npos, "tree roundtrip");
            require(channel.call({Command::Inspect, 1, 0}, response) == S_OK && response.find(L"Visible") != std::wstring::npos, "inspect on owner thread");
            require(FAILED(channel.call({Command::Overlay, 999, 0}, response)), "reject stale overlay target");
            require(FAILED(channel.call({Command::Overlay, 1, 0}, response)) && !service->collapsed, "overlay rejects a non-browser target without mutations");
            require(FAILED(channel.call({Command::Pulse, 999, 1}, response)), "reject stale ID");
            require(channel.call({Command::Pulse, 1, 1}, response) == S_OK && service->collapsed, "pulse roundtrip");
            const auto restoredBy = GetTickCount64() + 2500;
            while (service->collapsed && GetTickCount64() < restoredBy) Sleep(25);
            require(!service->collapsed && !service->local, "timer restores original source");
            require(channel.call({Command::Pulse, 1, 30}, response) == S_OK && service->collapsed, "long lease before disconnect");
            require(SUCCEEDED(channel.call({Command::Restore, 0, 0}, response)) && !service->collapsed, "restore long pulse");
            require(channel.call({Command::SetSidebar, 0, 1}, response) == S_OK && service->collapsed, "enable session feature through IPC");
            require(channel.call({Command::SetSidebar, 0, 0}, response) == S_OK && !service->collapsed && !service->local, "disable feature restores source");
            require(channel.call({Command::SetFeature, UINT64_MAX, 1}, response) == E_INVALIDARG && !service->collapsed, "reject overflowing feature id without mutations");
            require(channel.call({Command::SetFeature, (1ULL << 32) + static_cast<uint64_t>(FeatureId::Sidebar), 1}, response) == E_INVALIDARG && !service->collapsed, "reject feature id before narrowing to its enum");
            require(channel.call({Command::SetFeature, static_cast<uint64_t>(FeatureId::Sidebar), 1}, response) == S_OK && service->collapsed, "generic feature IPC enables sidebar");
            require(static_cast<uint32_t>(read_flag(channel.shared->currentFeatureMask)) == feature_bit(FeatureId::Sidebar), "publish UI/IPC switch state");
            require(channel.call({Command::SetFeature, static_cast<uint64_t>(FeatureId::Sidebar), 0}, response) == S_OK && !service->collapsed, "generic feature IPC restores sidebar");
            require(channel.call({Command::SetSidebar, 0, 1}, response) == S_OK && service->collapsed, "leave feature on before disconnect");
        }
        require(!service->collapsed && !service->local, "controller disconnect restores");
        require(!service->wrongThread, "all property calls stay on UI thread");
        CoUninitialize();
    });
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        MsgWaitForMultipleObjectsEx(0, nullptr, 10, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    DestroyWindow(service->ui);
    future.get();
    check(tap->SetSite(nullptr));
    // Like diagnostics, retain the module until process exit, even after stop.
}
int wmain(int argc, wchar_t** argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        if (argc == 3 && wcscmp(argv[1], L"--runtime") == 0) {
            RuntimeLibrary runtime(argv[2]);
            require(runtime.symbol("InitializeXamlDiagnosticsEx") != nullptr, "diagnostics export");
            std::cout << "PASS: diagnostics runtime loads; temporary staging=" << runtime.staged() << "; no target attachment attempted\n";
        } else {
            dependency_tests(); property_tests(); session_tests(argc == 2 && wcscmp(argv[1], L"--background") == 0);
            std::cout << "PASS: restore policy, failure rollback, IPC, UI-thread dispatch, timer and disconnect\n";
        }
    } catch (HRESULT hr) { std::cerr << "HRESULT failure: " << std::hex << hr << '\n'; return 1; }
      catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    CoUninitialize();
    return 0;
}
