#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
namespace dt {
enum class FeatureId : uint32_t { Edge, Square, Toolbar, Hover, Sidebar, Chat, Chats, Recent, Active, Appearance, Count };
inline constexpr const wchar_t* FeatureKeys[] = {L"edgeToEdge", L"squareSidebarCorners", L"hideToolbar",
    L"revealToolbarOnHover", L"hideSidebarButton", L"hideChatButton", L"hideChatsButton", L"hideRecentTabsButton", L"keepWindowsActive", L"customAppearance"};
inline constexpr uint32_t feature_bit(FeatureId id) { return 1u << static_cast<uint32_t>(id); }
inline bool requested(uint32_t mask, FeatureId id) { return (mask & feature_bit(id)) != 0; }
inline bool effective(uint32_t mask, FeatureId id) {
    if (!requested(mask, id)) return false;
    if (id == FeatureId::Square) return requested(mask, FeatureId::Edge);
    if (id == FeatureId::Hover) return requested(mask, FeatureId::Toolbar);
    if (id == FeatureId::Sidebar || id == FeatureId::Chat) return !requested(mask, FeatureId::Toolbar);
    return true;
}
inline FeatureId feature_id(const std::wstring& key) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(FeatureId::Count); ++i) if (key == FeatureKeys[i]) return static_cast<FeatureId>(i);
    return FeatureId::Count;
}
struct FeatureState {
    FeatureId id;
    bool available = false, on = false, applied = false;
    std::wstring detail;
};
using FeatureStates = std::vector<FeatureState>;
using FeatureChanged = std::function<void(FeatureId, bool)>;
}
