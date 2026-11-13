#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/lifecycle.h>
#include <D2RLPlugin/lifecycle_events.h>
#include <MinHook.h>

namespace {
using StepFn = void(*)(void*, void*, uint64_t);
using BirthFn = void(*)(void*, void*, void*, void*);
using FactoryFn = void*(*)(void*, int32_t);
using SyncFn = void(*)(void*, void*, void*, int32_t);
using LookupFn = void*(*)(void*, uint32_t, uint32_t);
using EachFn = void(*)(void*, void(*)(void*, void*), void*, uint32_t);
using LevelFn = int32_t(*)(const void*);
using CoordFn = uint16_t(*)(const void*);
using PathTypeFn = void(*)(void*, int32_t);
using PathBuildFn = int32_t(*)(void*, void*, int32_t, int32_t);

const D2RL::PluginContext* context = nullptr;
const D2RL::LifecycleService* lifecycle = nullptr;
D2RL::Lifecycle::ListenerHandle listener{};
uintptr_t enemy = 0, exe = 0;
StepFn originalStep = nullptr;
BirthFn originalBirth = nullptr;
FactoryFn originalFactory = nullptr;
SyncFn originalSync = nullptr;
void* targets[4]{};
unsigned installed = 0;
struct Birth { uint32_t uid; bool nativeSync; };
struct Batch {
    void* game;
    void* owner;
    Birth items[16];
    unsigned count;
};
thread_local Batch* activeBatch = nullptr;
thread_local bool creatingPacketHammer = false;

template<typename T> bool Read(uintptr_t address, T& value) {
    SIZE_T count = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
        &value, sizeof(value), &count) && count == sizeof(value);
}
template<typename T> bool Write(uintptr_t address, const T& value) {
    SIZE_T count = 0;
    return address && WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
        &value, sizeof(value), &count) && count == sizeof(value);
}
template<size_t N> bool Match(uintptr_t at, const unsigned char (&expected)[N]) {
    unsigned char bytes[N]{};
    return Read(at, bytes) && memcmp(bytes, expected, N) == 0;
}

bool IsGorath(void* owner) {
    uint32_t id[2]{};
    return Read(reinterpret_cast<uintptr_t>(owner), id) && id[0] == 1 && id[1] >= 1827 && id[1] <= 1842;
}

bool IsHammer(void* missile) {
    uint32_t id[2]{};
    return Read(reinterpret_cast<uintptr_t>(missile), id) && id[0] == 3 && id[1] == 92;
}

void SetSpiral(void* missile) {
    uintptr_t path = 0;
    uint32_t type = 0;
    if (!IsHammer(missile) || !Read(reinterpret_cast<uintptr_t>(missile) + 0x38, path)
        || !path || !Read(path + 0x50, type) || type == 14) return;
    reinterpret_cast<PathTypeFn>(exe + 0x342a70)(reinterpret_cast<void*>(path), 14);
    reinterpret_cast<PathBuildFn>(exe + 0x340e70)(reinterpret_cast<void*>(path), missile, 0, 1);
}

void RepairCollision(void* missile) {
    auto unit = reinterpret_cast<uintptr_t>(missile);
    uintptr_t path = 0;
    uint32_t mode = 0, mask = 0, mask3 = 0, mask5 = 0;
    constexpr uintptr_t collisionModes = 0x2391370;
    if (!Read(unit + 0xc, mode) || mode != 5 || !Read(unit + 0x38, path) || !path
        || !Read(path + 0x64, mask) || mask != 0x100
        || !Read(exe + collisionModes + 3 * 16 + 8, mask3) || mask3 != 0x184
        || !Read(exe + collisionModes + 5 * 16 + 8, mask5) || mask5 != mask) return;
    if (Write(path + 0x64, mask3) && !Write(unit + 0xc, uint32_t(3))) Write(path + 0x64, mask);
}

struct SendRequest { void* game; void* missile; LevelFn level; int32_t levelId; int x, y; };
bool Position(void* unit, int& x, int& y) {
    uintptr_t path = 0;
    if (!Read(reinterpret_cast<uintptr_t>(unit) + 0x38, path) || !path) return false;
    x = reinterpret_cast<CoordFn>(exe + 0x341a20)(reinterpret_cast<void*>(path));
    y = reinterpret_cast<CoordFn>(exe + 0x341a30)(reinterpret_cast<void*>(path));
    return x > 0 && y > 0;
}
void SendToClient(void* client, void* data) {
    const auto& request = *static_cast<SendRequest*>(data);
    const auto at = reinterpret_cast<uintptr_t>(client);
    void* player = nullptr;
    void* game = nullptr;
    uint32_t type = 99;
    int x = 0, y = 0;
    if (!Read(at + 0x2b0, game) || game != request.game || !Read(at + 0x278, player)
        || !Read(reinterpret_cast<uintptr_t>(player), type) || type != 0
        || request.level(player) != request.levelId || !Position(player, x, y)) return;
    if (x < request.x - 48 || x > request.x + 48 || y < request.y - 48 || y > request.y + 48) return;
    reinterpret_cast<SyncFn>(enemy + 0x25400)(client, request.game, request.missile, 1);
}
void ObserveSync(void* client, void* game, void* missile, int32_t velocity) {
    uint32_t uid = 0;
    const DWORD error = GetLastError();
    if (activeBatch && activeBatch->game == game && IsHammer(missile)
        && Read(reinterpret_cast<uintptr_t>(missile) + 8, uid)) {
        for (unsigned i = 0; i < activeBatch->count; ++i)
            if (activeBatch->items[i].uid == uid) activeBatch->items[i].nativeSync = true;
    }
    SetLastError(error);
    originalSync(client, game, missile, velocity);
}
void ObserveBirth(void* game, void* owner, void* params, void* missile) {
    originalBirth(game, owner, params, missile);
    const DWORD error = GetLastError();
    uint32_t skill = 0, uid = 0;
    if (activeBatch && activeBatch->game == game && activeBatch->owner == owner
        && activeBatch->count < 16 && Read(reinterpret_cast<uintptr_t>(params) + 60, skill)
        && skill == 447 && IsHammer(missile) && Read(reinterpret_cast<uintptr_t>(missile) + 8, uid) && uid) {
        bool duplicate = false;
        for (unsigned i = 0; i < activeBatch->count; ++i) duplicate |= activeBatch->items[i].uid == uid;
        if (!duplicate) activeBatch->items[activeBatch->count++] = {uid, false};
    }
    SetLastError(error);
}
void ObserveStep(void* game, void* unit, uint64_t now) {
    const DWORD before = GetLastError();
    if (!game || activeBatch || !IsGorath(unit)) {
        SetLastError(before);
        originalStep(game, unit, now);
        return;
    }
    Batch batch{game, unit, {}, 0};
    activeBatch = &batch;
    SetLastError(before);
    originalStep(game, unit, now);
    const DWORD after = GetLastError();
    activeBatch = nullptr;

    LookupFn lookup = nullptr;
    EachFn each = nullptr;
    uint8_t ready = 0;
    uintptr_t forwarding = 0;
    if (batch.count && Read(enemy + 0x114d691, ready) && ready
        && Read(enemy + 0x114d7b0, forwarding) && forwarding
        && Read(enemy + 0x114d810, lookup) && lookup && Read(enemy + 0x114d828, each) && each) {
        const auto core = GetModuleHandleW(L"D2RCore.dll");
        const auto level = core ? reinterpret_cast<LevelFn>(GetProcAddress(core, MAKEINTRESOURCEA(2000))) : nullptr;
        const int32_t levelId = level ? level(unit) : -1;
        for (unsigned i = 0; i < batch.count; ++i) {
            void* missile = lookup(game, 3, batch.items[i].uid);
            void* missileGame = nullptr;
            if (!IsHammer(missile) || !Read(reinterpret_cast<uintptr_t>(missile) + 0xd8, missileGame)
                || missileGame != game) continue;
            RepairCollision(missile);
            SetSpiral(missile);
            SendRequest request{game, missile, level, levelId, 0, 0};
            if (!batch.items[i].nativeSync && levelId > 0 && Position(missile, request.x, request.y))
                each(game, SendToClient, &request, 0);
        }
    }
    SetLastError(after);
}
__attribute__((noinline)) void* CreateClientHammer(void* params, int32_t extra) {
    const auto caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    const DWORD before = GetLastError();
    uint32_t missile = 0, skill = 0;
    void* owner = nullptr;
    const bool isGorathHammer = Read(reinterpret_cast<uintptr_t>(params) + 32, missile) && missile == 92
        && Read(reinterpret_cast<uintptr_t>(params) + 8, owner) && IsGorath(owner);
    if (creatingPacketHammer && isGorathHammer && caller == exe + 0x1b9eac) {
        SetLastError(before);
        return nullptr;
    }
    const bool root = isGorathHammer && caller == exe + 0x215ba1
        && Read(reinterpret_cast<uintptr_t>(params) + 60, skill) && skill == 447;
    const bool saved = creatingPacketHammer;
    if (root) creatingPacketHammer = true;
    SetLastError(before);
    void* result = originalFactory(params, extra);
    const DWORD after = GetLastError();
    creatingPacketHammer = saved;
    if (root) SetSpiral(result);
    SetLastError(after);
    return result;
}

void Detach() {
    while (installed) {
        void* target = targets[--installed];
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
}

bool Attach() {
    if (installed == 4) return true;
    enemy = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"EnemySkills.dll"));
    if (!enemy) return false;
    uint32_t pe = 0, timestamp = 0, imageSize = 0;
    if (!Read(enemy + 0x3c, pe) || !Read(enemy + pe + 8, timestamp) || timestamp != 0x6ae3cf39
        || !Read(enemy + pe + 80, imageSize) || imageSize != 0x16b2000) return false;
    constexpr unsigned char cloneCaller[]={0xe8,0xb4,0xd8,0xff,0xff,0xeb,0x02,0x33,0xc0};
    constexpr unsigned char packetCaller[]={0xe8,0xbf,0x1b,0xfa,0xff,0x4c,0x8b,0xf0,0x48,0x85,0xc0};
    constexpr unsigned char setBytes[]={0x48,0x63,0xc2,0x4c,0x8d,0x15,0x86,0xd5,0xcb,0xff,0x4c,0x8d,0x0c,0x85,0x00,0x00,0x00,0x00,0x47,0x8b,0x84,0x11,0xc0,0x02};
    constexpr unsigned char buildBytes[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    constexpr unsigned char StepBytes[] = {
    0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,
    0x48,0x81,0xec,0x98,0x02,0x00,0x00,0x48,0x8d,0xac,0x24,0x80,
    0x00,0x00,0x00,0x0f,0x29,0xb5,0x00,0x02
};
    constexpr unsigned char coordX[] = {0x0f,0xb7,0x41,0x02,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
    constexpr unsigned char coordY[] = {0x0f,0xb7,0x41,0x06,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
    const unsigned char bytes[][16]={{0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0xa8},{0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x83,0xec,0x28},{0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0x08},{0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0xe8},{0x55,0x48,0x89,0xe5,0x48,0x8b,0x05,0xc5,0x82,0x12,0x01,0x5d,0x48,0xff,0xe0,0x90}};
    constexpr unsigned char hammerMeta[] = {0xbf, 0x01, 0x70, 0x00, 0x00, 0x00, 0x49, 0x00};
    constexpr unsigned char maskSetter[] = {0x89, 0x51, 0x64, 0xc3};
    constexpr unsigned char modeGetter[] = {0x8b, 0x41, 0x0c};
    if (!Match(enemy + 0x7b0f8, hammerMeta) || !Match(enemy + 0x66e50, StepBytes)
        || !Match(enemy + 0x19270, bytes[0]) || !Match(enemy + 0x25400, bytes[1])
        || !Match(enemy + 0x2a4b0, bytes[2]) || !Match(exe + 0x1b9ea7, cloneCaller)
        || !Match(exe + 0x215b9c, packetCaller) || !Match(exe + 0x342a70, setBytes)
        || !Match(exe + 0x340e70, buildBytes) || !Match(exe + 0x341a20, coordX)
        || !Match(exe + 0x341a30, coordY) || !Match(exe + 0x342740, maskSetter)
        || !Match(exe + 0x34ab83, modeGetter)) return false;
    const auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
    const uintptr_t rvas[] = {0x19270, 0x25400, 0x2a4b0, 0x66e50};
    void* replacements[] = {reinterpret_cast<void*>(ObserveBirth), reinterpret_cast<void*>(ObserveSync),
        reinterpret_cast<void*>(CreateClientHammer), reinterpret_cast<void*>(ObserveStep)};
    void** originals[] = {reinterpret_cast<void**>(&originalBirth), reinterpret_cast<void**>(&originalSync),
        reinterpret_cast<void**>(&originalFactory), reinterpret_cast<void**>(&originalStep)};
    for (unsigned i = 0; i < 4; ++i) {
        auto* target = reinterpret_cast<void*>(enemy + rvas[i]);
        if (MH_CreateHook(target, replacements[i], originals[i]) != MH_OK) {
            Detach();
            return false;
        }
        targets[installed++] = target;
        if (MH_QueueEnableHook(target) != MH_OK) {
            Detach();
            return false;
        }
    }
    if (MH_ApplyQueued() == MH_OK) return true;
    Detach();
    return false;
}

void Joined(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent*, void*) noexcept {
    if (!Attach()) context->WriteConsoleMessage("Gorath Hammer Sync: unsupported EnemySkills or game build.");
}

const D2RL::PluginInfo info = {
    sizeof(D2RL::PluginInfo), 4, "r3ni-gorath-hammer-sync", "Gorath Hammer Sync", "1.0.0", "r3ni",
    "Synchronizes native Gorath hammers and restores their spiral path.",
    D2RL::PluginFlags::ModScopedOnly | D2RL::PluginFlags::NativeHooks | D2RL::PluginFlags::Shared, {}
};
}

extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept {
    return &info;
}
extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept {
    if (!D2RL::HasContext(ctx) || !ctx->exeBase || !ctx->activeMod
        || strcmp(ctx->activeMod, "VanillaDeluxe") != 0
        || ctx->QueryService(&lifecycle) != D2RL::ServiceQueryResult::Success || !lifecycle
        || !lifecycle->registerGameplayEventListener || !lifecycle->unregisterGameplayEventListener)
        return false;
    context = ctx;
    exe = ctx->exeBase;
    const D2RL::Lifecycle::GameplayEventListener subscription = {
        .structSize = D2RL::Lifecycle::GameplayEventListenerSize,
        .kind = D2RL::Lifecycle::GameplayEventKind::GameJoined,
        .callback = Joined
    };
    return lifecycle->registerGameplayEventListener(ctx, &subscription, &listener)
        == D2RL::Lifecycle::Result::Success;
}
extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept {
    if (listener && context) lifecycle->unregisterGameplayEventListener(context, listener);
    Detach();
    listener = {};
    context = nullptr;
}
extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
