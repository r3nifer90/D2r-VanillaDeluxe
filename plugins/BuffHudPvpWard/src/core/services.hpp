#pragma once

#include <D2RLPlugin/api.h>

namespace BuffPanel::Core {

struct ServiceRegistry final {
    const D2RL::PluginContext* context{};
    const D2RL::PanelService* panels{};
    const D2RL::CustomTableService* customTables{};
    const D2RL::LifecycleService* lifecycle{};
    const D2RL::ThreadService* threads{};
    const D2RL::WidgetService* widgets{};
    const D2RL::DataTableService* dataTables{};
    const D2RL::LocalizationService* localization{};
};

[[nodiscard]] bool InitializeServices(const D2RL::PluginContext* context) noexcept;
void ShutdownServices() noexcept;
[[nodiscard]] const ServiceRegistry& Services() noexcept;



[[nodiscard]] const D2RL::LocalizationService* LocalizationService() noexcept;
[[nodiscard]] const D2RL::DataTableService* DataTableService() noexcept;

}
