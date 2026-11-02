#pragma once
#include "appearance.h"
namespace dt
{
class ThemesPanel : public std::enable_shared_from_this<ThemesPanel>
{
    FeatureChanged changed_;
    FeatureState state_{};
    bool updating_ = false;
    bool pending_ = false;
    uint64_t requestedAt_ = 0;
    winrt::weak_ref<winrt::Microsoft::UI::Xaml::FrameworkElement> anchor_;
    winrt::weak_ref<winrt::Microsoft::UI::Xaml::Controls::MenuFlyoutPresenter> menu_;
    std::wstring status_ = L"Waiting for Themes click";
    std::unique_ptr<AppearanceEditor> editor_;
    winrt::Microsoft::UI::Xaml::Controls::Flyout flyout_{nullptr};

  public:
    explicit ThemesPanel(FeatureChanged changed) : changed_(std::move(changed)) {}
    ~ThemesPanel();
    void show(const winrt::Microsoft::UI::Xaml::FrameworkElement &);
    void request_show(const winrt::Microsoft::UI::Xaml::FrameworkElement &,
                      const winrt::Microsoft::UI::Xaml::Controls::MenuFlyoutPresenter &);
    void sync(const FeatureState &);
    const std::wstring &status() const
    {
        return status_;
    }
};
} // namespace dt
