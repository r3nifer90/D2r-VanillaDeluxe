"""Regenerate the exact versioned catalog; never infer a new reward pool."""
from pathlib import Path
import json
r=Path(__file__).resolve().parent
rows=json.loads((r/'catalog.json').read_text())['rewards']
assert len(rows)==1390
lines=['#pragma once','#include <cstdint>','struct Reward {uint32_t code,quality,record;};','inline constexpr Reward Rewards[] = {']
for x in rows:
 code=int.from_bytes(x['code'].encode('ascii').ljust(4,b' '),'little')
 lines.append('{%du,%du,%du},'%(code,x['quality'],x['record']))
lines+=['};','inline constexpr uint64_t CatalogToken=0x4348524f4e323130ULL;']
(r/'catalog.hpp').write_text('\n'.join(lines)+'\n')
