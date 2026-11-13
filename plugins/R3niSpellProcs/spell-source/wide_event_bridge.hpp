#pragma once
#include <cstdint>

namespace ruffneckk::cast_triggers::wide_events {







using Effect = int(*)(void*, int, void*, void*, void*, std::uint64_t, int, int, void**);
using Invoke = int(*)(void*, void**, int*, void**, void**, void**,
    std::uint64_t*, int*, int*, void**, Effect*);
using Before = bool(*)(void*, int, void*, void*, void*&, std::uint64_t, Effect);
using After = void(*)(void*, void*, void*, void*, Effect, int);
struct Table { void* destroy{}; Invoke invoke{}; };
struct Bridge {
    const Table* table;
    void* previous;
    Before before;
    After after;

    static int Call(void* object, void** game, int* event, void** owner,
            void** target, void** damage, std::uint64_t* key, int* a7,
            int* a8, void** context, Effect* effect) {
        auto& self = *static_cast<Bridge*>(object);
        void* localDamage = *damage;
        if (self.before && !self.before(*game, *event, *owner, *target,
                localDamage, *key, *effect)) return 0;
        int result;
        auto* previous = self.previous;
        std::uintptr_t table = previous
            ? *static_cast<std::uintptr_t*>(previous) : 0;
        if (table & 1) {
            previous = reinterpret_cast<void*>(table & ~std::uintptr_t{1});
            table = previous ? *static_cast<std::uintptr_t*>(previous) : 0;
        }
        if (table) {
            result = reinterpret_cast<const Table*>(table)->invoke(previous,
                game, event, owner, target, &localDamage, key, a7, a8, context, effect);
        } else {
            SPD::Span diagnostic(SPD::Effect,*game,*owner,*target,localDamage,0,0,*key);
            diagnostic.Mark(SPD::NativeBegin,reinterpret_cast<uintptr_t>(*effect));
            SPD::AuditSeed seed(*owner,*target);
            result = (*effect)(*game, *event, *owner, *target, localDamage,
                *key, *a7, *a8, context);
            diagnostic.Mark(SPD::NativeEnd,static_cast<uint32_t>(result));
            SPD::AuditProc(diagnostic.record,result,seed);
        }
        if (self.after) self.after(*game, *owner, *target, *damage, *effect, result);
        return result;
    }
    inline static const Table Vtable{nullptr, Call};
    Bridge(void* prior, Before pre, After post) : table(&Vtable),
        previous(prior), before(pre), after(post) {}
};
constexpr int StatId(std::uint64_t key) noexcept {
    return static_cast<int>((key >> 32) & 0xffff);
}
}
