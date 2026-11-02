#pragma once
#include "native_features.h"
#include "session.h"
#include "layout_features.h"
#include "window_appearance.h"
#include "appearance.h"
#include "themes_menu.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <map>
namespace dt {
struct NativeFeatures::Impl {
    ComPtr<IXamlDiagnostics> diagnostics;
    ComPtr<IVisualTreeService> service;
    std::map<uint64_t, VisibilityLease> hidden;
    std::map<uint64_t, FeatureId> roles;
    std::unique_ptr<LayoutFeatures> layout;
    std::unique_ptr<WindowAppearance> appearance;
    std::unique_ptr<AppearanceRuntime> theme;
    std::unique_ptr<ThemesMenu> themesMenu;
    FeatureChanged changed;
    Impl(IXamlDiagnostics* d, IVisualTreeService* s) : diagnostics(d), service(s) {}
    winrt::Microsoft::UI::Xaml::FrameworkElement element(InstanceHandle h) {
        ComPtr<::IInspectable> raw;
        check(diagnostics->GetIInspectableFromHandle(h, &raw));
        winrt::Windows::Foundation::IInspectable object{nullptr};
        winrt::copy_from_abi(object, raw.Get());
        return object.try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>();
    }
    FeatureId button_role(const Node&);
};
}
