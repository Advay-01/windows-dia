#include "appearance_controls.h"
#include "appearance_editor_impl.h"
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.Foundation.Collections.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace Controls;
using namespace Media;
void AppearanceEditor::Impl::activate()
{
    if (changed)
        changed(FeatureId::Appearance, appearance_values().theme.count > 0);
}
void AppearanceEditor::Impl::apply(ThemeModel m, bool animate)
{
    sanitize_theme(m);
    change_theme(m, !dragging);
    activate();
    render(animate);
}
AppearanceEditor::AppearanceEditor(FeatureChanged changed)
{
    root.Name(L"DiaTweaksThemesPanel");
    root.Width(380);
    root.RequestedTheme(ElementTheme::Dark);
    impl = std::make_shared<Impl>(changed);
    auto weak = std::weak_ptr<Impl>(impl);
    Border frame;
    impl->surface = root;
    impl->frame = frame;
    frame.Padding({10, 10, 10, 10});
    // The inner radius is 18 with a 10-unit inset: keep corner centers aligned.
    frame.CornerRadius({28, 28, 28, 28});
    frame.Background(SolidColorBrush(winrt::Windows::UI::Color{255, 31, 33, 33}));
    StackPanel inner;
    inner.Spacing(4);
    frame.Child(inner);
    root.Children().Append(frame);
    impl->canvas();
    Border surface;
    surface.CornerRadius({18, 18, 18, 18});
    surface.Child(impl->board);
    inner.Children().Append(surface);
    impl->palette(inner);
    Grid bottom;
    bottom.Width(360);
    bottom.Height(76);
    ColumnDefinition left, gap, right;
    left.Width({250, GridUnitType::Pixel});
    gap.Width({34, GridUnitType::Pixel});
    right.Width({76, GridUnitType::Pixel});
    bottom.ColumnDefinitions().Append(left);
    bottom.ColumnDefinitions().Append(gap);
    bottom.ColumnDefinitions().Append(right);
    auto activate = [weak] {
        if (auto s = weak.lock())
            s->activate();
    };
    bottom.Children().Append(appearance_opacity(activate));
    auto dial = appearance_texture(activate);
    Grid::SetColumn(dial, 2);
    bottom.Children().Append(dial);
    inner.Children().Append(bottom);
    impl->render();
}
} // namespace dt
