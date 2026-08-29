#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PORT_CHEAT_MAX 64
#define PORT_CHEAT_NAME_MAX 39
#define PORT_CHEAT_WRITES_MAX 16

/* hid key bits (libctru layout) that the cheat menu navigates with. Kept
 * local so the module stays compilable outside the 3DS include tree. */
#define PORT_CHEAT_HID_KEY_A 0x0001u
#define PORT_CHEAT_HID_KEY_B 0x0002u
#define PORT_CHEAT_HID_KEY_UP 0x0040u
#define PORT_CHEAT_HID_KEY_DOWN 0x0080u

typedef struct {
    char name[PORT_CHEAT_NAME_MAX + 1];
    uint32_t addr[PORT_CHEAT_WRITES_MAX];
    uint32_t value[PORT_CHEAT_WRITES_MAX];
    uint8_t width[PORT_CHEAT_WRITES_MAX]; /* 1, 2 or 4 bytes */
    uint8_t count;                        /* number of write entries */
    uint8_t needValue;                    /* placeholder value (e.g. "xx") left unfilled */
} PortCheatDef;

/* Load cheats from a text file (raw VBA "ADDR:VALUE" codes). Returns the
 * number of cheats parsed, or 0 when the file could not be opened. */
int Port_CheatMenu_LoadFile(const char* path);

/* Seed the menu with the built-in code list. Call at boot; LoadFile above is
 * an optional override for power users and is a no-op when no file exists. */
void Port_CheatMenu_LoadBuiltIn(void);

int Port_CheatMenu_GetCount(void);
const PortCheatDef* Port_CheatMenu_GetDef(int index);
int Port_CheatMenu_IsEnabled(int index);

/* Enabled state is persisted by the 3DS config module. */
uint32_t Port_Config_GetCheatEnabledMask(void);
void Port_Config_SetCheatEnabledMask(uint32_t mask);

/* Menu state + input. */
int Port_CheatMenu_Active(void);
void Port_CheatMenu_ToggleOpen(void);
int Port_CheatMenu_GetCursor(void);
void Port_CheatMenu_HandleInput(uint32_t keysDown, uint32_t keysHeld);

/* Write every enabled cheat's values into GBA memory. Called once per engine
 * frame while the menu is closed. */
void Port_CheatMenu_ApplyFrame(void);
