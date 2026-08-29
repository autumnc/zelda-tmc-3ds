#include "port_cheats.h"
#include "port_gba_mem.h"

#ifdef TMC_3DS
#include "save.h"
#include "entity.h"
#include "player.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Cheat definitions are parsed once at boot, before gameplay, then treated as
 * read-only by the bottom-screen paint thread. Only the cursor and the
 * open/closed flag change after boot; both are atomics like the 3DS config
 * values, since the paint thread reads them from a different core. */
static PortCheatDef sCheats[PORT_CHEAT_MAX];
static int sCheatCount;
static _Atomic int sCursor;
static _Atomic bool sActive;

static int NewCheat(const char* name) {
    if (sCheatCount >= PORT_CHEAT_MAX) return -1;
    PortCheatDef* d = &sCheats[sCheatCount];
    memset(d, 0, sizeof *d);
    snprintf(d->name, sizeof d->name, "%s", (name && *name) ? name : "(cheat)");
    return sCheatCount++;
}

static void AddWrite(int idx, uint32_t addr, uint32_t value, uint8_t width) {
    if (idx < 0 || idx >= sCheatCount) return;
    PortCheatDef* d = &sCheats[idx];
    if (d->count >= PORT_CHEAT_WRITES_MAX) return;
    if (addr < 0x02000000u || addr >= 0x0E000000u) return;
    d->addr[d->count] = addr;
    d->value[d->count] = value;
    d->width[d->count] = width;
    d->count++;
}

/* Offset of an 8-hex "xxxxxxxx:" address token in s, or -1. */
static int FindAddrToken(const char* s) {
    for (const char* c = s; *c != '\0'; ++c) {
        if (*c != ':') continue;
        int i = (int)(c - s) - 8;
        if (i < 0) continue;
        int ok = 1;
        for (int k = 0; k < 8; ++k) {
            if (!isxdigit((unsigned char)s[i + k])) {
                ok = 0;
                break;
            }
        }
        if (ok) return i;
    }
    return -1;
}

/* Parse state shared by the file loader and the built-in list. */
typedef struct {
    int current;
    int havePending;
    char pendingName[PORT_CHEAT_NAME_MAX + 1];
} CheatParseState;

/* One trimmed cheats.txt-style line. Blank lines, `#` comments and pure value
 * tables ("00 无", "01 スミスの剣") self-discard; a plain name line is held
 * until the next write line commits it as a cheat name. */
static void ProcessCheatLine(CheatParseState* st, char* line) {
    char* nl = strpbrk(line, "\r\n");
    if (nl) *nl = '\0';
    char* t = line;
    while (*t == ' ' || *t == '\t') ++t;
    size_t len = strlen(t);
    while (len > 0 && (t[len - 1] == ' ' || t[len - 1] == '\t')) t[--len] = '\0';
    if (*t == '\0' || *t == '#') return;

    int aoff = FindAddrToken(t);
    if (aoff < 0) {
        /* Plain text: hold as a pending name. A following write line
         * commits it as a cheat name; another plain line overwrites it,
         * which is how reference tables never become cheats. */
        st->havePending = 1;
        snprintf(st->pendingName, sizeof st->pendingName, "%.*s",
                 (int)(sizeof st->pendingName) - 1, t);
        return;
    }

    char prefix[PORT_CHEAT_NAME_MAX + 1] = "";
    int prefixLen = aoff;
    while (prefixLen > 0 && (t[prefixLen - 1] == ' ' || t[prefixLen - 1] == '\t')) --prefixLen;
    if (prefixLen > PORT_CHEAT_NAME_MAX) prefixLen = PORT_CHEAT_NAME_MAX;
    memcpy(prefix, t, (size_t)prefixLen);
    prefix[prefixLen] = '\0';

    const char* vstart = t + aoff + 9; /* past the colon */
    int vlen = (int)strlen(vstart);
    int vok = (vlen == 2 || vlen == 4 || vlen == 8);
    for (int k = 0; vok && k < vlen; ++k) {
        if (!isxdigit((unsigned char)vstart[k])) vok = 0;
    }

    if (!vok) {
        /* Address present but the value is a placeholder like "xx". */
        st->havePending = 0;
        st->current = NewCheat(prefix[0] ? prefix
                             : (st->current >= 0 ? sCheats[st->current].name : ""));
        if (st->current >= 0) sCheats[st->current].needValue = 1;
        return;
    }

    uint32_t addr = (uint32_t)strtoul(t + aoff, NULL, 16);
    uint32_t value = (uint32_t)strtoul(vstart, NULL, 16);
    uint8_t width = (uint8_t)(vlen / 2);

    if (prefix[0] != '\0') {
        /* "name ADDR:VALUE" — a new named cheat. */
        st->havePending = 0;
        st->current = NewCheat(prefix);
        AddWrite(st->current, addr, value, width);
    } else {
        /* "ADDR:VALUE" alone — extends the current cheat, or starts a
         * fresh one from a held-back name. */
        if (st->havePending) {
            st->current = NewCheat(st->pendingName);
            st->havePending = 0;
        }
        if (st->current < 0) {
            st->current = NewCheat("");
        }
        AddWrite(st->current, addr, value, width);
    }
}

/* The codes ship with the build so the menu works with no cheats.txt at all.
 * Names are ASCII because the menu renders through the port's English face
 * (the JP-ROM build disables the decoded CJK font). */
static const char kBuiltInCheats[] =
    "# Built-in cheat codes for The Minish Cap 3DS.\n"
    "# Names stay ASCII: the JP-ROM build disables the CJK font.\n"
    "# Add a cheats.txt next to the ROM to replace this list.\n"
    "\n"
    "HP 02002AEA:A0\n"
    "\n"
    "Max HP 02002AEB:A0\n"
    "\n"
    "Rupees 02002B00:03E7\n"
    "\n"
    "Rupees Max 02002AE8:03\n"
    "\n"
    "All Sword Skills 0300402C:FFFF\n"
    "\n"
    "All Scrolls 02002B44:FFFF\n"
    "\n"
    "Bomb Count 02002AEC:63\n"
    "\n"
    "Arrow Count 02002AED:63\n"
    "\n"
    "Shell Count 02002B02:03E7\n"
    "\n"
    "Key Count\n"
    "02002E9D:63\n"
    "02002E9E:6363\n"
    "02002EA0:63636363\n"
    "02002EA4:63636363\n"
    "02002EA8:63636363\n"
    "02002EAC:63\n"
    "\n"
    "Dungeon Items\n"
    "02002EAD:07\n"
    "02002EAE:0707\n"
    "02002EB0:07070707\n"
    "02002EB4:07070707\n"
    "02002EB8:07070707\n"
    "02002EBC:07\n"
    "\n"
    "A-Button Item 02002AF4:xx\n"
    "\n"
    "B-Button Item 02002AF5:xx\n"
    "\n"
    "Sword Type 02002B32:xx\n"
    "\n"
    "Partial Items (Careful) 02002B34:45545115\n"
    "\n"
    "Body Size 03003FB0:xx\n"
    "\n"
    "Full Map\n"
    "02002A80:FFFF\n"
    "02002A82:01\n"
    "\n"
    "All Figurines\n"
    "02002B0E:FFFF\n"
    "02002B10:FFFFFFFF\n"
    "02002B14:FFFFFFFF\n"
    "02002B18:FFFFFFFF\n"
    "02002B1C:FFFF\n"
    "02002B1E:FF\n";

void Port_CheatMenu_LoadBuiltIn(void) {
    sCheatCount = 0;
    sCursor = 0;
    CheatParseState st;
    memset(&st, 0, sizeof st);
    st.current = -1;
    for (const char* p = kBuiltInCheats; *p != '\0';) {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[256];
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        ProcessCheatLine(&st, line);
        p = nl ? nl + 1 : p + len;
    }
}

int Port_CheatMenu_LoadFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    sCheatCount = 0;
    sCursor = 0;
    CheatParseState st;
    memset(&st, 0, sizeof st);
    st.current = -1;
    char line[256];
    while (fgets(line, sizeof line, f) != NULL) {
        ProcessCheatLine(&st, line);
    }
    fclose(f);
    return sCheatCount;
}

int Port_CheatMenu_GetCount(void) {
    return sCheatCount;
}

const PortCheatDef* Port_CheatMenu_GetDef(int index) {
    return (index >= 0 && index < sCheatCount) ? &sCheats[index] : NULL;
}

int Port_CheatMenu_IsEnabled(int index) {
    if (index < 0 || index >= sCheatCount) return 0;
    return (int)((Port_Config_GetCheatEnabledMask() >> (unsigned)index) & 1u);
}

int Port_CheatMenu_Active(void) {
    return sActive ? 1 : 0;
}

int Port_CheatMenu_GetCursor(void) {
    return sCursor;
}

void Port_CheatMenu_ToggleOpen(void) {
    if (sCheatCount == 0) return; /* nothing to show */
    sActive = !sActive;
    if (sActive) sCursor = 0;
}

void Port_CheatMenu_HandleInput(uint32_t keysDown, uint32_t keysHeld) {
    (void)keysHeld;
    if (!sActive) return;
    const int count = sCheatCount;
    if (count == 0) return;

    if (keysDown & PORT_CHEAT_HID_KEY_UP) {
        sCursor = (sCursor - 1 + count) % count;
    } else if (keysDown & PORT_CHEAT_HID_KEY_DOWN) {
        sCursor = (sCursor + 1) % count;
    }

    if (keysDown & PORT_CHEAT_HID_KEY_A) {
        const PortCheatDef* d = &sCheats[sCursor];
        if (!d->needValue) {
            uint32_t mask = Port_Config_GetCheatEnabledMask();
            mask ^= (1u << (unsigned)sCursor);
            Port_Config_SetCheatEnabledMask(mask);
        }
    }
    if (keysDown & PORT_CHEAT_HID_KEY_B) {
        sActive = false;
    }
}

/* The port relocates the game's EWRAM/IWRAM globals to plain C symbols
 * instead of keeping the original hardware layout, so a raw VBA address
 * (e.g. 0x02002AEA for HP) no longer points at the live data — writes land
 * on the unused gEwram/gIwram backing arrays and do nothing. The addresses
 * below are the ORIGINAL GBA bases from linker.ld; an address that falls
 * inside one of these ranges is re-based onto the port's current variable,
 * anything else falls through to the plain array mapping in gba_TryMemPtr.
 * The byte offsets inside each struct are unchanged by the port. */
#ifdef TMC_3DS
extern u32 gRand; /* gameplay PRNG seed, relocated out of IWRAM 0x03001150 */

typedef struct {
    uint32_t gbaBase;
    void* portPtr;
    uint32_t size;
} PortCheatRemap;

static const PortCheatRemap sCheatRemaps[] = {
    { 0x02002A40u, &gSave, sizeof(gSave) },         /* linker.ld: ewram   */
    { 0x03003F80u, &gPlayerState, sizeof(gPlayerState) }, /* iwram */
    { 0x03001160u, &gPlayerEntity, sizeof(gPlayerEntity) }, /* iwram */
    { 0x030015A0u, gEntities, sizeof(gEntities) },  /* iwram, 72 * 0x88   */
    { 0x03004040u, gPlayerClones, 3 * (uint32_t)sizeof(Entity*) }, /* iwram, 3 ptrs */
    { 0x03001150u, &gRand, sizeof(gRand) },         /* iwram, PRNG seed   */
};

static void* CheatResolvePtr(uint32_t addr) {
    for (uint32_t i = 0; i < (uint32_t)(sizeof(sCheatRemaps) / sizeof(sCheatRemaps[0])); ++i) {
        const PortCheatRemap* r = &sCheatRemaps[i];
        if (addr >= r->gbaBase && addr < r->gbaBase + r->size) {
            return (u8*)r->portPtr + (addr - r->gbaBase);
        }
    }
    return gba_TryMemPtr(addr);
}
#else
static void* CheatResolvePtr(uint32_t addr) {
    return gba_TryMemPtr(addr);
}
#endif

void Port_CheatMenu_ApplyFrame(void) {
    if (sCheatCount == 0) return;
    uint32_t mask = Port_Config_GetCheatEnabledMask();
    if (mask == 0) return;
    for (int i = 0; i < sCheatCount; ++i) {
        if (((mask >> (unsigned)i) & 1u) == 0) continue;
        const PortCheatDef* d = &sCheats[i];
        for (int w = 0; w < d->count; ++w) {
            void* p = CheatResolvePtr(d->addr[w]);
            if (!p) continue;
            if (d->width[w] == 1) {
                *(volatile uint8_t*)p = (uint8_t)d->value[w];
            } else if (d->width[w] == 2) {
                uint16_t v = (uint16_t)d->value[w];
                memcpy(p, &v, 2);
            } else {
                uint32_t v = d->value[w];
                memcpy(p, &v, 4);
            }
        }
    }
}
