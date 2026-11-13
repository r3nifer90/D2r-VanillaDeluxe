#pragma once
#include <cstdint>
namespace R3niBossSkills {
constexpr uint32_t Abi=1;
struct Skill {uint32_t id=0,level=30,kind=0,cooldown=750,refresh=0;};
struct Registration {uint32_t size=sizeof(Registration),abi=Abi;void* game=nullptr;void* unit=nullptr;uint64_t session=0;Skill skills[3];uint32_t reserved=0;};
using RegisterFn=bool(__cdecl*)(const Registration*) noexcept;
struct Api {uint32_t size,abi;RegisterFn registerBoss;};
}
