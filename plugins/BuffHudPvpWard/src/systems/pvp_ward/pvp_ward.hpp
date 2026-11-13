#pragma once
#include <D2RLPlugin/api.h>
#include "policy.hpp"
namespace BuffPanel::Systems::PvpWard {
bool Initialize(const D2RL::PluginContext*) noexcept;
void Shutdown() noexcept;
void ClearHudRect() noexcept;
void SetHudRect(D2RL::Widgets::Rect grid,D2RL::Widgets::Rect slot) noexcept;
}
