#pragma once
#include "appearance.h"
namespace dt
{
inline void finish_appearance_drag(const std::shared_ptr<bool> &dragging) noexcept
{
    if (!*dragging)
        return;
    *dragging = false;
    try
    {
        auto s = appearance_values();
        change_appearance(s.palette, s.opacity, s.texture);
    }
    catch (...)
    {
    }
}
} // namespace dt
