#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/lifecycle.h>
#include <D2RLPlugin/lifecycle_events.h>
#include <D2RLPlugin/threads.h>

namespace {
const D2RL::PluginContext* context = nullptr;
const D2RL::LifecycleService* lifecycle = nullptr;
const D2RL::ThreadService* threads = nullptr;
D2RL::Lifecycle::ListenerHandle listeners[2]{};
SRWLOCK stateLock = SRWLOCK_INIT;
uint64_t session = 0;
bool pending = false;
bool spawned = false;

struct SpawnRequest {
    void* game;
    const void* room;
    void* origin;
    int32_t monster;
    uint32_t mode, unused;
    int32_t x, y;
    uint32_t flags;
    uint64_t extra1, extra2;
};
static_assert(sizeof(SpawnRequest) == 64);

struct DungeonApi {
    uint32_t size, version, count, reserved;
    bool (*initialize)(uint64_t, uint32_t) noexcept;
    bool (*spawn)(const SpawnRequest*, uint32_t, uint32_t, uint32_t) noexcept;
    uint32_t (*weight)(uint32_t) noexcept;
};
static_assert(sizeof(DungeonApi) == 40);

template <class T>
bool Read(uintptr_t address, T& value) noexcept {
    SIZE_T count = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                             &value, sizeof(value), &count) && count == sizeof(value);
}

bool CoordinatesAvailable() noexcept {
    // D2R build 95032. Check both coordinate accessors before calling either.
    constexpr unsigned char xCode[] = {0x0f, 0xb7, 0x41, 0x02, 0xc3, 0xcc, 0xcc, 0xcc};
    constexpr unsigned char yCode[] = {0x0f, 0xb7, 0x41, 0x06, 0xc3, 0xcc, 0xcc, 0xcc};
    unsigned char actual[sizeof(xCode)];
    return Read(context->exeBase + 0x341a20, actual) && !memcmp(actual, xCode, sizeof(actual))
        && Read(context->exeBase + 0x341a30, actual) && !memcmp(actual, yCode, sizeof(actual));
}

const char* Spawn(void* userData, uint64_t generation, bool& success) noexcept {
    if (!CoordinatesAvailable())
        return "SkeletonBossProbe: unsupported D2R build.";
    uintptr_t client = reinterpret_cast<uintptr_t>(userData), player = 0, game = 0, path = 0, room = 0;
    uint8_t difficulty = 0, bank = 0;
    if (!Read(client + 0x278, player) || !player
        || !Read(client + 0x2b0, game) || !game
        || !Read(player + 0x38, path) || !path
        || !Read(path + 0x20, room) || !room
        || !Read(game + 0x104, difficulty) || difficulty != 2
        || !Read(game + 0x106, bank) || bank < 2 || bank > 3)
        return "SkeletonBossProbe: enter a Hell game as host, outside town.";

    using LevelFn = int32_t(__fastcall*)(const void*);
    auto core = GetModuleHandleW(L"D2RCore.dll");
    auto getLevel = core ? reinterpret_cast<LevelFn>(GetProcAddress(core, MAKEINTRESOURCEA(2000))) : nullptr;
    const int32_t level = getLevel ? getLevel(reinterpret_cast<void*>(player)) : -1;
    if (level <= 0 || level == 1 || level == 40 || level == 75 || level == 103 || level == 109)
        return "SkeletonBossProbe: try an open spot in the Blood Moor on Hell.";

    auto dungeon = GetModuleHandleW(L"DungeonLegends.dll");
    if (!dungeon || !GetModuleHandleW(L"EnemySkills.dll"))
        return "SkeletonBossProbe: DungeonLegends.dll and EnemySkills.dll must be loaded.";
    using ApiFn = const DungeonApi*(*)() noexcept;
    auto getApi = reinterpret_cast<ApiFn>(GetProcAddress(dungeon, "R3niDungeonGetApiV1"));
    const DungeonApi* api = getApi ? getApi() : nullptr;
    if (!api || api->size != sizeof(DungeonApi) || api->version != 1 || api->count != 31
        || !api->initialize || !api->spawn || !api->initialize(generation, bank))
        return "SkeletonBossProbe: Dungeon Legends API or runtime tables unavailable.";

    using CoordinateFn = uint16_t(__fastcall*)(const void*);
    auto getX = reinterpret_cast<CoordinateFn>(context->exeBase + 0x341a20);
    auto getY = reinterpret_cast<CoordinateFn>(context->exeBase + 0x341a30);
    const SpawnRequest request = {
        .game = reinterpret_cast<void*>(game), .room = reinterpret_cast<void*>(room), .mode = 1,
        .x = getX(reinterpret_cast<void*>(path)) + 6, .y = getY(reinterpret_cast<void*>(path)) + 6,
        .flags = 3,
    };
    constexpr uint32_t azharak = 5; // Dungeon Legends profile 6, zero-based.
    success = api->spawn(&request, azharak, 0, static_cast<uint32_t>(level));
    return success ? "SkeletonBossProbe: Azharak, Keeper of the Dead spawned nearby."
                   : "SkeletonBossProbe: spawn failed. Try another open spot.";
}

void OnGameThread(const D2RL::PluginContext*, void* client) noexcept {
    AcquireSRWLockExclusive(&stateLock);
    const auto generation = pending ? session : 0;
    ReleaseSRWLockExclusive(&stateLock);
    if (!generation) return;

    bool success = false;
    const char* result = Spawn(client, generation, success);

    AcquireSRWLockExclusive(&stateLock);
    if (session == generation) {
        spawned = success;
        pending = false;
    }
    ReleaseSRWLockExclusive(&stateLock);
    context->WriteConsoleMessage(result);
}

D2RL::ConsoleCommandResult Command(D2R::Game::Client* client,
    const D2RL::ConsoleCommandContext* command, void*) noexcept {
    using Result = D2RL::ConsoleCommandResult;
    if (!client || !command || command->contextSize < D2RL::ConsoleCommandContextSize)
        return Result::Failed;
    if (command->argsLength) return Result::InvalidArguments;

    AcquireSRWLockExclusive(&stateLock);
    const auto generation = session;
    const char* unavailable = !session ? "SkeletonBossProbe: start a local Hell game first."
        : spawned ? "SkeletonBossProbe: one boss per game. Start a new game to repeat."
        : pending ? "SkeletonBossProbe: spawn is already queued." : nullptr;
    if (!unavailable) pending = true;
    ReleaseSRWLockExclusive(&stateLock);
    if (unavailable) {
        context->WriteConsoleMessage(unavailable);
        return Result::Handled;
    }
    if (threads->runOnGameThread(context, OnGameThread, client) != D2RL::Threads::Result::Success) {
        AcquireSRWLockExclusive(&stateLock);
        if (session == generation) pending = false;
        ReleaseSRWLockExclusive(&stateLock);
        context->WriteConsoleMessage("SkeletonBossProbe: authoritative game thread unavailable.");
    }
    return Result::Handled;
}

void Gameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
    if (!event || event->structSize < D2RL::Lifecycle::GameplayEventSize) return;
    AcquireSRWLockExclusive(&stateLock);
    session = event->kind == D2RL::Lifecycle::GameplayEventKind::GameLeft ? 0 : event->sessionGeneration;
    pending = spawned = false;
    ReleaseSRWLockExclusive(&stateLock);
}

const D2RL::PluginInfo info = {
    sizeof(D2RL::PluginInfo), 4, "r3ni-skeleton-boss-probe", "Skeleton Boss Probe", "1.0.0", "r3ni",
    "Spawns Azharak through Dungeon Legends to test EnemySkills summons.",
    D2RL::PluginFlags::ModScopedOnly | D2RL::PluginFlags::Server, {}
};
}

extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept {
    return &info;
}

extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept {
    if (lifecycle && context)
        for (auto& handle : listeners) {
            if (handle) lifecycle->unregisterGameplayEventListener(context, handle);
            handle = {};
        }
    context = nullptr;
}

extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept {
    if (!D2RL::HasContext(ctx) || !ctx->exeBase || !ctx->activeMod
        || strcmp(ctx->activeMod, "VanillaDeluxe") != 0
        || ctx->QueryService(&threads) != D2RL::ServiceQueryResult::Success || !threads || !threads->runOnGameThread
        || ctx->QueryService(&lifecycle) != D2RL::ServiceQueryResult::Success
        || !lifecycle || !lifecycle->registerGameplayEventListener || !lifecycle->unregisterGameplayEventListener)
        return false;

    context = ctx;
    using Kind = D2RL::Lifecycle::GameplayEventKind;
    const Kind kinds[] = {Kind::GameJoined, Kind::GameLeft};
    for (unsigned i = 0; i < 2; ++i) {
        const D2RL::Lifecycle::GameplayEventListener listener = {
            .structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kinds[i], .callback = Gameplay,
        };
        if (lifecycle->registerGameplayEventListener(ctx, &listener, &listeners[i]) != D2RL::Lifecycle::Result::Success) {
            D2RLoaderUnloadPlugin();
            return false;
        }
    }
    auto command = D2RL::MakeConsoleCommand("skeletonboss", Command, "Spawn Azharak for a summon test.");
    command.scope = D2RL::ConsoleCommandScope::LocalOnly;
    command.access = D2RL::ConsoleCommandAccess::Host;
    command.usage = "skeletonboss";
    if (ctx->RegisterConsoleCommand(command)) return true;
    D2RLoaderUnloadPlugin();
    return false;
}
