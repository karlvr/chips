#pragma once
/*#
    # ui_c64.h

    Integrated debugging UI for c64.h

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C++ file to create the 
    implementation.

    Optionally provide the following macros with your own implementation
    
    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    Include the following headers (and their depenencies) before including
    ui_atom.h both for the declaration and implementation.

    - c64.h
    - mem.h
    - ui_chip.h
    - ui_util.h
    - ui_m6502.h
    - ui_m6526.h
    - ui_m6569.h
    - ui_m6581.h
    - ui_audio.h
    - ui_dasm.h
    - ui_memedit.h
    - ui_memmap.h
    - ui_kbd.h

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution. 
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* reboot callback */
typedef void (*ui_c64_boot_cb)(c64_t* sys);

/* setup params for ui_c64_init() */
typedef struct {
    c64_t* c64;             /* pointer to c64_t instance to track */
    ui_c64_boot_cb boot_cb; /* reboot callback function */
} ui_c64_desc_t;

typedef struct {
    c64_t* c64;
    ui_c64_boot_cb boot_cb;
    ui_m6502_t cpu;
    ui_audio_t audio;
    ui_kbd_t kbd;
    ui_memmap_t memmap;
    ui_memedit_t memedit[4];
    ui_dasm_t dasm[4];
} ui_c64_t;

void ui_c64_init(ui_c64_t* ui, const ui_c64_desc_t* desc);
void ui_c64_discard(ui_c64_t* ui);
void ui_c64_draw(ui_c64_t* ui, double time_ms);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION (include in C++ source) ----------------------------------*/
#ifdef CHIPS_IMPL
#ifndef __cplusplus
#error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

static void _ui_c64_draw_menu(ui_c64_t* ui, double time_ms) {
    CHIPS_ASSERT(ui && ui->c64 && ui->boot_cb);
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("System")) {
            if (ImGui::MenuItem("Reset")) {
                c64_reset(ui->c64);
            }
            if (ImGui::MenuItem("Cold Boot")) {
                ui->boot_cb(ui->c64);
            }
            if (ImGui::BeginMenu("Joystick")) {
                if (ImGui::MenuItem("None", 0, ui->c64->joystick_type == C64_JOYSTICKTYPE_NONE)) {
                    ui->c64->joystick_type = C64_JOYSTICKTYPE_NONE;
                }
                if (ImGui::MenuItem("Digital #1", 0, ui->c64->joystick_type == C64_JOYSTICKTYPE_DIGITAL_1)) {
                    ui->c64->joystick_type = C64_JOYSTICKTYPE_DIGITAL_1;
                }
                if (ImGui::MenuItem("Digital #2", 0, ui->c64->joystick_type == C64_JOYSTICKTYPE_DIGITAL_2)) {
                    ui->c64->joystick_type = C64_JOYSTICKTYPE_DIGITAL_2;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Hardware")) {
            ImGui::MenuItem("Memory Map", 0, &ui->memmap.open);
            ImGui::MenuItem("Keyboard Matrix", 0, &ui->kbd.open);
            ImGui::MenuItem("Audio Output", 0, &ui->audio.open);
            ImGui::MenuItem("MOS 6510 (CPU)", 0, &ui->cpu.open);
            ImGui::MenuItem("MOS 6526 #1 (CIA-1) TODO!");
            ImGui::MenuItem("MOS 6526 #2 (CIA-2) TODO!");
            ImGui::MenuItem("MOS 6581 (SID) TODO!");
            ImGui::MenuItem("MOS 6569 (VIC-II) TODO!");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::BeginMenu("Memory Editor")) {
                ImGui::MenuItem("Window #1", 0, &ui->memedit[0].open);
                ImGui::MenuItem("Window #2", 0, &ui->memedit[1].open);
                ImGui::MenuItem("Window #3", 0, &ui->memedit[2].open);
                ImGui::MenuItem("Window #4", 0, &ui->memedit[3].open);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Disassembler")) {
                ImGui::MenuItem("Window #1", 0, &ui->dasm[0].open);
                ImGui::MenuItem("Window #2", 0, &ui->dasm[1].open);
                ImGui::MenuItem("Window #3", 0, &ui->dasm[2].open);
                ImGui::MenuItem("Window #4", 0, &ui->dasm[3].open);
                ImGui::EndMenu();
            }
            ImGui::MenuItem("CPU Debugger (TODO)");
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 120);
        ImGui::Text("emu: %.2fms", time_ms);
        ImGui::EndMainMenuBar();
    }
}

/* keep the layers with code at the start */
#define _UI_C64_MEMLAYER_CPU    (0)     /* CPU visible mapping */
#define _UI_C64_MEMLAYER_RAM    (1)     /* RAM blocks */
#define _UI_C64_MEMLAYER_ROM    (2)     /* ROM blocks */
#define _UI_C64_MEMLAYER_VIC    (3)     /* VIC visible mapping */
#define _UI_C64_MEMLAYER_COLOR  (4)     /* special static color RAM */
#define _UI_C64_CODELAYER_NUM   (3)     /* first 3 layers can contain code */
#define _UI_C64_MEMLAYER_NUM    (5)

static const char* _ui_c64_memlayer_names[_UI_C64_MEMLAYER_NUM] = {
    "CPU Mapped", "RAM Banks", "ROM Banks", "VIC Mapped", "Color RAM"
};

static uint8_t _ui_c64_mem_read(int layer, uint16_t addr, void* user_data) {
    CHIPS_ASSERT(user_data);
    c64_t* c64 = (c64_t*) user_data;
    switch (layer) {
        case _UI_C64_MEMLAYER_CPU:
            return mem_rd(&c64->mem_cpu, addr);
        case _UI_C64_MEMLAYER_RAM:
            return c64->ram[addr];
        case _UI_C64_MEMLAYER_ROM:
            if ((addr >= 0xA000) && (addr < 0xC000)) {
                /* BASIC ROM */
                return c64->rom_basic[addr - 0xA000];
            }
            else if ((addr >= 0xD000) && (addr < 0xE000)) {
                /* Character ROM */
                return c64->rom_char[addr - 0xD000];
            }
            else if ((addr >= 0xE000) && (addr <= 0xFFFF)) {
                /* Kernal ROM */
                return c64->rom_kernal[addr - 0xE000];
            }
            else {
                return 0xFF;
            }
            break;
        case _UI_C64_MEMLAYER_VIC:
            return mem_rd(&c64->mem_vic, addr);
        case _UI_C64_MEMLAYER_COLOR:
            if ((addr >= 0xD800) && (addr < 0xDC00)) {
                /* static COLOR RAM */
                return c64->color_ram[addr - 0xD800];
            }
            else {
                return 0xFF;
            }
        default:
            return 0xFF;
    }
}

static void _ui_c64_mem_write(int layer, uint16_t addr, uint8_t data, void* user_data) {
    CHIPS_ASSERT(user_data);
    c64_t* c64 = (c64_t*) user_data;
    switch (layer) {
        case _UI_C64_MEMLAYER_CPU:
            mem_wr(&c64->mem_cpu, addr, data);
            break;
        case _UI_C64_MEMLAYER_RAM:
            c64->ram[addr] = data;
            break;
        case _UI_C64_MEMLAYER_ROM:
            if ((addr >= 0xA000) && (addr < 0xC000)) {
                /* BASIC ROM */
                c64->rom_basic[addr - 0xA000] = data;
            }
            else if ((addr >= 0xD000) && (addr < 0xE000)) {
                /* Character ROM */
                c64->rom_char[addr - 0xD000] = data;
            }
            else if ((addr >= 0xE000) && (addr <= 0xFFFF)) {
                /* Kernal ROM */
                c64->rom_kernal[addr - 0xE000] = data;
            }
            break;
        case _UI_C64_MEMLAYER_VIC:
            mem_wr(&c64->mem_vic, addr, data);
            break;
        case _UI_C64_MEMLAYER_COLOR:
            if ((addr >= 0xD800) && (addr < 0xDC00)) {
                /* static COLOR RAM */
                c64->color_ram[addr - 0xD800] = data;
            }
            break;
    }
}

static void _ui_c64_update_memmap(ui_c64_t* ui) {
    CHIPS_ASSERT(ui && ui->c64);
    const c64_t* c64 = ui->c64;
    bool all_ram = (c64->cpu_port & (C64_CPUPORT_HIRAM|C64_CPUPORT_LORAM)) == 0;
    bool basic_rom = (c64->cpu_port & (C64_CPUPORT_HIRAM|C64_CPUPORT_LORAM)) == (C64_CPUPORT_HIRAM|C64_CPUPORT_LORAM);
    bool kernal_rom = (c64->cpu_port & C64_CPUPORT_HIRAM) != 0;
    bool io_enabled = !all_ram && ((c64->cpu_port & C64_CPUPORT_CHAREN) != 0);
    bool char_rom = !all_ram && ((c64->cpu_port & C64_CPUPORT_CHAREN) == 0);
    ui_memmap_reset(&ui->memmap);
    ui_memmap_layer(&ui->memmap, "IO");
        ui_memmap_region(&ui->memmap, "IO REGION", 0xD000, 0x1000, io_enabled);
    ui_memmap_layer(&ui->memmap, "ROM");
        ui_memmap_region(&ui->memmap, "BASIC ROM", 0xA000, 0x2000, basic_rom);
        ui_memmap_region(&ui->memmap, "CHAR ROM", 0xD000, 0x1000, char_rom);
        ui_memmap_region(&ui->memmap, "KERNAL ROM", 0xE000, 0x2000, kernal_rom);
    ui_memmap_layer(&ui->memmap, "RAM");
        ui_memmap_region(&ui->memmap, "RAM", 0x0000, 0x10000, true);
}

static const ui_chip_pin_t _ui_c64_cpu_pins[] = {
    { "D0",     0,      M6502_D0 },
    { "D1",     1,      M6502_D1 },
    { "D2",     2,      M6502_D2 },
    { "D3",     3,      M6502_D3 },
    { "D4",     4,      M6502_D4 },
    { "D5",     5,      M6502_D5 },
    { "D6",     6,      M6502_D6 },
    { "D7",     7,      M6502_D7 },
    { "RW",     9,      M6502_RW },
    { "RDY",    10,     M6502_RDY },
    { "AEC",    11,     M6510_AEC },
    { "IRQ",    12,     M6502_IRQ },
    { "NMI",    13,     M6502_NMI },
    { "P0",     15,     M6510_P0 },
    { "P1",     16,     M6510_P1 },
    { "P2",     17,     M6510_P2 },
    { "P3",     18,     M6510_P3 },
    { "P4",     19,     M6510_P4 },
    { "P5",     20,     M6510_P5 },
    { "A0",     21,     M6502_A0 },
    { "A1",     22,     M6502_A1 },
    { "A2",     23,     M6502_A2 },
    { "A3",     24,     M6502_A3 },
    { "A4",     25,     M6502_A4 },
    { "A5",     26,     M6502_A5 },
    { "A6",     27,     M6502_A6 },
    { "A7",     28,     M6502_A7 },
    { "A8",     29,     M6502_A8 },
    { "A9",     30,     M6502_A9 },
    { "A10",    31,     M6502_A10 },
    { "A11",    32,     M6502_A11 },
    { "A12",    33,     M6502_A12 },
    { "A13",    34,     M6502_A13 },
    { "A14",    35,     M6502_A14 },
    { "A15",    36,     M6502_A15 },
};

void ui_c64_init(ui_c64_t* ui, ui_c64_desc_t* desc) {
    CHIPS_ASSERT(ui && desc);
    CHIPS_ASSERT(desc->c64);
    CHIPS_ASSERT(desc->boot_cb);
    ui->c64 = desc->c64;
    ui->boot_cb = desc->boot_cb;
    int x = 20, y = 20, dx = 10, dy = 10;
    {
        ui_m6502_desc_t desc = {0};
        desc.title = "MOS 6510";
        desc.cpu = &ui->c64->cpu;
        desc.x = x;
        desc.y = y;
        desc.h = 390;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "6510", 42, _ui_c64_cpu_pins);
        ui_m6502_init(&ui->cpu, &desc);
    }
    x += dx; y += dy;
    {
        ui_audio_desc_t desc = {0};
        desc.title = "Audio Output";
        desc.sample_buffer = ui->c64->sample_buffer;
        desc.num_samples = ui->c64->num_samples;
        desc.x = x;
        desc.y = y;
        ui_audio_init(&ui->audio, &desc);
    }
    x += dx; y += dy;
    {
        ui_kbd_desc_t desc = {0};
        desc.title = "Keyboard Matrix";
        desc.kbd = &ui->c64->kbd;
        desc.layers[0] = "None";
        desc.layers[1] = "Shift";
        desc.layers[2] = "Ctrl";
        desc.x = x;
        desc.y = y;
        ui_kbd_init(&ui->kbd, &desc);
    }
    x += dx; y += dy;
    {
        ui_memedit_desc_t desc = {0};
        for (int i = 0; i < _UI_C64_MEMLAYER_NUM; i++) {
            desc.layers[i] = _ui_c64_memlayer_names[i];
        }
        desc.read_cb = _ui_c64_mem_read;
        desc.write_cb = _ui_c64_mem_write;
        desc.user_data = ui->c64;
        desc.h = 120;
        static const char* titles[] = { "Memory Editor #1", "Memory Editor #2", "Memory Editor #3", "Memory Editor #4" };
        for (int i = 0; i < 4; i++) {
            desc.title = titles[i]; desc.x = x; desc.y = y;
            ui_memedit_init(&ui->memedit[i], &desc);
            x += dx; y += dy;
        }
    }
    x += dx; y += dy;
    {
        ui_memmap_desc_t desc = {0};
        desc.title = "Memory Map";
        desc.x = x;
        desc.y = y;
        desc.w = 400;
        desc.h = 64;
        ui_memmap_init(&ui->memmap, &desc);
    }
    x += dx; y += dy;
    {
        ui_dasm_desc_t desc = {0};
        for (int i = 0; i < _UI_C64_CODELAYER_NUM; i++) {
            desc.layers[i] = _ui_c64_memlayer_names[i];
        }
        desc.cpu_type = UI_DASM_CPUTYPE_M6502;
        desc.start_addr = mem_rd16(&ui->c64->mem_cpu, 0xFFFC);
        desc.read_cb = _ui_c64_mem_read;
        desc.user_data = ui->c64;
        desc.w = 400;
        desc.h = 256;
        static const char* titles[4] = { "Disassembler #1", "Disassembler #2", "Disassembler #2", "Dissassembler #3" };
        for (int i = 0; i < 4; i++) {
            desc.title = titles[i]; desc.x = x; desc.y = y;
            ui_dasm_init(&ui->dasm[i], &desc);
            x += dx; y += dy;
        }
    }
}

void ui_c64_discard(ui_c64_t* ui) {
    CHIPS_ASSERT(ui && ui->c64);
    ui->c64 = 0;
    ui_m6502_discard(&ui->cpu);
    ui_kbd_discard(&ui->kbd);
    ui_audio_discard(&ui->audio);
    ui_memmap_discard(&ui->memmap);
    for (int i = 0; i < 4; i++) {
        ui_memedit_discard(&ui->memedit[i]);
        ui_dasm_discard(&ui->dasm[i]);
    }
}

void ui_c64_draw(ui_c64_t* ui, double time_ms) {
    CHIPS_ASSERT(ui && ui->c64);
    _ui_c64_draw_menu(ui, time_ms);
    if (ui->memmap.open) {
        _ui_c64_update_memmap(ui);
    }
    ui_audio_draw(&ui->audio, ui->c64->sample_pos);
    ui_kbd_draw(&ui->kbd);
    ui_m6502_draw(&ui->cpu);
    ui_memmap_draw(&ui->memmap);
    for (int i = 0; i < 4; i++) {
        ui_memedit_draw(&ui->memedit[i]);
        ui_dasm_draw(&ui->dasm[i]);
    }
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif /* CHIPS_IMPL */
