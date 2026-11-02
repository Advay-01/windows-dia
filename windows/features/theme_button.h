#pragma once
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
namespace dt
{
inline void theme_button(const winrt::Microsoft::UI::Xaml::Controls::Button &button)
{
    using namespace winrt::Microsoft::UI::Xaml;
    // One template per UI thread. Hover changes opacity on the compositor,
    // without replacing the selected background or changing control geometry.
    thread_local Controls::ControlTemplate shape = Markup::XamlReader::Load(LR"(
<ControlTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" TargetType="Button">
 <Grid x:Name="Root">
  <VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
   <VisualStateGroup.Transitions><VisualTransition GeneratedDuration="0:0:0.12"/></VisualStateGroup.Transitions>
   <VisualState x:Name="Normal"/>
   <VisualState x:Name="PointerOver"><Storyboard><DoubleAnimation Storyboard.TargetName="Hover" Storyboard.TargetProperty="Opacity" To="1" Duration="0"/></Storyboard></VisualState>
   <VisualState x:Name="Pressed"><Storyboard><DoubleAnimation Storyboard.TargetName="Hover" Storyboard.TargetProperty="Opacity" To="1" Duration="0"/></Storyboard></VisualState>
   <VisualState x:Name="Disabled"><Storyboard><DoubleAnimation Storyboard.TargetName="Root" Storyboard.TargetProperty="Opacity" To="0.4" Duration="0"/></Storyboard></VisualState>
  </VisualStateGroup></VisualStateManager.VisualStateGroups>
  <Border Background="{TemplateBinding Background}" CornerRadius="{TemplateBinding CornerRadius}"/>
  <Border x:Name="Hover" Opacity="0" Background="{ThemeResource ControlFillColorSecondaryBrush}" CornerRadius="{TemplateBinding CornerRadius}"/>
  <ContentPresenter Content="{TemplateBinding Content}" ContentTemplate="{TemplateBinding ContentTemplate}" Padding="{TemplateBinding Padding}" HorizontalContentAlignment="{TemplateBinding HorizontalContentAlignment}" VerticalContentAlignment="{TemplateBinding VerticalContentAlignment}"/>
 </Grid>
</ControlTemplate>)")
                                                       .as<Controls::ControlTemplate>();
    button.Template(shape);
}
} // namespace dt
