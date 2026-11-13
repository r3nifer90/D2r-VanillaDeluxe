#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>

namespace BuffPanel::Systems::BuffHud::Internal {

enum class IconFrameBackendState : std::uint8_t {
    Unavailable = 0,
    Ready = 1,
};





[[nodiscard]] bool InitializeIconFrameBackend(
    const D2RL::PluginContext* context) noexcept;
void ShutdownIconFrameBackend() noexcept;
[[nodiscard]] IconFrameBackendState IconFrameBackendStatus() noexcept;

[[nodiscard]] bool TryApplyIconFrame(
    void* buttonWidget,
    std::uint16_t requestedFrame,
    std::uint16_t declaredFrame) noexcept;

}
