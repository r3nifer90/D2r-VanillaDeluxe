#pragma once
#include <cstdint>
namespace R3niDiagnostics {
constexpr uint32_t ObserverAbi=1,RecordBytes=128,SupportedState=0x10000,ConnectedState=0x100;
using Sink=void(__cdecl*)(const void*,uint32_t,uint16_t) noexcept;
struct Observer {uint32_t size=sizeof(Observer),abi=ObserverAbi,recordBytes=RecordBytes,reserved=0;Sink sink=nullptr;};
using GetObserver=const Observer*(__cdecl*)() noexcept;
using SpellState=uint32_t(__cdecl*)() noexcept;
static_assert(sizeof(Observer)==24);
}
