# Mystery Chest 2.1.0 — Chronicle reconciliation

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
