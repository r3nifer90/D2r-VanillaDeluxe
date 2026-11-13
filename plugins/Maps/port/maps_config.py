






ROW_TAG = "RMAP"
CUBE_TAG = "rmap"
ITEM_TAG = "RMap"
AFFIX_TAG = "rmapaffix"







FIRST_LEVEL_ID = 166


FIRST_PREST_DEF = 1105






AUTOMAP_LAYER_COUNT = 100













MAP_WORLD_ORIGIN = (400, 4300)
MAP_BODY_CELL = 220
MAP_BODY_COLUMNS = 20
MAP_ARENA_CELL = 130
MAP_ARENA_COLUMNS = 30








MONLVL_BASE_ROW = 110
FIRST_MONLVL_ROW = 111

TIERS = [




    {"tier": 1, "scale": 1.2, "density": 1100, "umin": 20, "umax": 26},
    {"tier": 2, "scale": 1.4, "density": 1250, "umin": 22, "umax": 29},
    {"tier": 3, "scale": 1.6, "density": 1400, "umin": 23, "umax": 31},
    {"tier": 4, "scale": 1.8, "density": 1550, "umin": 25, "umax": 34},
    {"tier": 5, "scale": 2.0, "density": 1700, "umin": 27, "umax": 36},

    {"tier": 6, "scale": 3.0, "density": 2000, "umin": 29, "umax": 38},
]




MONLVL_FLAT_XP = True













































































THEMES = [
    {
        "key": "desert",
        "name": "Sandswept Tomb",



        "body_template": 62,







        "body_exits": [(1, 49)],
        "body_entry": (0, 48),


        "body_presets": {497: 501, 498: 502, 499: 503, 500: 504,
                         501: 497, 502: 498, 503: 499, 504: 500},




        "body_rooms": (80, 80, 80),
        "body_rooms_per_tier": 14,



        "arena_template": 138,
        "arena_ds1": "custom:sandswept",
        "arena_return": (2, 83),
    },
    {
        "key": "kurast",
        "name": "Corrupted Durance",


        "body_template": 88,


        "body_exits": [(0, 56)],
        "body_entry": (1, 55),


        "body_rooms": (12, 18, 24),




        "arena_template": 102,
        "arena_ds1": "custom:durance",
        "arena_return": (2, 65),
    },
    {
        "key": "catacombs",
        "name": "Forsaken Catacombs",
        "body_template": 157,
        "body_exits": [(1, 18)],

        "body_entry": (0, 17),





        "arena_template": 37,
        "arena_ds1": "custom:catacombs",
        "arena_return": (0, 17),
    },
    {
        "key": "frozen",
        "name": "Frozen Depths",
        "body_template": 159,







        "body_exits": [(2, 75)],
        "body_entry": (0, 73),




        "arena_template": 160,
        "arena_ds1": "custom:frozen",
        "arena_return": (0, 73),
    },
    {
        "key": "worldstone",
        "name": "Worldstone Keep",
        "body_template": 164,
        "body_exits": [(1, 82)],

        "body_entry": (0, 81),





        "arena_template": 131,
        "arena_ds1": "custom:worldstone",
        "arena_return": (0, 81),
    },
]




STOCK_DATA = r"C:\dev\d2r\base-files\data\data"








ITEM_CODE_PREFIX = "m"
THEME_CODE_LETTERS = {
    "desert": "d",
    "kurast": "k",
    "catacombs": "c",
    "frozen": "f",
    "worldstone": "w",
    "dunes": "s", "highlands": "h", "travincal": "t", "steppes": "e", "infernal": "i",
}


THEMES += [
    dict(key="dunes", name="Sunscar Dunes", body_template=42, exterior=True,
         body_size=(80,80), body_entries=[(0,33),(1,34),(2,35),(3,36)], body_entry=(0,33),


         body_exits=[(7,33)], exit_preset=388, exit_files=1, initializer=0x3fbd10,
         arena_template=138, arena_ds1="custom:dunes", arena_return=(2,83)),
    dict(key="highlands", name="Forsaken Highlands", body_template=7, exterior=True,
         body_size=(80,80), body_entries=[(3,0),(4,1),(5,2),(6,3)], body_entry=(3,0),





         body_exits=[(7,2)], exit_preset=52, exit_files=1, initializer=0x3fac10,




         entry_preset=51,



         arena_template=158, arena_ds1="custom:highlands", arena_return=(2,83)),
    dict(key="travincal", name="Fallen Travincal", body_template=83, exterior=True,
         body_size=(64,64), body_entry=(6,83), body_exits=[(0,64)], initializer=0x3fcbe0,



         arena_template=148, arena_ds1="custom:travincal", arena_return=(2,83)),
    dict(key="steppes", name="Ashen Steppes", body_template=104, exterior=True,


         body_size=(80,64), body_entry=(1,69), body_exits=[(7,69)], exit_preset=811, entry_preset=811,



         initializer=0x3fdc00, arena_template=165, arena_ds1="custom:steppes", arena_return=(0,83)),









    dict(key="infernal", name="Infernal Rift", body_template=125, exterior=True,
         body_level_type=28, body_size=(200,200), body_rooms=(22,22,22), body_rooms_per_tier=2,
         body_entry=(6,70), body_exits=[(7,70)], stairs_preset=852, initializer=0,



         arena_template=165, arena_ds1="custom:infernal", arena_return=(0,83)),
]



MAP_ITEM_LEVEL = {1: 70, 2: 76, 3: 81, 4: 84, 5: 87, 6: 90}





CURRENCY = [
    {"code": "mor", "name": "Horadric Sextant", "asset": "map/horadric_sextant",
     "desc": ["Cube with a Tier 1-4 Map", "to upgrade it to the next tier"], "level": 70},
    {"code": "mws", "name": "Horadric Astrolabe", "asset": "map/horadric_astrolabe",
     "desc": ["Cube with a Tier 5 Map", "to corrupt it into Tier 6"], "level": 87},
    {"code": "mrl", "name": "Vizjerei Prism", "asset": "map/vizjerei_prism",
     "desc": ["Cube with a Map", "to reroll its modifiers"], "level": 70},
]


def currency_desc_key(code: str) -> str:
    return f"{code}Description"




















AFFIX_PREFIXES = [
    {
        "key": "teeming", "display": "Teeming",
        "kind": "world", "strength": 2, "tiers": [1, 2, 3, 4, 5, 6],
        "density_bonus": 250, "rarity_bonus": 0,
        "props": [],
    },
    {
        "key": "swarming", "display": "Swarming",
        "kind": "world", "strength": 4, "tiers": [3, 4, 5, 6],
        "density_bonus": 500, "rarity_bonus": 1,
        "props": [],
    },
    {
        "key": "storied", "display": "Storied",
        "kind": "world", "strength": 3, "tiers": [2, 3, 4, 5, 6],
        "density_bonus": 0, "rarity_bonus": 4,
        "props": [],
    },
    {
        "key": "legendary", "display": "Legendary",
        "kind": "world", "strength": 5, "tiers": [4, 5, 6],
        "density_bonus": 100, "rarity_bonus": 8,
        "props": [],
    },
    {
        "key": "sapping", "display": "Sapping",
        "kind": "player", "strength": 3, "tiers": [1, 2, 3, 4, 5, 6],
        "density_bonus": 0, "rarity_bonus": 0,

        "props": [("dmg%", 0, -15, -15)],
    },
    {
        "key": "withering", "display": "Withering",
        "kind": "player", "strength": 5, "tiers": [3, 4, 5, 6],
        "props": [("dmg%", 0, -30, -30)],
        "density_bonus": 0, "rarity_bonus": 0,
    },
    {
        "key": "unhallowed", "display": "Unhallowed",
        "kind": "player", "strength": 4, "tiers": [2, 3, 4, 5, 6],


        "props": [("allskills", 0, -2, -2)],
        "density_bonus": 0, "rarity_bonus": 0,
    },
]

AFFIX_SUFFIXES = [
    {
        "key": "conviction", "display": "of Conviction",
        "kind": "aura", "strength": 6, "tiers": [3, 4, 5, 6],
        "aura_skill": "Conviction", "aura_level": 10,
        "density_bonus": 0, "rarity_bonus": 2,
        "props": [],
    },
    {
        "key": "might", "display": "of Might",
        "kind": "aura", "strength": 4, "tiers": [2, 3, 4, 5, 6],
        "aura_skill": "Might", "aura_level": 12,
        "density_bonus": 0, "rarity_bonus": 1,
        "props": [],
    },
    {
        "key": "fanaticism", "display": "of Fanaticism",
        "kind": "aura", "strength": 6, "tiers": [4, 5, 6],
        "aura_skill": "Fanaticism", "aura_level": 10,
        "density_bonus": 0, "rarity_bonus": 2,
        "props": [],
    },
    {
        "key": "frailty", "display": "of Frailty",
        "kind": "player", "strength": 4, "tiers": [1, 2, 3, 4, 5, 6],

        "props": [("res-all", 0, -30, -30)],
        "density_bonus": 0, "rarity_bonus": 0,
    },
    {
        "key": "ruin", "display": "of Ruin",
        "kind": "player", "strength": 6, "tiers": [4, 5, 6],
        "props": [("res-all", 0, -60, -60)],
        "density_bonus": 0, "rarity_bonus": 0,
    },
    {
        "key": "agony", "display": "of Agony",
        "kind": "player", "strength": 5, "tiers": [3, 4, 5, 6],




        "props": [("red-dmg%", 0, -25, -25)],
        "density_bonus": 0, "rarity_bonus": 0,
    },
]








MONSTER_AURA_CLONES = {
    "Conviction": "rmap_conviction",
    "Might": "rmap_might",
    "Fanaticism": "rmap_fanaticism",
}


def monster_aura(skill):
    return MONSTER_AURA_CLONES.get(skill, skill)




STRENGTH_DENSITY_PER_POINT = 40
STRENGTH_RARITY_PER_POINT = 1















TEST_RECIPES = False









MAP_MONSTERS = {

    "desert": ["clawviper5", "unraveler5", "sandmaggot5", "swarm4", "scarab5",
               "mummy4", "skmage_pois5", "batdemon5", "wraith5"],

    "kurast": ["councilmember3", "vampire4", "blunderbore4", "bonefetish5",
               "fetishshaman5", "frogdemon3", "thornhulk4", "zealot3"],



    "catacombs": ["unraveler5", "mummy5", "sk_archer5", "vampire5", "bighead4",
                  "skmage_fire4", "zombie5", "arach4", "corruptrogue4"],
    "frozen": ["frozenhorror5", "succubus5", "snowyeti4", "skmage_cold5",
               "succubuswitch6", "bloodlord3", "wraith6", "cr_lancer7", "willowisp3"],
    "worldstone": ["hellbovine", "willowisp3", "minion11", "bloodlord5", "vampire7",
                   "dkmag2", "skmage_ltng6", "cr_lancer8", "wraith8"],
    "dunes": ["scarab5", "sandleaper5", "vulture4", "pantherwoman5", "slinger5",
              "sandmaggot4", "brute4", "sandraider5"],
    "highlands": ["goatman5", "corruptrogue5", "cr_archer5", "cr_lancer5", "fallen5",
                  "fallenshaman5", "skmage_fire6", "brute5", "quillrat5"],
    "travincal": ["councilmember3", "zealot3", "cantor3", "baboon5", "fetish5",
                  "fetishblow5", "arach5", "mosquito3", "vampire4"],
    "steppes": ["megademon1", "vilemother1", "fingermage1", "doomknight1", "doomknight2",
                "bighead5", "batdemon4", "regurgitator1"],
    "infernal": ["minion1", "succubus4", "overseer1", "megademon4", "blunderbore6",
                 "vampire6", "bonefetish6", "skmage_ltng5", "imp5"],
}



MAP_KEEP_SPAWN = {"sandmaggot4", "sandmaggot5"}

MAP_MONSTER_LIMIT = 10
MAP_AREA_LEVEL = 99























MAP_AIDIST = 50
AI_PROFILES = {

    "AbyssKnight": dict(aip={2: 30, 3: 100, 7: 90, 8: 10}, speed=(10, 10)),

    "Arach": dict(aip={1: 100, 2: 0, 3: 100, 4: 0, 5: 0}, speed=(12, 16)),

    "Baboon": dict(aip={2: 10, 3: 100}, speed=(10, 14)),

    "BatDemon": dict(aip={2: 5, 3: 100}, speed=(12, 12)),

    "Bighead": dict(aip={1: 100, 2: 0, 3: 100, 4: 100}, speed=(8, 14)),

    "BloodLord": dict(aip={2: 100, 3: 100, 4: 5}, speed=(13, 16)),

    "Brute": dict(aip={3: 100}, speed=(10, 12)),

    "ClawViper": dict(aip={3: 100}, speed=(10, 14)),

    "CorruptArcher": dict(aip={4: 10}, speed=(8, 12)),

    "CorruptLancer": dict(aip={1: 100, 4: 75}, speed=(8, 12)),

    "CorruptRogue": dict(aip={1: 100, 3: 100}, speed=(8, 12)),

    "DoomKnight": dict(aip={3: 100, 4: 5}, speed=(9, 10)),

    "Fallen": dict(aip={1: 100, 2: 0, 3: 100}, speed=(10, 10)),

    "FallenShaman": dict(aip={3: 100, 5: 30}, speed=(10, 10)),

    "Fetish": dict(aip={2: 5, 4: 50}, speed=(12, 14)),

    "FetishBlowgun": dict(aip={2: 5}, speed=(12, 12)),

    "FetishShaman": dict(aip={4: 10}, speed=(12, 12)),

    "FingerMage": dict(aip={1: 80, 2: 100, 3: 100, 4: 1, 5: 30, 6: 1, 8: 5}, speed=(10, 16)),

    "FrogDemon": dict(aip={3: 10, 4: 10, 7: 5}, speed=(12, 12)),

    "FrozenHorror": dict(aip={1: 100, 2: 100}, speed=(9, 9)),

    "Goatman": dict(aip={1: 100, 2: 5, 3: 100}, speed=(11, 11)),

    "GreaterMummy": dict(aip={2: 60, 4: 100}, speed=(9, 9)),

    "HighPriest": dict(aip={3: 30}, speed=(10, 12)),

    "Imp": dict(aip={1: 5, 2: 10, 3: 100, 4: 100}, speed=(12, 12)),

    "Megademon": dict(aip={2: 80, 4: 100, 5: 0, 6: 15}, speed=(13, 16)),

    "Minion": dict(aip={3: 100, 4: 8}, speed=(10, 12)),

    "Mosquito": dict(aip={3: 100}, speed=(9, 12)),

    "Mummy": dict(aip={1: 30, 2: 100}, speed=(7, 7)),

    "OblivionKnight": dict(aip={2: 40, 5: 100, 6: 50, 7: 100, 8: 20}, speed=(10, 12)),

    "Overseer": dict(aip={}, speed=(7, 7)),

    "PantherJavelin": dict(aip={1: 100, 2: 100, 6: 40}, speed=(10, 15)),

    "PantherWoman": dict(aip={1: 100, 2: 100}, speed=(12, 15)),

    "PinHead": dict(aip={3: 100}, speed=(10, 10)),

    "QuillRat": dict(aip={1: 35}, speed=(8, 8)),

    "Regurgitator": dict(aip={2: 5, 3: 100, 4: 10, 5: 0}, speed=(9, 12)),

    "SandLeaper": dict(aip={1: 50, 3: 100, 4: 10}, speed=(11, 11)),

    "SandMaggot": dict(aip={}, speed=(4, 4)),

    "SandRaider": dict(aip={1: 15, 2: 5, 4: 100, 5: 5}, speed=(12, 12)),

    "Scarab": dict(aip={1: 100, 3: 15}, speed=(9, 9)),

    "Skeleton": dict(aip={}, speed=(12, 12)),

    "SkeletonBow": dict(aip={1: 100, 2: 5, 3: 100, 4: 1, 5: 3}, speed=(9, 9)),

    "SkeletonMage": dict(aip={1: 100, 2: 30, 3: 40, 5: 20, 6: 100, 7: 0}, speed=(8, 8)),

    "Succubus": dict(aip={3: 80, 4: 40, 6: 10}, speed=(13, 13)),

    "SuccubusWitch": dict(aip={2: 60, 3: 80, 6: 8}, speed=(12, 12)),

    "Swarm": dict(aip={1: 100, 2: 6, 3: 100}, speed=(10, 10)),

    "ThornHulk": dict(aip={2: 15, 3: 0}, speed=(8, 10)),

    "Vampire": dict(aip={2: 75, 3: 40}, speed=(12, 12)),

    "VileMother": dict(aip={6: 10}, speed=(9, 9)),

    "Vulture": dict(aip={2: 5, 4: 5, 5: 100}, speed=(9, 9)),

    "WillOWisp": dict(aip={3: 80}, speed=(17, 17)),

    "Wraith": dict(aip={3: 100}, speed=(10, 14)),

    "ZakarumPriest": dict(aip={2: 40, 5: 50}, speed=(10, 10)),

    "ZakarumZealot": dict(aip={1: 100, 3: 30}, speed=(12, 14)),

    "Zombie": dict(aip={2: 40}, speed=(5, 10)),
}




































WARDENS = {



    "dunes": dict(aura=None, on_attack=None,
                  escort=("scarab5","sandraider5",3,4), melee=("ltng",80,180,0), drain=None),



    "highlands": dict(aura=None, on_attack=None,
                      escort=("goatman5","corruptrogue5",3,5), melee=None, drain=None, body="cr_archer5"),




    "travincal": dict(aura=("Fanaticism",6), on_attack=None,
                      escort=("zealot3","baboon5",3,5), melee=("ltng",90,160,0), drain=None),




    "steppes": dict(aura=None, on_attack=None,
                    escort=("megademon1","fingermage1",3,4), melee=("fire",100,180,0), drain=None,
                    body="doomknight2"),



    "infernal": dict(aura=None, on_attack=None, escort=("minion1","imp5",4,6),
                     melee=("fire",120,190,0), drain=None, body="megademon4", model_scale=1.4),
    "desert": {



        "aura": None,
        "on_attack": None,
        "escort": ("clawviper5", "unraveler5", 2, 3),
        "melee": ("cold", 110, 185, 150),
        "drain": None,
    },
    "kurast": {





        "aura": None,
        "on_attack": None,
        "escort": ("zealot3", "blunderbore4", 2, 3),
        "melee": ("fire", 90, 160, 0),
        "drain": None,
    },
    "catacombs": {
        "aura": None,




        "on_attack": None,
        "escort": ("unraveler5", "mummy5", 3, 5),
        "melee": None,
        "drain": None,
    },
    "frozen": {




        "aura": None,
        "on_attack": None,
        "escort": ("frozenhorror5", "snowyeti4", 2, 4),
        "melee": ("cold", 90, 160, 75),
        "drain": None,
        "body": "succubuswitch6",
    },
    "worldstone": {



        "aura": None,
        "on_attack": None,
        "escort": ("hellbovine", "willowisp3", 4, 6),
        "melee": None,
        "drain": None,
    },
}







WARDEN_HP_RATIO = (300, 360)
WARDEN_HP_MULTIPLIER = 9


WARDEN_DAMAGE_MULTIPLIER = 1.2



WARDEN_ACCURACY_BASE = 1.2
WARDEN_ACCURACY_PER_TIER = 0.1




WARDEN_HASTE_PERCENT = 56





WARDEN_PACE = 1.3
WARDEN_HASTE_SKILL = "rmap_warden_haste"
WARDEN_HASTE_SLOT = 6


WARDEN_DAMAGE_REGEN = 0
WARDEN_UTRANS = 3



WARDEN_MODEL_SCALE = 2.0
WARDEN_AURA_SLOT = 4
WARDEN_PROC_SLOT = 5
WARDEN_AURA_PER_TIER = 2
WARDEN_SKILL_PER_TIER = 4
WARDEN_ESCORT_PER_TIER = 1
WARDEN_TIER6_ESCORT_BONUS = 2


WARDEN_RESIST_CAP = 75


def warden_body(theme_key):
    return WARDENS[theme_key].get("body", MAP_MONSTERS[theme_key][0])
MAP_MF_PER_TIER = 25
MAP_MF_PER_STRENGTH = 5


AFFIX_FAMILIES = [0, 0, 1, 1, 2, 2, 3, 4, 5, 6, 7, 7, 8]



EXPANSION_ITEM_PREFIX = "x"
LEGACY_AFFIX_COUNT = 13
EXPANSION_AFFIXES = [
    dict(key="bovine", display="Bovine Incursion", kind="population", strength=2,
         tiers=[1, 2, 3, 4, 5, 6], family=9, population=1, weight=10,
         description="Bovine Incursion: some native packs replaced by Hell Bovines"),
    dict(key="coven", display="Witch Coven", kind="population", strength=3,
         tiers=[2, 3, 4, 5, 6], family=9, population=2, weight=10,
         description="Witch Coven: some native packs replaced by Succubi"),
    dict(key="legion", display="Restless Legion", kind="population", strength=2,
         tiers=[1, 2, 3, 4, 5, 6], family=9, population=3, weight=10,
         description="Restless Legion: some native packs replaced by skeletal archers"),
    dict(key="frenzy", display="Blood Frenzy", kind="monster", strength=3,
         tiers=[2, 3, 4, 5, 6], family=10, weight=8,
         monster_props=[("move2", 0, 15, 15), ("swing2", 0, 15, 15)],
         description="Blood Frenzy: Monsters gain 15% movement and attack speed"),
    dict(key="fire_pact", display="Flame Pact", kind="monster", strength=3,
         tiers=[1, 2, 3, 4, 5, 6], family=11, weight=4,
         monster_props=[("dmg-fire", 0, 20, 40)], scale_with_tier=True,
         description="Flame Pact: Monsters add 20-40 fire attack damage per tier"),
    dict(key="cold_pact", display="Frost Pact", kind="monster", strength=3,
         tiers=[1, 2, 3, 4, 5, 6], family=11, weight=4,
         monster_props=[("dmg-cold", 25, 15, 30)], scale_with_tier=True,
         description="Frost Pact: Monsters add 15-30 cold attack damage per tier"),
    dict(key="light_pact", display="Storm Pact", kind="monster", strength=3,
         tiers=[1, 2, 3, 4, 5, 6], family=11, weight=4,
         monster_props=[("dmg-ltng", 0, 5, 60)], scale_with_tier=True,
         description="Storm Pact: Monsters add 5-60 lightning attack damage per tier"),
    dict(key="gilded", display="Gilded", kind="reward", strength=0,
         tiers=[1, 2, 3, 4, 5, 6], family=12, reward=1, weight=8,
         description="Gilded: elite bonus drop chance for gold, gems and mapping currency"),
    dict(key="artificer", display="Artificer's", kind="reward", strength=0,
         tiers=[1, 2, 3, 4, 5, 6], family=12, reward=2, weight=8,
         description="Artificer's: elite bonus drop chance for gems, jewels and equipment"),
]



EXPANSION_POPULATIONS = [None, "hellbovine", "succubus5", "sk_archer5"]
EXPANSION_REWARDS = [None, "Gilded", "Artificer"]



TREASURE_MONSTERS = [
    dict(key="jewel", name="Jewel Hoarder", source="goatman5", palette=8,
         item="jew", rare=0),
    dict(key="amulet", name="Amulet Collector", source="corruptrogue5", palette=12,
         item="amu", rare=1024),

    dict(key="gem", name="Gem Smuggler", source="sandraider5", palette=5,
         item="Gem", rare=0),
]
TREASURE_DROPS = 6
TREASURE_DEFAULT_CHANCE = 8
TREASURE_RELEASE_CHANCE = 8


def all_affixes():
    return AFFIX_PREFIXES + AFFIX_SUFFIXES + EXPANSION_AFFIXES


def expansion_code(code):
    return EXPANSION_ITEM_PREFIX + code[1:]
















LEVEL_NAME_TWO_LINE = True
