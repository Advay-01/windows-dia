#pragma once
#include <string>
#include <vector>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
namespace dt
{
struct DiaTab
{
    std::wstring title, url;
    winrt::Microsoft::UI::Xaml::Media::ImageSource icon{nullptr};
    winrt::weak_ref<winrt::Microsoft::UI::Xaml::Automation::Peers::AutomationPeer> peer;
    int pattern = 0;
    winrt::Microsoft::UI::Xaml::Input::ICommand command{nullptr};
    winrt::Windows::Foundation::IInspectable parameter{nullptr};
    bool customModel = false;
    winrt::weak_ref<winrt::Microsoft::UI::Xaml::FrameworkElement> element;
};
std::vector<DiaTab> dia_tabs(const winrt::Microsoft::UI::Xaml::FrameworkElement &root);
bool select_dia_tab(const DiaTab &tab);
bool dia_tab_selected(const DiaTab &tab);
} // namespace dt
