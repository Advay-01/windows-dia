#pragma once
#include "feature_state.h"
#include "theme_model.h"
#include <memory>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
struct IXamlDiagnostics;
struct IVisualTreeService;
namespace dt
{
struct Node;
struct AppearanceEditor
{
    winrt::Microsoft::UI::Xaml::Controls::StackPanel root;
    winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch enabled;
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status;
    struct Impl;
    std::shared_ptr<Impl> impl;
    AppearanceEditor(FeatureChanged);
};
class AppearanceRuntime
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    AppearanceRuntime(IXamlDiagnostics *, IVisualTreeService *);
    ~AppearanceRuntime();
    void tick(const std::vector<Node> &, bool, FeatureState &);
    void restore();
    bool active() const;
};
struct AppearanceValues
{
    int palette = 0, opacity = 80, texture = 0;
    uint32_t color = 0x244c68;
    ThemeModel theme;
    bool preview = false;
    uint64_t revision = 1;
};
AppearanceValues appearance_values();
void change_appearance(int palette, int opacity, int texture, bool persist = true);
void change_theme(const ThemeModel &, bool persist = true);
} // namespace dt

namespace dt
{
void change_appearance_color(uint32_t rgb);
}
