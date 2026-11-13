using uintptr_t = __UINTPTR_TYPE__;
using uint64_t = __UINT64_TYPE__;
using uint32_t = __UINT32_TYPE__;
using int32_t = __INT32_TYPE__;
using uint16_t = __UINT16_TYPE__;
using uint8_t = __UINT8_TYPE__;
using size_t = __SIZE_TYPE__;
using SIZE_T = size_t;
using DWORD = unsigned long;
using HANDLE = void*;
using IMAGE_DOS_HEADER = unsigned char;
struct MEMORY_BASIC_INFORMATION {
    void* BaseAddress;
    void* AllocationBase;
    DWORD AllocationProtect;
    unsigned short PartitionId;
    size_t RegionSize;
    DWORD State, Protect, Type;
};
constexpr DWORD MEM_COMMIT=0x1000, PAGE_READWRITE=4, PAGE_EXECUTE_READWRITE=0x40;
extern "C" DWORD GetCurrentThreadId();
extern "C" DWORD GetLastError();
extern "C" void SetLastError(DWORD);
extern "C" void* GetModuleHandleW(const wchar_t*);
extern "C" void* GetProcAddress(void*, const char*);
extern "C" int ReadProcessMemory(HANDLE, const void*, void*, size_t, size_t*);
extern "C" size_t VirtualQuery(const void*, MEMORY_BASIC_INFORMATION*, size_t);
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

constexpr uintptr_t payloadBaseRva = 0x16b2000;
extern "C" IMAGE_DOS_HEADER __ImageBase;
uintptr_t Enemy() { return reinterpret_cast<uintptr_t>(&__ImageBase) - payloadBaseRva; }
uintptr_t GameExe() {
    uintptr_t ctx = *reinterpret_cast<const uintptr_t*>(Enemy() + 0x114cbb0);
    return ctx ? *reinterpret_cast<const uintptr_t*>(ctx + 0x18) : 0;
}
template<typename T> T Import(uintptr_t rva) {
    return *reinterpret_cast<const T*>(Enemy() + rva);
}
DWORD ThreadId() { return Import<decltype(&GetCurrentThreadId)>(0x7d1b0)(); }
DWORD LastError() { return Import<decltype(&GetLastError)>(0x7d1c0)(); }
void RestoreError(DWORD value) { Import<decltype(&SetLastError)>(0x7d248)(value); }
struct Birth { uint32_t uid; };
struct Batch {
    void* game;
    void* owner;
    Birth items[16];
    unsigned count;
};
struct Scope { unsigned long thread; Batch* batch; };
Scope serverScopes[16]{};
unsigned long clientScopes[16]{};

template<typename T> bool Read(uintptr_t address, T& value) {
    SIZE_T count = 0;
    return address && Import<decltype(&ReadProcessMemory)>(0x7d230)(
        reinterpret_cast<HANDLE>(-1), reinterpret_cast<const void*>(address),
        &value, sizeof(value), &count) && count == sizeof(value);
}
bool Writable(uintptr_t address) {
    MEMORY_BASIC_INFORMATION info{};
    return Import<decltype(&VirtualQuery)>(0x7d290)(reinterpret_cast<void*>(address), &info, sizeof(info))
        && info.State == MEM_COMMIT && (info.Protect == PAGE_READWRITE || info.Protect == PAGE_EXECUTE_READWRITE)
        && address - reinterpret_cast<uintptr_t>(info.BaseAddress) <= info.RegionSize - sizeof(uint32_t);
}
Scope* FindScope(unsigned long thread) {
    for (auto& slot : serverScopes)
        if (__atomic_load_n(&slot.thread, __ATOMIC_ACQUIRE) == thread) return &slot;
    return nullptr;
}
template<typename T> bool Match(uintptr_t at, const T& expected) {
    T bytes{};
    if (!Read(at, bytes)) return false;
    for (unsigned i = 0; i < sizeof(T); ++i) if (bytes[i] != expected[i]) return false;
    return true;
}

constexpr unsigned char cloneCaller[]={0xe8,0xb4,0xd8,0xff,0xff,0xeb,0x02,0x33,0xc0};
constexpr unsigned char packetCaller[]={0xe8,0xbf,0x1b,0xfa,0xff,0x4c,0x8b,0xf0,0x48,0x85,0xc0};
constexpr unsigned char setBytes[]={0x48,0x63,0xc2,0x4c,0x8d,0x15,0x86,0xd5,0xcb,0xff,0x4c,0x8d,0x0c,0x85,0x00,0x00,0x00,0x00,0x47,0x8b,0x84,0x11,0xc0,0x02};
constexpr unsigned char buildBytes[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
constexpr unsigned char coordX[] = {0x0f,0xb7,0x41,0x02,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
constexpr unsigned char coordY[] = {0x0f,0xb7,0x41,0x06,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
constexpr unsigned char maskSetter[] = {0x89, 0x51, 0x64, 0xc3};
constexpr unsigned char modeGetter[] = {0x8b, 0x41, 0x0c};
bool PathBindings() {
    const uintptr_t exe = GameExe();
    return exe && Match(exe + 0x342a70, setBytes) && Match(exe + 0x340e70, buildBytes);
}

uint32_t BossHammerSkill(void* owner) {
    uint32_t id[2]{};
    if (!Read(reinterpret_cast<uintptr_t>(owner), id) || id[0] != 1) return 0;
    if (id[1] >= 1827 && id[1] <= 1842) return 447;
    if (id[1] >= 1971 && id[1] <= 1986) return 474;
    return 0;
}

bool IsHammer(void* missile) {
    uint32_t id[2]{};
    return Read(reinterpret_cast<uintptr_t>(missile), id) && id[0] == 3 && id[1] == 92;
}

void SetSpiral(void* missile) {
    uintptr_t path = 0;
    uint32_t type = 0;
    if (!PathBindings() || !IsHammer(missile) || !Read(reinterpret_cast<uintptr_t>(missile) + 0x38, path)
        || !path || !Read(path + 0x50, type) || type == 14) return;
    reinterpret_cast<PathTypeFn>(GameExe() + 0x342a70)(reinterpret_cast<void*>(path), 14);
    reinterpret_cast<PathBuildFn>(GameExe() + 0x340e70)(reinterpret_cast<void*>(path), missile, 0, 1);
}

void RepairCollision(void* missile) {
    auto unit = reinterpret_cast<uintptr_t>(missile);
    uintptr_t path = 0;
    uint32_t mode = 0, mask = 0, mask3 = 0, mask5 = 0;
    constexpr uintptr_t collisionModes = 0x2391370;
    if (!Match(GameExe() + 0x342740, maskSetter) || !Match(GameExe() + 0x34ab83, modeGetter)
        || !Read(unit + 0xc, mode) || mode != 5 || !Read(unit + 0x38, path) || !path
        || !Read(path + 0x64, mask) || mask != 0x100
        || !Read(GameExe() + collisionModes + 3 * 16 + 8, mask3) || mask3 != 0x184
        || !Read(GameExe() + collisionModes + 5 * 16 + 8, mask5) || mask5 != mask) return;
    if (!Writable(path + 0x64) || !Writable(unit + 0xc)) return;
    *reinterpret_cast<uint32_t*>(path + 0x64) = mask3;
    *reinterpret_cast<uint32_t*>(unit + 0xc) = 3;
}

struct SendRequest { void* game; void* missile; LevelFn level; int32_t levelId; int x, y; };
bool Position(void* unit, int& x, int& y) {
    uintptr_t path = 0;
    if (!Match(GameExe() + 0x341a20, coordX) || !Match(GameExe() + 0x341a30, coordY)
        || !Read(reinterpret_cast<uintptr_t>(unit) + 0x38, path) || !path) return false;
    x = reinterpret_cast<CoordFn>(GameExe() + 0x341a20)(reinterpret_cast<void*>(path));
    y = reinterpret_cast<CoordFn>(GameExe() + 0x341a30)(reinterpret_cast<void*>(path));
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
    reinterpret_cast<SyncFn>(Enemy() + 0x25400)(client, request.game, request.missile, 1);
}
extern "C" __declspec(dllexport) void ObserveBirth(void* game, void* owner, void* params, void* missile) {
    reinterpret_cast<BirthFn>(Enemy() + 0x19270)(game, owner, params, missile);
    const DWORD error = LastError();
    const uint32_t expectedSkill = BossHammerSkill(owner);
    if (!expectedSkill) { RestoreError(error); return; }
    auto* slot = FindScope(ThreadId());
    auto* activeBatch = slot ? slot->batch : nullptr;
    uint32_t skill = 0, uid = 0;
    if (activeBatch && activeBatch->game == game && activeBatch->owner == owner
        && activeBatch->count < 16 && Read(reinterpret_cast<uintptr_t>(params) + 60, skill)
        && skill == expectedSkill && IsHammer(missile) && Read(reinterpret_cast<uintptr_t>(missile) + 8, uid) && uid) {
        bool duplicate = false;
        for (unsigned i = 0; i < activeBatch->count; ++i) duplicate |= activeBatch->items[i].uid == uid;
        if (!duplicate) activeBatch->items[activeBatch->count++] = {uid};
    }
    RestoreError(error);
}
extern "C" __declspec(dllexport) int32_t WalkDefiler(void* game, void* unit, int32_t x, int32_t y) {
    using WalkFn = int32_t(*)(void*, void*, int32_t, int32_t);
    const auto walk = Import<WalkFn>(0x114d570);
    if (!walk) return 0;
    int unitX = 0, unitY = 0;
    if (!Position(unit, unitX, unitY) || unitX - x > 3 || x - unitX > 3 ||
        unitY - y > 3 || y - unitY > 3) return walk(game, unit, x, y);


    int dx = (unitX > x) - (unitX < x);
    const int dy = (unitY > y) - (unitY < y);
    if (!dx && !dy) dx = 1;
    for (int side = 1; side >= -1; side -= 2) {
        const int flankX = x - side * dy;
        const int flankY = y + side * dx;
        if (flankX > 0 && flankX <= 65535 && flankY > 0 && flankY <= 65535 &&
            (flankX != unitX || flankY != unitY)) {
            const int result = walk(game, unit, flankX, flankY);
            if (result) return result;
        }
    }
    return walk(game, unit, x, y);
}
extern "C" __declspec(dllexport) bool WaitForPetAction(void* pet, void* unit) {
    using WaitFn = bool(*)(void*, void*);
    using FillModeFn = void(*)(void*, int32_t, void*);
    using ChangeModeFn = int32_t(*)(void*, void*, int32_t);
    const auto priorWait = reinterpret_cast<WaitFn>(Enemy() + 0x4af70);
    const auto record = reinterpret_cast<uintptr_t>(pet);
    const auto actor = reinterpret_cast<uintptr_t>(unit);
    uint32_t kind = 0, mode = 0;
    const uint32_t skill = *reinterpret_cast<const uint32_t*>(record + 0x94);
    const bool completedAttack = *reinterpret_cast<const uint8_t*>(record + 0x9c) &&
        *reinterpret_cast<const uint32_t*>(record + 0x90) == 4;
    const bool rearm = completedAttack && Read(actor + 4, kind) &&
        skill == 0 && (kind == 363 || kind == 419 || kind == 792) &&
        Read(actor + 0xc, mode) && mode == 1;
    const bool waiting = priorWait(pet, unit);
    if (waiting || !rearm) return waiting;



    const auto exe = GameExe();
    const unsigned char neutralFill[] = {0x48,0x89,0x5c,0x24,0x20,0x55,0x56,0x57};
    const unsigned char neutralGate[] = {0x48,0x8b,0x5a,0x08,0x48,0x8b,0xe9,0x48,0x8b,0xcb};
    if (!exe || !Match(exe + 0x4471e0, neutralFill) || !Match(exe + 0x4475ca, neutralGate))
        return false;
    alignas(8) unsigned char request[48]{};
    reinterpret_cast<FillModeFn>(exe + 0x4471e0)(unit, 1, request);
    void* game = *reinterpret_cast<void* const*>(record + 0x50);
    return game && reinterpret_cast<ChangeModeFn>(exe + 0x4475c0)(game, request, 1) != 0;
}
extern "C" __declspec(dllexport) void ObserveStep(void* game, void* unit, uint64_t now) {
    const DWORD before = LastError();
    const auto originalStep = reinterpret_cast<StepFn>(Enemy() + 0x66e50);
    if (!game || !BossHammerSkill(unit) || !GameExe()) {
        RestoreError(before);
        originalStep(game, unit, now);
        return;
    }
    Batch batch{game, unit, {}, 0};
    const auto thread = ThreadId();
    Scope* slot = nullptr;
    if (!FindScope(thread)) {
        for (auto& candidate : serverScopes) {
            unsigned long empty = 0;
            if (__atomic_compare_exchange_n(&candidate.thread, &empty, thread, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
                slot = &candidate;
                slot->batch = &batch;
                break;
            }
        }
    }
    RestoreError(before);
    originalStep(game, unit, now);
    const DWORD after = LastError();
    if (!slot) return;
    slot->batch = nullptr;
    __atomic_store_n(&slot->thread, 0ul, __ATOMIC_RELEASE);

    LookupFn lookup = nullptr;
    EachFn each = nullptr;
    uint8_t ready = 0;
    uintptr_t forwarding = 0;
    if (batch.count && Read(Enemy() + 0x114d691, ready) && ready
        && Read(Enemy() + 0x114d7b0, forwarding) && forwarding
        && Read(Enemy() + 0x114d810, lookup) && lookup && Read(Enemy() + 0x114d828, each) && each) {
        const auto core = Import<decltype(&GetModuleHandleW)>(0x7d1c8)(L"D2RCore.dll");
        const auto level = core ? reinterpret_cast<LevelFn>(Import<decltype(&GetProcAddress)>(0x7d1d8)(core, reinterpret_cast<const char*>(2000))) : nullptr;
        const int32_t levelId = level ? level(unit) : -1;
        for (unsigned i = 0; i < batch.count; ++i) {
            void* missile = lookup(game, 3, batch.items[i].uid);
            void* missileGame = nullptr;
            if (!IsHammer(missile) || !Read(reinterpret_cast<uintptr_t>(missile) + 0xd8, missileGame)
                || missileGame != game) continue;
            RepairCollision(missile);
            SetSpiral(missile);
            SendRequest request{game, missile, level, levelId, 0, 0};
            if (levelId > 0 && Position(missile, request.x, request.y))
                each(game, SendToClient, &request, 0);
        }
    }
    RestoreError(after);
}
extern "C" __declspec(dllexport) __attribute__((noinline)) void* CreateClientHammer(void* params, int32_t extra) {
    const auto caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    const DWORD before = LastError();
    uint32_t missile = 0, skill = 0;
    void* owner = nullptr;
    const uint32_t expectedSkill = Read(reinterpret_cast<uintptr_t>(params) + 32, missile) && missile == 92
        && Read(reinterpret_cast<uintptr_t>(params) + 8, owner) ? BossHammerSkill(owner) : 0;
    const auto thread = expectedSkill ? ThreadId() : 0;
    bool creatingPacketHammer = false;
    if (expectedSkill && caller == GameExe() + 0x1b9eac && Match(GameExe() + 0x1b9ea7, cloneCaller))
        for (auto& slot : clientScopes) creatingPacketHammer |= __atomic_load_n(&slot, __ATOMIC_ACQUIRE) == thread;
    if (creatingPacketHammer) {
        RestoreError(before);
        return nullptr;
    }
    const bool root = expectedSkill && caller == GameExe() + 0x215ba1 && Match(GameExe() + 0x215b9c, packetCaller)
        && Read(reinterpret_cast<uintptr_t>(params) + 60, skill) && skill == expectedSkill;
    unsigned long* active = nullptr;
    if (root) for (auto& slot : clientScopes) {
        unsigned long empty = 0;
        if (__atomic_compare_exchange_n(&slot, &empty, thread, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            active = &slot;
            break;
        }
    }
    RestoreError(before);
    void* result = reinterpret_cast<FactoryFn>(Enemy() + 0x2a4b0)(params, extra);
    const DWORD after = LastError();
    if (active) __atomic_store_n(active, 0ul, __ATOMIC_RELEASE);
    if (root) SetSpiral(result);
    RestoreError(after);
    return result;
}

}
extern "C" void* memset(void* target, int value, size_t length) {
    auto* bytes = static_cast<volatile unsigned char*>(target);
    for (size_t i=0; i<length; ++i) bytes[i]=static_cast<unsigned char>(value);
    return target;
}



extern "C" __declspec(dllexport) __attribute__((naked)) void SkarnathApproachRange() {
    __asm__(
        "movl $3, %edx\n"
        "testb $1, %r8b\n"
        "jz 1f\n"
        "cmpl $2051, 4(%r14)\n"
        "jb 2f\n"
        "cmpl $2066, 4(%r14)\n"
        "jbe 1f\n"
        "2: movl $9, %edx\n"
        "1: movl $9, %r8d\n"
        "retq\n"
    );
}
