#include "themes_panel.h"
#include <sstream>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
ThemesPanel::~ThemesPanel()
{
    try
    {
        if (flyout_)
            flyout_.Hide();
    }
    catch (...)
    {
    }
}
void ThemesPanel::show(const FrameworkElement &anchor)
{
    if (flyout_)
        flyout_.Hide();
    auto weak = weak_from_this();
    editor_ = std::make_unique<AppearanceEditor>([weak](FeatureId id, bool on) {
        if (auto panel = weak.lock(); panel && !panel->updating_ && panel->changed_)
            panel->changed_(id, on);
    });
    sync(state_);
    flyout_ = Flyout{};
    flyout_.FlyoutPresenterStyle(
        Markup::XamlReader::Load(
            LR"(<Style xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" TargetType="FlyoutPresenter"><Setter Property="Padding" Value="0"/><Setter Property="BorderThickness" Value="0"/><Setter Property="BorderBrush" Value="Transparent"/><Setter Property="Background" Value="Transparent"/><Setter Property="CornerRadius" Value="28"/><Setter Property="MaxWidth" Value="400"/></Style>)")
            .as<Style>());
    ScrollViewer scroll;
    scroll.MaxHeight(620);
    scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Hidden);
    scroll.Content(editor_->root);
    flyout_.Content(scroll);
    flyout_.ShowAt(anchor);
}
void ThemesPanel::request_show(const FrameworkElement &anchor, const MenuFlyoutPresenter &menu)
{
    anchor_ = anchor ? make_weak(anchor) : weak_ref<FrameworkElement>{};
    menu_ = menu ? make_weak(menu) : weak_ref<MenuFlyoutPresenter>{};
    requestedAt_ = GetTickCount64();
    pending_ = true;
    status_ = L"Themes clicked; waiting for native menu to close";
}
void ThemesPanel::sync(const FeatureState &state)
{
    state_ = state;
    if (pending_ && GetTickCount64() - requestedAt_ >= 150)
    {
        auto menu = menu_.get();
        if (!menu || !menu.IsLoaded() || GetTickCount64() - requestedAt_ >= 750)
        {
            pending_ = false;
            try
            {
                auto anchor = anchor_.get();
                if (!anchor || !anchor.IsLoaded())
                    throw E_CHANGED_STATE;
                show(anchor);
                status_ = L"Themes panel shown";
            }
            catch (...)
            {
                std::wostringstream message;
                message << L"Themes open failed: 0x" << std::hex
                        << static_cast<uint32_t>(winrt::to_hresult());
                status_ = message.str();
            }
        }
    }
    if (!editor_)
        return;
    updating_ = true;
    try
    {
        if (editor_->enabled.IsOn() != state.on)
            editor_->enabled.IsOn(state.on);
        editor_->enabled.IsEnabled(state.available || state.on);
        editor_->status.Text(state.detail);
    }
    catch (...)
    {
        updating_ = false;
        throw;
    }
    updating_ = false;
}
} // namespace dt
