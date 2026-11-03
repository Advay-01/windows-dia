#include "url_glass.h"
#include <initguid.h>
#include <d2d1effects.h>
#include <windows.graphics.effects.interop.h>
#include <array>
#include <vector>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace dt
{
namespace
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Windows::Graphics::Effects;
namespace EffectsAbi = ABI::Windows::Graphics::Effects;
using Inspectable = winrt::Windows::Foundation::IInspectable;
using winrt::Windows::Foundation::IPropertyValue;
using winrt::Windows::Foundation::PropertyValue;

struct Effect : implements<Effect, IGraphicsEffect, IGraphicsEffectSource,
                           EffectsAbi::IGraphicsEffectD2D1Interop>
{
    GUID id;
    IGraphicsEffectSource source;
    std::vector<Inspectable> properties;
    hstring name;

    Effect(GUID id, IGraphicsEffectSource source, std::vector<Inspectable> properties)
        : id(id), source(std::move(source)), properties(std::move(properties))
    {
    }

    hstring Name() const { return name; }
    void Name(const hstring &value) { name = value; }

    HRESULT __stdcall GetEffectId(GUID *value) noexcept final
    {
        *value = id;
        return S_OK;
    }

    HRESULT __stdcall GetNamedPropertyMapping(
        LPCWSTR, UINT *, EffectsAbi::GRAPHICS_EFFECT_PROPERTY_MAPPING *) noexcept final
    {
        return E_INVALIDARG;
    }

    HRESULT __stdcall GetPropertyCount(UINT *count) noexcept final
    {
        *count = static_cast<UINT>(properties.size());
        return S_OK;
    }

    HRESULT __stdcall GetProperty(
        UINT index, ABI::Windows::Foundation::IPropertyValue **value) noexcept final
    {
        *value = nullptr;
        if (index >= properties.size())
            return E_BOUNDS;
        return properties[index].as(guid_of<IPropertyValue>(), reinterpret_cast<void **>(value));
    }

    HRESULT __stdcall GetSourceCount(UINT *count) noexcept final
    {
        *count = 1;
        return S_OK;
    }

    HRESULT __stdcall GetSource(
        UINT index, EffectsAbi::IGraphicsEffectSource **value) noexcept final
    {
        *value = nullptr;
        if (index != 0)
            return E_BOUNDS;
        copy_to_abi(source, *reinterpret_cast<void **>(value));
        return S_OK;
    }
};

struct BackdropBrush : Media::XamlCompositionBrushBaseT<BackdropBrush>
{
    CompositionEffectBrush effect;

    BackdropBrush(const CompositionEffectBrush &effect, winrt::Windows::UI::Color fallback)
        : effect(effect)
    {
        FallbackColor(fallback);
    }

    void OnConnected() { CompositionBrush(effect); }
    void OnDisconnected() { CompositionBrush(nullptr); }
};
} // namespace

Media::Brush frosted_backdrop(const Controls::Border &panel, winrt::Windows::UI::Color fallback,
                             double luminosityOpacity) noexcept
{
    try
    {
        using namespace winrt::Windows::UI::ViewManagement;
        if (!UISettings{}.AdvancedEffectsEnabled() || AccessibilitySettings{}.HighContrast() ||
            !CompositionCapabilities{}.AreEffectsFast())
            return nullptr;

        auto compositor = Hosting::ElementCompositionPreview::GetElementVisual(panel).Compositor();
        auto blur = make<Effect>(CLSID_D2D1GaussianBlur, CompositionEffectSourceParameter(L"Scene"),
                                 std::vector<Inspectable>{
                                     PropertyValue::CreateSingle(40.f),
                                     PropertyValue::CreateUInt32(D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED),
                                     PropertyValue::CreateUInt32(D2D1_BORDER_MODE_HARD)});

        // Preserve alpha and compress the brightness range before applying the tint.
        constexpr std::array luminance{.2126f, .7152f, .0722f};
        const float saturation = 1.20f - .20f * float(luminosityOpacity);
        const float brightness = .84f - .05f * float(luminosityOpacity);
        std::array<float, 20> matrix{};
        for (size_t input = 0; input < 3; ++input)
            for (size_t output = 0; output < 3; ++output)
                matrix[input * 4 + output] =
                    brightness * (luminance[input] * (1.f - saturation) +
                                  (input == output ? saturation : 0.f));
        matrix[15] = 1.f;
        matrix[16] = matrix[17] = matrix[18] = (1.f - brightness) * .43f;

        auto grade = make<Effect>(CLSID_D2D1ColorMatrix, blur.as<IGraphicsEffectSource>(),
                                  std::vector<Inspectable>{
                                      PropertyValue::CreateSingleArray(matrix),
                                      PropertyValue::CreateUInt32(D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED),
                                      PropertyValue::CreateBoolean(true)});
        auto effect = compositor.CreateEffectFactory(grade).CreateBrush();
        effect.SetSourceParameter(L"Scene", compositor.CreateBackdropBrush());
        return make<BackdropBrush>(effect, fallback).as<Media::Brush>();
    }
    catch (...)
    {
        return nullptr;
    }
}
} // namespace dt
