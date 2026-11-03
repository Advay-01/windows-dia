#include "session.h"
#include <set>
namespace dt {
void Session::set_mask(uint32_t mask) {
    std::lock_guard lock(featureMutex_);
    featureMask = mask;
    if (shared_) InterlockedExchange(&shared_->currentFeatureMask, static_cast<LONG>(mask));
}
void Session::set_feature(FeatureId id, bool on) {
    std::lock_guard lock(featureMutex_);
    const auto mask = on ? featureMask.load() | feature_bit(id) : featureMask.load() & ~feature_bit(id);
    featureMask = mask;
    if (shared_) InterlockedExchange(&shared_->currentFeatureMask, static_cast<LONG>(mask));
}
std::vector<Node> Session::nodes_on_thread(DWORD thread) {
    std::lock_guard lock(mutex_);
    std::vector<Node> result;
    for (const auto& [_, n] : nodes_) if (n.thread == thread) result.push_back(n);
    return result;
}
std::vector<Node> Session::sidebar_nodes(DWORD thread) {
    std::lock_guard lock(mutex_);
    std::vector<Node> result;
    for (const auto& [_, n] : nodes_) if (n.thread == thread &&
        n.type == L"Microsoft.UI.Xaml.Controls.Button" && n.name == L"SidebarToggleButton") result.push_back(n);
    return result;
}
bool Session::is_settings_navigation(uint64_t id) {
    std::lock_guard lock(mutex_);
    const Node* nav = nullptr;
    for (const auto& [_, n] : nodes_) if (n.id == id) nav = &n;
    if (!nav || nav->type != L"Microsoft.UI.Xaml.Controls.NavigationView") return false;
    std::set<std::wstring> names;
    for (const auto& [_, n] : nodes_) {
        if (n.thread != nav->thread || n.type != L"Microsoft.UI.Xaml.Controls.NavigationViewItem") continue;
        auto parent = n.parent;
        for (size_t depth = 0; parent && depth < 64; ++depth) {
            if (parent == nav->handle) { names.insert(n.name); break; }
            auto it = nodes_.find({nav->thread, parent});
            if (it == nodes_.end()) break;
            parent = it->second.parent;
        }
    }
    return names.contains(L"account") && names.contains(L"tabs") && names.contains(L"advanced");
}
void Context::sync_feature() {
#ifdef DIATWEAKS_SETTINGS
    if (!nativeFeatures) nativeFeatures = std::make_unique<NativeFeatures>(session->diagnostics.Get(), session->service.Get(), [s = session](FeatureId id, bool on) { s->set_feature(id, on); });
    const auto nodes = session->nodes_on_thread(thread);
    featureStates = nativeFeatures->tick(nodes, session->stopping ? 0 : session->featureMask.load());
    if (session->stopping) urlOverlay.reset();
    else {
        if (!urlOverlay) urlOverlay = std::make_unique<UrlOverlay>(session->diagnostics.Get(), window.load());
        urlOverlay->tick(nodes);
    }
    if (session->stopping) nativeFeatures->restore();
    else if (session->autoSettings) auto_settings();
#else
    const auto nodes = session->sidebar_nodes(thread);
    const bool enabled = requested(session->featureMask.load(), FeatureId::Sidebar) && !session->stopping;
    std::set<uint64_t> current;
    for (const auto& n : nodes) current.insert(n.id);
    for (auto it = featureOverrides.begin(); it != featureOverrides.end();) {
        if (!current.contains(it->first)) { it = featureOverrides.erase(it); continue; }
        if (!enabled) {
            if (SUCCEEDED(it->second.restore(session->service.Get()))) it = featureOverrides.erase(it);
            else ++it;
        } else ++it;
    }
    if (enabled) for (const auto& n : nodes) if (!featureOverrides.contains(n.id)) {
        VisibilityLease value;
        if (value.enable(session->service.Get(), n.handle) == S_OK) featureOverrides.emplace(n.id, std::move(value));
    }
    featureStates = {{FeatureId::Sidebar, !nodes.empty(), enabled, !featureOverrides.empty(), L"Probe-only sidebar adapter"}};
#endif
}
}
