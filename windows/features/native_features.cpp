#include "native_features_impl.h"
#include <set>
#include <algorithm>
namespace dt {
using namespace winrt::Microsoft::UI::Xaml;
FeatureId NativeFeatures::Impl::button_role(const Node& n) {
    if (n.type == L"Microsoft.UI.Xaml.Controls.Button" && n.name == L"SidebarToggleButton") return FeatureId::Sidebar;
    if (n.type == L"Microsoft.UI.Xaml.Controls.ContentControl" && n.name == L"ChatButtonHost") return FeatureId::Chat;
    if (n.type != L"Microsoft.UI.Xaml.Controls.Button") return FeatureId::Count;
    // Compare against known control roles only. Never expose tab names, URLs or
    // arbitrary accessible names in logs or use a substring as a selector.
    auto e = element(n.handle);
    bool chrome = false;
    for (auto p = Media::VisualTreeHelper::GetParent(e); p; p = Media::VisualTreeHelper::GetParent(p)) {
        if (auto ancestor = p.try_as<FrameworkElement>()) {
            const auto name = ancestor.Name();
            // Tab titles are user content, even if a tab happens to be called
            // Chats. Restrict accessible-name matching to chrome controls.
            if (name == L"TabListPresenter" || name == L"SpacePagesPanel") return FeatureId::Count;
            if (name == L"SidebarPresenter" || name == L"NavigationChromeHost" || name == L"TabStripInset") chrome = true;
        }
    }
    if (!chrome) return FeatureId::Count;
    const auto label = Automation::AutomationProperties::GetName(e);
    if (label == L"Recent tabs" || label == L"Search tabs" || label == L"Tab search") return FeatureId::Recent;
    if (label == L"Chats" || label == L"Chat history" || label == L"Search chats") return FeatureId::Chats;
    return FeatureId::Count;
}
NativeFeatures::NativeFeatures(IXamlDiagnostics* d, IVisualTreeService* s, FeatureChanged changed) : impl_(std::make_unique<Impl>(d, s)) { impl_->changed = std::move(changed); }
NativeFeatures::~NativeFeatures() = default;
bool NativeFeatures::active() const { return !impl_->hidden.empty() || (impl_->layout && impl_->layout->active()) || (impl_->appearance && impl_->appearance->active()) || (impl_->theme && impl_->theme->active()); }
void NativeFeatures::restore() {
    if(impl_->theme)impl_->theme->restore();
    if(impl_->themesMenu)impl_->themesMenu->remove();
    if (impl_->layout) impl_->layout->restore();
    if (impl_->appearance) impl_->appearance->restore();
    for (auto it = impl_->hidden.begin(); it != impl_->hidden.end();) {
        if (SUCCEEDED(it->second.restore(impl_->service.Get()))) it = impl_->hidden.erase(it); else ++it;
    }
}
FeatureStates NativeFeatures::tick(const std::vector<Node>& nodes, uint32_t mask) {
    FeatureStates states;
    for (uint32_t i = 0; i < static_cast<uint32_t>(FeatureId::Count); ++i)
        states.push_back({static_cast<FeatureId>(i), false, requested(mask, static_cast<FeatureId>(i)), false, L"Windows adapter discovery pending"});
    if (!impl_->layout) impl_->layout = std::make_unique<LayoutFeatures>(impl_->diagnostics.Get(), impl_->service.Get());
    impl_->layout->tick(nodes, mask, states);
    if(!impl_->theme)impl_->theme=std::make_unique<AppearanceRuntime>(impl_->diagnostics.Get(),impl_->service.Get());
    impl_->theme->tick(nodes,effective(mask,FeatureId::Appearance),states[static_cast<size_t>(FeatureId::Appearance)]);
    if (!impl_->themesMenu) impl_->themesMenu = std::make_unique<ThemesMenu>(impl_->diagnostics.Get(), impl_->changed);
    impl_->themesMenu->tick(nodes, states[static_cast<size_t>(FeatureId::Appearance)]);
    if (!impl_->appearance) impl_->appearance = std::make_unique<WindowAppearance>(impl_->diagnostics.Get());
    // A mock/base diagnostics tree need not host a real WinUI window.
    if (impl_->appearance->active() || std::any_of(nodes.begin(), nodes.end(), [](const Node& n) { return n.name == L"SidebarBorder"; }))
        impl_->appearance->tick(nodes, effective(mask, FeatureId::Active), states[static_cast<size_t>(FeatureId::Active)]);
    uint32_t activeMask = mask;
    if (!states[static_cast<size_t>(FeatureId::Toolbar)].applied) activeMask &= ~feature_bit(FeatureId::Toolbar);
    std::set<uint64_t> surviving;
    for (const auto& n : nodes) surviving.insert(n.id);
    for (auto it = impl_->hidden.begin(); it != impl_->hidden.end();)
        if (!surviving.contains(it->first)) it = impl_->hidden.erase(it); else ++it;
    for (auto it = impl_->roles.begin(); it != impl_->roles.end();)
        if (!surviving.contains(it->first)) it = impl_->roles.erase(it); else ++it;
    for (const auto& n : nodes) {
        FeatureId id = FeatureId::Count;
        try {
            auto [it, inserted] = impl_->roles.try_emplace(n.id, FeatureId::Count);
            if (inserted) it->second = impl_->button_role(n);
            id = it->second;
        } catch (...) { continue; }
        if (id == FeatureId::Count) continue;
        auto& state = states[static_cast<size_t>(id)];
        Visibility value;
        const auto readable = read_visibility(impl_->service.Get(), n.handle, value);
        if (FAILED(readable)) { state.detail = L"Control found; its Visibility source is unsupported"; continue; }
        state.available = true; state.detail = L"Experimental Windows adapter · this session only";
        auto it = impl_->hidden.find(n.id);
        if (effective(activeMask, id) && it == impl_->hidden.end()) {
            VisibilityLease lease;
            const auto hr = lease.enable(impl_->service.Get(), n.handle);
            if (hr == S_OK) impl_->hidden.emplace(n.id, std::move(lease));
            else if (FAILED(hr)) state.detail = L"Could not apply override; original value retained";
            else state.detail = L"This control is already hidden by Dia in the current tab";
        } else if (!effective(activeMask, id) && it != impl_->hidden.end()) {
            if (SUCCEEDED(it->second.restore(impl_->service.Get()))) impl_->hidden.erase(it);
            else state.detail = L"Restoration pending; retrying";
        }
        state.applied = state.applied || impl_->hidden.contains(n.id);
    }
    for (const auto id : {FeatureId::Chat, FeatureId::Chats, FeatureId::Recent}) {
        auto& state = states[static_cast<size_t>(id)];
        if (!state.available && state.detail == L"Windows adapter discovery pending") state.detail = L"Matching control not found in this Windows layout";
    }
    return states;
}
}
