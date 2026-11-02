#pragma once
#include <functional>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
namespace dt
{
void preview_appearance_opacity(double value) noexcept;
winrt::Microsoft::UI::Xaml::Controls::Grid appearance_opacity(std::function<void()> changed);
winrt::Microsoft::UI::Xaml::Controls::StackPanel appearance_texture(std::function<void()> changed);
} // namespace dt
