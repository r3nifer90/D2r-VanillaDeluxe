#pragma once
#include <cstdint>
#include <cstddef>
#include <limits>

namespace BuffPanel::Systems::PvpWard {
inline constexpr std::uint64_t BuffKey = 0x5233505650574152ULL;
inline constexpr std::uint32_t PacketMagic = 0x31575052;
inline constexpr std::uint64_t CompatibilityToken = 0x5233505650570001ULL;
inline constexpr std::size_t MaximumPlayers = 32;
inline constexpr std::size_t MaximumPeers = 32;
inline constexpr std::size_t PlayersPerPacket = 16;
inline constexpr std::uint32_t HostilityFlag = 8;
struct PlayerState { std::uint32_t id{}, active{}; };
struct Packet {
    std::uint32_t magic{PacketMagic}, sequence{}, reduction{}, count{};
    PlayerState players[PlayersPerPacket]{};
};
static_assert(sizeof(Packet) <= 224);
constexpr std::int32_t Scale(std::int32_t value, std::uint32_t reduction) noexcept {
    if (value <= 0) return value;
    if (reduction > 100) reduction = 100;
    return static_cast<std::int32_t>(static_cast<std::int64_t>(value) * (100-reduction) / 100);
}
constexpr std::int32_t ScaleNegative(std::int32_t value, std::uint32_t reduction) noexcept {
    if (value >= 0) return value;
    if (reduction > 100) reduction = 100;
    return static_cast<std::int32_t>(static_cast<std::int64_t>(value) * (100-reduction) / 100);
}
constexpr bool Hostile(std::uint32_t a, std::uint32_t b) noexcept { return ((a|b)&HostilityFlag)!=0; }
constexpr bool ValidPacket(const Packet& p, std::uint32_t size) noexcept {
    if(p.magic!=PacketMagic || p.count>PlayersPerPacket || p.reduction>100 || size!=16+p.count*sizeof(PlayerState)) return false;
    for(std::uint32_t i=0;i<p.count;++i) if(p.players[i].active>1) return false;
    return true;
}
}
