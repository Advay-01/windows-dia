#pragma once
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
namespace dt
{
inline auto url_vector_icon(const winrt::Microsoft::UI::Xaml::Media::Brush &stroke, bool search)
{
    using namespace winrt::Microsoft::UI::Xaml;
    Controls::Canvas canvas;
    canvas.Width(search ? 20 : 16);
    canvas.Height(search ? 20 : 16);
    auto line = [&](double x1, double y1, double x2, double y2) {
        Shapes::Line shape;
        shape.X1(x1);
        shape.Y1(y1);
        shape.X2(x2);
        shape.Y2(y2);
        shape.Stroke(stroke);
        shape.StrokeThickness(1.7);
        canvas.Children().Append(shape);
    };
    if (search)
    {
        Shapes::Ellipse circle;
        circle.Width(14);
        circle.Height(14);
        circle.Stroke(stroke);
        circle.StrokeThickness(1.7);
        Controls::Canvas::SetLeft(circle, 1);
        Controls::Canvas::SetTop(circle, 1);
        canvas.Children().Append(circle);
        line(13, 13, 20, 20);
    }
    else
    {
        line(2, 8, 14, 8);
        line(9, 3, 14, 8);
        line(14, 8, 9, 13);
    }
    return canvas;
}
} // namespace dt
