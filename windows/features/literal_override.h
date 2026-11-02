#pragma once
#include "visibility.h"
#include <memory>
#include <type_traits>
#include <vector>
#include <winrt/Microsoft.UI.Xaml.h>
namespace dt
{
// UI-thread transaction for one literal XAML property. Preserve expressions and
// application updates; restore the original local value or clear our override.
class OverrideBase
{
  public:
    virtual ~OverrideBase() = default;
    virtual void restore() = 0;
};
template <class T> class LiteralOverride final : public OverrideBase
{
    using Object = winrt::Microsoft::UI::Xaml::DependencyObject;
    using Property = winrt::Microsoft::UI::Xaml::DependencyProperty;
    Object object_{nullptr};
    Property property_{nullptr};
    winrt::Windows::Foundation::IInspectable original_{nullptr};
    T expected_ = [] {
        if constexpr (std::is_convertible_v<T, winrt::Windows::Foundation::IInspectable>)
            return T{nullptr};
        else
            return T{};
    }();
    bool applied_ = false;
    static constexpr bool objectValue =
        std::is_convertible_v<T, winrt::Windows::Foundation::IInspectable>;
    bool ours() const
    {
        const auto value = object_.ReadLocalValue(property_);
        if (value == Property::UnsetValue())
            return false;
        if constexpr (objectValue)
            return value.try_as<T>() == expected_;
        else
            return winrt::unbox_value<T>(value) == expected_;
    }

  public:
    LiteralOverride(IXamlDiagnostics *diagnostics, IVisualTreeService *service,
                    const Object &object, const Property &property, const wchar_t *name)
        : object_(object), property_(property)
    {
        original_ = object.ReadLocalValue(property);
        if (original_ != Property::UnsetValue())
        {
            if constexpr (objectValue)
            {
                if (original_ && !original_.try_as<T>())
                    throw E_NOTIMPL;
            }
            else
                (void)winrt::unbox_value<T>(original_);
        }
        InstanceHandle handle;
        check(diagnostics->GetHandleFromIInspectable(
            reinterpret_cast<::IInspectable *>(winrt::get_abi(object)), &handle));
        PropertyValues values;
        check(values.read(service, handle));
        bool found = false;
        for (unsigned i = 0; i < values.valuesCount; ++i)
        {
            const auto &v = values.values[i];
            const auto key = str(v.PropertyName);
            if ((key != name && key != L"Grid." + std::wstring(name) &&
                 key != L"Canvas." + std::wstring(name)) ||
                v.Overridden)
                continue;
            constexpr unsigned allowed = objectValue ? IsValueHandle | IsValueNull : 0;
            if ((v.MetadataBits & ~allowed) || v.PropertyChainIndex >= values.sourcesCount)
                throw E_NOTIMPL;
            const auto origin = values.sources[v.PropertyChainIndex].Source;
            if (origin != BaseValueSourceDefault && origin != BaseValueSourceBuiltInStyle &&
                origin != BaseValueSourceStyle && origin != BaseValueSourceLocal)
                throw E_NOTIMPL;
            found = true;
        }
        if (!found)
            throw E_NOTIMPL;
    }
    ~LiteralOverride()
    {
        try
        {
            restore();
        }
        catch (...)
        {
        }
    }
    void apply(const T &value)
    {
        if (applied_ && !ours())
            throw E_CHANGED_STATE;
        expected_ = value;
        applied_ = true;
        object_.SetValue(property_, winrt::box_value(value));
        if constexpr (objectValue)
        {
            if (object_.GetValue(property_).try_as<T>() != value)
                throw E_FAIL;
        }
        else if (winrt::unbox_value<T>(object_.GetValue(property_)) != value)
            throw E_FAIL;
    }
    void restore() override
    {
        if (!applied_)
            return;
        bool owned = false;
        try
        {
            owned = ours();
        }
        catch (...)
        {
            applied_ = false;
            return;
        }
        if (owned)
        {
            if (original_ == Property::UnsetValue())
                object_.ClearValue(property_);
            else
                object_.SetValue(property_, original_);
        }
        applied_ = false;
    }
};
class OverrideSet
{
    IXamlDiagnostics *diagnostics_;
    IVisualTreeService *service_;
    std::vector<std::unique_ptr<OverrideBase>> values_;

  public:
    OverrideSet(IXamlDiagnostics *d, IVisualTreeService *s) : diagnostics_(d), service_(s) {}
    ~OverrideSet()
    {
        try
        {
            restore();
        }
        catch (...)
        {
        }
    }
    template <class T>
    LiteralOverride<T> &add(const winrt::Microsoft::UI::Xaml::DependencyObject &object,
                            const winrt::Microsoft::UI::Xaml::DependencyProperty &property,
                            const wchar_t *name, const T &value)
    {
        auto lease =
            std::make_unique<LiteralOverride<T>>(diagnostics_, service_, object, property, name);
        auto &result = *lease;
        values_.push_back(std::move(lease));
        result.apply(value);
        return result;
    }
    void restore()
    {
        for (auto it = values_.rbegin(); it != values_.rend(); ++it)
            (*it)->restore();
        values_.clear();
    }
    bool empty() const
    {
        return values_.empty();
    }
};
} // namespace dt
