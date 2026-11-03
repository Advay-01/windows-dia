#pragma once
#include "protocol.h"
#include "visibility.h"
#include "feature_state.h"
#include <ocidl.h>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#ifdef DIATWEAKS_SETTINGS
#include "native_settings.h"
#include "native_features.h"
#include "url_overlay.h"
#endif

namespace dt {
struct Node { uint64_t id; InstanceHandle handle, parent; DWORD thread; std::wstring type, name; };
struct Job {
    Request request{};
    Handle done{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    std::atomic<bool> cancelled{false};
    ULONGLONG deadline = GetTickCount64() + 5000;
    HRESULT result = E_PENDING;
    std::wstring response;
};
class Session;
struct Context {
    Session* session;
    DWORD thread;
    std::atomic<HWND> window{nullptr};
    VisibilityLease lease;
    std::map<uint64_t, VisibilityLease> featureOverrides;
    FeatureStates featureStates;
#ifdef DIATWEAKS_SETTINGS
    std::unique_ptr<NativeSettings> settings;
    std::unique_ptr<NativeFeatures> nativeFeatures;
    std::unique_ptr<UrlOverlay> urlOverlay;
    uint64_t settingsId = 0;
    ULONGLONG nextSettingsAttempt = 0;
    std::wstring settingsError;
    void auto_settings();
#endif
    void sync_feature();
    static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
    void execute(Job&);
};
class Session final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IVisualTreeServiceCallback2, Microsoft::WRL::FtmBase> {
    Handle mapping_, controller_;
    Shared* shared_ = nullptr;
    std::mutex mutex_;
    std::mutex featureMutex_;
    std::map<std::pair<DWORD, InstanceHandle>, Node> nodes_;
    std::map<DWORD, std::unique_ptr<Context>> contexts_;
    uint64_t nextId_ = 1;
    bool overflow_ = false;
    void run() noexcept;
    HRESULT dispatch(Context*, Request, std::wstring&);
    void set_mask(uint32_t);
public:
    ~Session() { if (shared_) UnmapViewOfFile(shared_); }
    ComPtr<IVisualTreeService> service;
    ComPtr<IXamlDiagnostics> diagnostics;
    std::atomic<uint32_t> featureMask{0};
    bool autoSettings = false;
    std::atomic<bool> stopping{false};
    HRESULT start(IUnknown*);
    HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation, VisualElement, VisualMutationType) noexcept override;
    HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle, VisualElementState, LPCWSTR) noexcept override { return S_OK; }
    std::optional<Node> node(uint64_t);
    std::wstring tree();
    std::vector<Node> sidebar_nodes(DWORD thread);
    std::vector<Node> nodes_on_thread(DWORD thread);
    bool is_settings_navigation(uint64_t id);
    void set_feature(FeatureId, bool);
};
}
