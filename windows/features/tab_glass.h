#pragma once
#include <memory>
#include <string>
#include <vector>
#include <winrt/Microsoft.UI.Xaml.h>
struct IXamlDiagnostics;
struct IVisualTreeService;
namespace dt
{
class TabGlass
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    TabGlass(IXamlDiagnostics *, IVisualTreeService *);
    ~TabGlass();
    void tick(const std::vector<winrt::Microsoft::UI::Xaml::XamlRoot> &, bool enabled);
    void restore();
    std::wstring status() const;
};
} // namespace dt
