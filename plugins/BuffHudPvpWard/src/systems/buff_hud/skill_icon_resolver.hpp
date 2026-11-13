#pragma once

#include "core/buff_display_bus.hpp"

#include <cstddef>
#include <cstdint>

namespace BuffPanel::Systems::BuffHud::Internal {

struct SkillIconDescriptor final {
    Core::BuffIconAtlas atlas{Core::BuffIconAtlas::Global};
    std::uint16_t frame{};
};

struct SkillIconResolverStatus final {
    bool ready{};
    bool namesReady{};
    std::uint32_t bank{};
    std::uint32_t skillsRowSize{};
    std::uint32_t skillDescRowSize{};
    std::uint32_t skillCount{};
    std::uint32_t skillDescCount{};
    std::uint32_t linkCandidateCount{};
    std::uint32_t classCandidateCount{};
    std::uint32_t nameCandidateCount{};
    std::uint32_t skillDescLinkOffset{};
    std::uint32_t skillClassOffset{};
    std::uint32_t iconCelOffset{};
    std::uint32_t skillNameStringIdOffset{};
    std::uint64_t tableRevision{};
};






[[nodiscard]] bool RebuildSkillIconCache(std::uint64_t tableRevision) noexcept;
void ResetSkillIconCache() noexcept;
[[nodiscard]] bool TryResolveSkillIcon(
    std::int32_t skillId,
    SkillIconDescriptor& descriptor) noexcept;
[[nodiscard]] bool TryResolveSkillName(
    std::int32_t skillId,
    char* output,
    std::size_t outputSize) noexcept;
[[nodiscard]] SkillIconResolverStatus SkillIconStatus() noexcept;

}
