#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace BuffPanel::Systems::BuffHud::LooseLayout {

inline constexpr std::string_view VirtualPath =
    "data/global/ui/layouts/buff-panel/BuffHudhd.json";
inline constexpr std::size_t MaximumBytes = 2u * 1024u * 1024u;

enum class Source { Embedded, ActiveMod, InvalidOverride };




struct GridRect final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

[[nodiscard]] bool ReadBuffGridRect(std::string_view bytes, GridRect& rect) noexcept;

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

[[nodiscard]] bool Validate(std::string_view bytes, std::string& error) noexcept;

}
