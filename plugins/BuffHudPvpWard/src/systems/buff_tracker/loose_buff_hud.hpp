#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace BuffPanel::Systems::BuffTracker::LooseBuffHud {

inline constexpr std::string_view VirtualPath =
    "data/global/excel/d2rloader/buff-panel/buff-hud.txt";
inline constexpr std::size_t MaximumBytes = 1024u * 1024u;

enum class Source { Embedded, ActiveMod, InvalidOverride };

struct Selection final {
    Source source{Source::Embedded};
    std::filesystem::path path{};
    std::string bytes{};
    std::string error{};
};





[[nodiscard]] Selection Select(
    const wchar_t* modDirectory,
    const char* activeMod,
    std::string_view embedded) noexcept;

[[nodiscard]] bool ValidateAndNormalize(std::string& bytes, std::string& error) noexcept;

}
