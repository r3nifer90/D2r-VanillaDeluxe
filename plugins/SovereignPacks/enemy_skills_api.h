#pragma once


namespace R3niEnemySkills {
constexpr uint32_t Abi=1;
enum class Role:uint32_t { SovereignLeader=1, TerrorBoss=2 };
struct Registration {
 uint32_t size=sizeof(Registration),abi=Abi;
 void* game=nullptr;
 void* unit=nullptr;
 uint64_t session=0;
 Role role=Role::SovereignLeader;
 uint32_t reserved=0;
};
static_assert(sizeof(Registration)==40);
using RegisterFn=bool(__cdecl*)(const Registration*) noexcept;


}
