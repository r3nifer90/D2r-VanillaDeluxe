# Mystery Chest 2.1.2 — Chronicle reconciliation

Updates the existing r3ni-mystery-chest module. No second plugin, diagnostic plugin, save replacement, new item row, TreasureClass edit, or change to reward-slot weights.

Supported runtime: D2RLoader 95032, ABI 4, VanillaDeluxe RotW tables with 2149 unique rows and 219 set rows. Keep the previously installed V3 Chronicle visibility tables. This update contains no data-table replacements.

The native unique generator first selects the original entry using its original RNG, weights, level checks and repeat rules. Interchangeable same-base legacy IDs then resolve to the current balanced definition before properties are applied. The selected source's no-limit bit remains effective during that generation to avoid turning an allowed repeated legacy unique into a failed unique. Existing saved item instances, skill-specific Shard of Mastery variants and physical Rainbow Facet variants are retained. Chronicle identity is merged separately.

Loaded Chronicle records are merged by canonical identity and the earliest existing discovery is retained. A merged duplicate is accepted as a successfully read archive record, allowing all following records to load. Identified items decoded from the restoring character's saved inventory, shared pages, sockets and mercenary equipment provide recovery evidence; unrelated network item decoding does not. Original item history date/source is retained where available. Without such history, recovered discoveries use the localized unknown source and the recovery date, not an invented original drop date. This is normal runtime Chronicle updating, persisted by the game's ordinary save process; no offline save editor is used.

The exact 1390 ordered Mystery Chest reward slots from installed 2.0.5-charms remain equally weighted. Successful identified unique/set rewards now register through native Chronicle functions and update the owning client. Ready-time synchronization sends the reconciled entries after the character is available. Native runeword handling is unchanged.

## Installation

Fully close the game. Replace the existing file (do not rename it or install another copy):

D:\Battle.net\gry\Diablo II Resurrected\mods\VanillaDeluxe\d2rloader\plugins\MysteryChest.dll

No compilation is necessary for the MODS package. Restart the loader completely, enter the existing character, and inspect Arctic Gear / Arctic Mitts. Exit normally and enter again to confirm persistence. Also check a newly found item and an identified Mystery Chest unique/set reward.

## Build

From this source directory, with Python and ziglang installed:

```sh
python -m pip install ziglang==0.17.1
python build.py
```

Output: MysteryChest.dll. The bundled SDK headers and generated record_map.h are build inputs. catalog.py regenerates catalog.hpp from the versioned catalog.json without inferring a different pool.

## Verification and limits

The host test compiles the actual plugin translation unit with mocked Windows/SDK services. It covers production initialization, all service/hook registrations, expected-byte and table identity guards, all 1390 rewards, transaction rollback, duplicate/stale clicks, native recipe fallback and RNG failure.

```sh
g++ -std=c++20 -O2 -Itests/stubs -ISDK/include tests/test_plugin.cpp -o host_tests
./host_tests
```

A separate offline Unicorn test ran the compiled integration against captured build-95032 native x64 Chronicle and unique-generation code. It accepted all 802 supplied saved Chronicle records, recovered Arctic Mitts with its original history, exercised 1143 interchangeable unique definitions, verified an unchanged random roll/weights, owner-scoped save recovery, mercenary recovery, native client packets and new cube reward registration. Windows services and selected surrounding engine services were mocked explicitly. No live game, UI rendering, real hook installer, or multiplayer session was run here. The user's live check remains necessary.

User save data and game binaries are excluded from the source checkpoint.

## 2.1.1 shared-stash regression

2.1.0 scoped the character/mercenary ReadItemsByVersion entry point (41f0b0), but the actual shared-stash archive reader at 530590 calls the separate shared-page reader (41f0f0) directly. Its decoded items therefore had no restoring owner/evidence scope. 2.1.1 wraps that shared-page entry point using the same owner-scoped observer. It forwards all seven arguments and the return value unchanged, changes no item/save bytes, and preserves the existing ready-time update path. This new hook is guarded by its build-95032 expected bytes.

The regression executes the real native shared-stash archive reader over 20 synthetic pages. With the old unscoped path, Arctic Mitts does not recover; with the production shared-page wrapper it does recover with the original source/date. Item deserialization underneath that page wrapper is mocked explicitly. The test does not claim a live-game validation.

Optional native emulation test, from this source directory (a legally obtained unpacked build-95032 text capture is required and not included):

```sh
python -m pip install pefile capstone unicorn
python -m ziglang c++ -target x86_64-windows-gnu -std=c++20 -O2 -shared -fno-exceptions -fno-rtti -ISDK/include tests/native_exports.cpp -o tests/native_tests.dll
python tests/test_native.py --native-text /absolute/path/D2RLoader-95032-text.bin
```

The default native test uses synthetic items and empty initial Chronicle maps. An optional --audit JSON input can check a separately supplied offline audit without embedding it in this repository.

## 2.1.2 — selected world-loot rarity

The supplied list contains 44 logical uniques, covering 102 canonical/legacy physical record IDs. The compile-time record selector is in `selected_loot.h`. After native unique selection, the existing unique-record getter hook applies one independent unbiased 50% retention test, before native record assignment, properties and the game's unique-use flag. A rejected lookup follows the existing native failed-unique path; it never creates an alternative unique or an item row. Native loot stack recognition is scoped to build 95032's treasure-class executor range. Inventory restoration, vendors and cube generation are outside that executor. Mystery Chest's active transactional capture unconditionally bypasses the filter, preserving all 1390 original reward slots and their weights. Only the Rotw mod bank is affected. Existing Chronicle/canonical hooks and the same plugin ID are retained; no new hook entry or new module is introduced.

Randomness uses BCryptGenRandom and fails open. There is no diagnostic logging or diagnostic tool embedded in this module. Runtime code mutates neither TXT/BIN tables nor saved items. A stack capture is performed only for a freshly selected target unique; live stack unwinding and a multiplayer session still require an in-game test.

Tests cover both outcomes for all 102 physical identities, Mystery Chest/non-loot/non-target exclusions, entropy failure, scope restoration and 1,000,000 deterministic parity draws. The offline native emulator executes both the forced and weighted build-95032 unique generator with the compiled filter, including no-property/no-record-change failure and successful canonical property application. Windows stack walking and entropy are mocked explicitly in these offline tests. Native Chronicle regression and all 1390 cube reward slots also pass. These are not live-game validations.

Build (install ziglang 0.13.0): `python build.py`. Native fixture build requires `-lbcrypt`. Host unit tests require `-include tests/stubs/windows.h` when compiling the standalone selected-loot test.
