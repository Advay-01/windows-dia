#pragma once
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace dt
{
// Returns null when the compositor or Windows transparency settings require
// the native acrylic fallback.
winrt::Microsoft::UI::Xaml::Media::Brush frosted_backdrop(
    const winrt::Microsoft::UI::Xaml::Controls::Border &panel,
    winrt::Windows::UI::Color fallback, double luminosityOpacity) noexcept;
} // namespace dt
