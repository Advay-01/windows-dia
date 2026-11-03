#include "session.h"
#include <vector>

namespace dt {
HRESULT Session::start(IUnknown* site) {
    check(site->QueryInterface(IID_PPV_ARGS(&diagnostics)));
    check(site->QueryInterface(IID_PPV_ARGS(&service)));
    Bstr initialization;
    check(diagnostics->GetInitializationData(&initialization.value));
    auto name = str(initialization.value);
    if (!name.starts_with(L"Local\\DiaTweaks.Prototype.") || name.size() > 80) return E_INVALIDARG;
    mapping_.value = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (!mapping_) return last_error();
    shared_ = static_cast<Shared*>(MapViewOfFile(mapping_.value, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
    if (!shared_) return last_error();
    if (shared_->version != ProtocolVersion || shared_->target != GetCurrentProcessId()) return E_INVALIDARG;
    if ((shared_->options & ~AutoSettings) || (shared_->initialFeatureMask >> static_cast<uint32_t>(FeatureId::Count))) return E_INVALIDARG;
    autoSettings = (shared_->options & AutoSettings) != 0;
    set_mask(shared_->initialFeatureMask);
    controller_.value = OpenProcess(SYNCHRONIZE, FALSE, shared_->controller);
    if (!controller_) return last_error();
    // Retain for the process lifetime: callbacks and message windows own pointers
    // into this session. Diagnostics can pin the TAP; forced unload is unsafe.
    AddRef();
    Handle worker(CreateThread(nullptr, 0, [](void* self) -> DWORD {
        static_cast<Session*>(self)->run(); return 0;
    }, this, 0, nullptr));
    if (!worker) { Release(); return last_error(); }
    return S_OK;
}
void Session::run() noexcept {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool advised = false;
    HRESULT result = S_OK;
    try {
        check(com);
        // Advising on a fresh worker avoids a diagnostics/UI-thread deadlock.
        check(service->AdviseVisualTreeChange(this));
        advised = true;
        shared_->startup = S_OK;
        InterlockedExchange(&shared_->ready, 1);
        while (!stopping && !read_flag(shared_->stop) && WaitForSingleObject(controller_.value, 25) == WAIT_TIMEOUT) {
            if (InterlockedCompareExchange(&shared_->phase, Working, Pending) != Pending) continue;
            std::wstring response;
            HRESULT hr = E_INVALIDARG;
            const auto request = shared_->request;
            try {
                if (request.command == Command::Tree) { response = tree(); hr = S_OK; }
                else if (request.command == Command::Inspect || request.command == Command::Pulse || request.command == Command::Settings || request.command == Command::Layout || request.command == Command::Overlay) {
                    auto n = node(request.handle);
                    Context* context = nullptr;
                    if (n) { std::lock_guard lock(mutex_); context = contexts_.at(n->thread).get(); }
                    hr = context ? dispatch(context, request, response) : HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
                } else if (request.command == Command::Restore || request.command == Command::SetSidebar || request.command == Command::SetFeature || request.command == Command::Features) {
                    if (request.command == Command::Restore) set_mask(0);
                    else if (request.command != Command::Features) {
                        if (request.command == Command::SetFeature && request.handle >= static_cast<uint64_t>(FeatureId::Count)) throw E_INVALIDARG;
                        const auto id = request.command == Command::SetSidebar ? FeatureId::Sidebar : static_cast<FeatureId>(request.handle);
                        if (id >= FeatureId::Count || request.seconds > 1) throw E_INVALIDARG;
#ifndef DIATWEAKS_SETTINGS
                        if (id != FeatureId::Sidebar) throw E_NOTIMPL;
#endif
                        set_feature(id, request.seconds != 0);
                    }
                    std::vector<Context*> contexts;
                    { std::lock_guard lock(mutex_); for (auto& [_, c] : contexts_) contexts.push_back(c.get()); }
                    hr = S_OK;
                    for (auto* c : contexts) { HRESULT one = dispatch(c, request, response); if (FAILED(one)) hr = one; }
                }
            } catch (HRESULT error) { hr = error; } catch (...) { hr = E_FAIL; }
            if (response.size() >= ResponseCapacity) {
                response.resize(ResponseCapacity - 100);
                response += L"\nTRUNCATED: response exceeds prototype buffer.\n";
                hr = HRESULT_FROM_WIN32(ERROR_MORE_DATA);
            }
            wcscpy_s(shared_->response, response.c_str());
            shared_->result = hr;
            InterlockedExchange(&shared_->phase, Complete);
        }
    } catch (HRESULT error) { result = error; } catch (...) { result = E_FAIL; }
    stopping = true;
    try {
        // Stop new tree notifications before taking the final context snapshot.
        if (advised) service->UnadviseVisualTreeChange(this);
        std::vector<Context*> contexts;
        { std::lock_guard lock(mutex_); for (auto& [_, c] : contexts_) contexts.push_back(c.get()); }
        for (auto* c : contexts) {
            std::wstring response;
            HRESULT hr = dispatch(c, {Command::Restore, 0, 0}, response);
            if (FAILED(hr)) result = hr;
        }
    } catch (...) { result = E_FAIL; }
    shared_->startup = result;
    InterlockedExchange(&shared_->ready, FAILED(result) ? -1 : 2);
    if (SUCCEEDED(com)) CoUninitialize();
}
}
