#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL.h>

#define MEMORY_SIZE     4096
#define REGISTER_COUNT  16
#define STACK_SIZE      16
#define NORMAL_WIDTH    64
#define NORMAL_HEIGHT   32
#define EXT_WIDTH       128
#define EXT_HEIGHT      64
#define MAX_WIDTH       EXT_WIDTH   // Maximum allocated display width.
#define MAX_HEIGHT      EXT_HEIGHT  // Maximum allocated display height.
#define WINDOW_SCALE    10
#define START_ADDRESS   0x200

#define AUDIO_FREQUENCY 44100
#define TONE_FREQUENCY  440

// Global display dimensions (changes with mode).
int screen_width = NORMAL_WIDTH;
int screen_height = NORMAL_HEIGHT;
int extended_mode = 0; // 0 = normal, 1 = extended (SCHIP)

// Global color palette.
// Normal mode: white fg on black bg.
// Extended mode: bright cyan fg on dark blue bg.
Uint8 fg_r = 255, fg_g = 255, fg_b = 255;
Uint8 bg_r = 0,   bg_g = 0,   bg_b = 0;

// Global SDL_Window pointer for dynamic resizing.
SDL_Window *g_window = NULL;

// The Chip-8 state structure.
typedef struct {
    uint8_t memory[MEMORY_SIZE];
    uint8_t V[REGISTER_COUNT];
    uint16_t I;
    uint16_t pc;
    uint16_t stack[STACK_SIZE];
    uint8_t sp;
    uint8_t delay_timer;
    uint8_t sound_timer;
    // Allocate maximum size; when in normal mode, only use a subset.
    uint8_t display[MAX_WIDTH * MAX_HEIGHT];
    uint8_t keys[16];
} Chip8;

Chip8 chip8;

// Global variables for audio synthesis.
static double audio_phase = 0.0;
static double audio_phase_inc = (2.0 * M_PI * TONE_FREQUENCY) / AUDIO_FREQUENCY;

// Standard Chip-8 fontset (each character is 5 bytes).
uint8_t chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// Audio callback: generates a sine-wave tone if sound_timer > 0.
void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata; // Unused.
    int sample_count = len / sizeof(Sint16);
    Sint16 *buffer = (Sint16 *)stream;
    if (chip8.sound_timer > 0) {
        for (int i = 0; i < sample_count; i++) {
            buffer[i] = (Sint16)(32767 * sin(audio_phase));
            audio_phase += audio_phase_inc;
            if (audio_phase > 2.0 * M_PI)
                audio_phase -= 2.0 * M_PI;
        }
    } else {
        memset(stream, 0, len);
    }
}

// Initialize the Chip-8 system.
void initializeChip8(Chip8 *chip8) {
    chip8->pc = START_ADDRESS;
    chip8->I = 0;
    chip8->sp = 0;
    memset(chip8->memory, 0, sizeof(chip8->memory));
    memset(chip8->V, 0, sizeof(chip8->V));
    memset(chip8->stack, 0, sizeof(chip8->stack));
    memset(chip8->display, 0, sizeof(chip8->display));
    memset(chip8->keys, 0, sizeof(chip8->keys));
    chip8->delay_timer = 0;
    chip8->sound_timer = 0;
    memcpy(chip8->memory + 0x50, chip8_fontset, sizeof(chip8_fontset));
}

// Load the Chip-8 ROM into memory.
int loadROM(Chip8 *chip8, const char *filename) {
    FILE *rom = fopen(filename, "rb");
    if (!rom) {
        perror("Failed to open ROM");
        return 0;
    }
    fseek(rom, 0, SEEK_END);
    long rom_size = ftell(rom);
    rewind(rom);
    if (rom_size > (MEMORY_SIZE - START_ADDRESS)) {
        fprintf(stderr, "ROM too large for memory\n");
        fclose(rom);
        return 0;
    }
    fread(chip8->memory + START_ADDRESS, sizeof(uint8_t), rom_size, rom);
    fclose(rom);
    return 1;
}

// Render the current display using SDL2 with color support.
void drawGraphics(SDL_Renderer *renderer, Chip8 *chip8) {
    SDL_SetRenderDrawColor(renderer, bg_r, bg_g, bg_b, 255);
    SDL_RenderClear(renderer);
    SDL_Rect pixel;
    pixel.w = WINDOW_SCALE;
    pixel.h = WINDOW_SCALE;
    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            // The display is stored in a MAX_WIDTH-wide array.
            if (chip8->display[y * MAX_WIDTH + x]) {
                pixel.x = x * WINDOW_SCALE;
                pixel.y = y * WINDOW_SCALE;
                SDL_SetRenderDrawColor(renderer, fg_r, fg_g, fg_b, 255);
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

// Fetch the next opcode (16 bits) from memory.
uint16_t fetchOpcode(Chip8 *chip8) {
    return chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc + 1];
}

// Helper: Scroll display horizontally.
void scroll_horizontal(Chip8 *chip8, int direction) {
    int shift = 4;
    if (direction > 0) {
        for (int y = 0; y < screen_height; y++) {
            for (int x = screen_width - 1; x >= shift; x--) {
                chip8->display[y * MAX_WIDTH + x] = chip8->display[y * MAX_WIDTH + (x - shift)];
            }
            for (int x = 0; x < shift; x++) {
                chip8->display[y * MAX_WIDTH + x] = 0;
            }
        }
    } else {
        for (int y = 0; y < screen_height; y++) {
            for (int x = 0; x < screen_width - shift; x++) {
                chip8->display[y * MAX_WIDTH + x] = chip8->display[y * MAX_WIDTH + (x + shift)];
            }
            for (int x = screen_width - shift; x < screen_width; x++) {
                chip8->display[y * MAX_WIDTH + x] = 0;
            }
        }
    }
}

// Helper: Scroll display down by n rows.
void scroll_down(Chip8 *chip8, int n) {
    for (int y = screen_height - 1; y >= n; y--) {
        for (int x = 0; x < screen_width; x++) {
            chip8->display[y * MAX_WIDTH + x] = chip8->display[(y - n) * MAX_WIDTH + x];
        }
    }
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < screen_width; x++) {
            chip8->display[y * MAX_WIDTH + x] = 0;
        }
    }
}

// Emulate one cycle (fetch, decode, execute).
void emulateCycle(Chip8 *chip8) {
    uint16_t opcode = fetchOpcode(chip8);
    chip8->pc += 2;

    uint8_t x   = (opcode & 0x0F00) >> 8;
    uint8_t y   = (opcode & 0x00F0) >> 4;
    uint8_t kk  = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;
    uint8_t n   = opcode & 0x000F;

    // SCHIP extended opcodes.
    if ((opcode & 0xF0FF) == 0x00FB) {       // 00FB: Scroll right 4 pixels.
        scroll_horizontal(chip8, +1);
        return;
    } else if ((opcode & 0xF0FF) == 0x00FC) { // 00FC: Scroll left 4 pixels.
        scroll_horizontal(chip8, -1);
        return;
    } else if ((opcode & 0xF0FF) == 0x00FD) { // 00FD: Exit interpreter.
        exit(0);
    } else if ((opcode & 0xF0FF) == 0x00FE) { // 00FE: Disable extended mode.
        extended_mode = 0;
        screen_width = NORMAL_WIDTH;
        screen_height = NORMAL_HEIGHT;
        // Set colors for normal mode.
        fg_r = 255; fg_g = 255; fg_b = 255;
        bg_r = 0;   bg_g = 0;   bg_b = 0;
        memset(chip8->display, 0, sizeof(chip8->display));
        SDL_SetWindowSize(g_window, screen_width * WINDOW_SCALE, screen_height * WINDOW_SCALE);
        return;
    } else if ((opcode & 0xF0FF) == 0x00FF) { // 00FF: Enable extended mode.
        extended_mode = 1;
        screen_width = EXT_WIDTH;
        screen_height = EXT_HEIGHT;
        // Set colors for extended mode.
        fg_r = 0;   fg_g = 255; fg_b = 255;
        bg_r = 0;   bg_g = 0;   bg_b = 128;
        memset(chip8->display, 0, sizeof(chip8->display));
        SDL_SetWindowSize(g_window, screen_width * WINDOW_SCALE, screen_height * WINDOW_SCALE);
        return;
    } else if ((opcode & 0xF000) == 0x0000 && (opcode & 0x00F0) == 0x00C0) {
        int n_rows = opcode & 0x000F;
        scroll_down(chip8, n_rows);
        return;
    }

    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode) {
                case 0x00E0: // Clear display.
                    memset(chip8->display, 0, sizeof(chip8->display));
                    break;
                case 0x00EE: // Return from subroutine.
                    chip8->sp--;
                    chip8->pc = chip8->stack[chip8->sp];
                    break;
                default:
                    break;
            }
            break;
        case 0x1000:
            chip8->pc = nnn;
            break;
        case 0x2000:
            chip8->stack[chip8->sp] = chip8->pc;
            chip8->sp++;
            chip8->pc = nnn;
            break;
        case 0x3000:
            if (chip8->V[x] == kk)
                chip8->pc += 2;
            break;
        case 0x4000:
            if (chip8->V[x] != kk)
                chip8->pc += 2;
            break;
        case 0x5000:
            if (chip8->V[x] == chip8->V[y])
                chip8->pc += 2;
            break;
        case 0x6000:
            chip8->V[x] = kk;
            break;
        case 0x7000:
            chip8->V[x] += kk;
            break;
        case 0x8000:
            switch (opcode & 0x000F) {
                case 0x0:
                    chip8->V[x] = chip8->V[y];
                    break;
                case 0x1:
                    chip8->V[x] |= chip8->V[y];
                    break;
                case 0x2:
                    chip8->V[x] &= chip8->V[y];
                    break;
                case 0x3:
                    chip8->V[x] ^= chip8->V[y];
                    break;
                case 0x4: {
                    uint16_t sum = chip8->V[x] + chip8->V[y];
                    chip8->V[0xF] = (sum > 255) ? 1 : 0;
                    chip8->V[x] = sum & 0xFF;
                    break;
                }
                case 0x5:
                    chip8->V[0xF] = (chip8->V[x] > chip8->V[y]) ? 1 : 0;
                    chip8->V[x] -= chip8->V[y];
                    break;
                case 0x6:
                    chip8->V[0xF] = chip8->V[x] & 0x1;
                    chip8->V[x] >>= 1;
                    break;
                case 0x7:
                    chip8->V[0xF] = (chip8->V[y] > chip8->V[x]) ? 1 : 0;
                    chip8->V[x] = chip8->V[y] - chip8->V[x];
                    break;
                case 0xE:
                    chip8->V[0xF] = (chip8->V[x] & 0x80) >> 7;
                    chip8->V[x] <<= 1;
                    break;
                default:
                    break;
            }
            break;
        case 0x9000:
            if (chip8->V[x] != chip8->V[y])
                chip8->pc += 2;
            break;
        case 0xA000:
            chip8->I = nnn;
            break;
        case 0xB000:
            chip8->pc = nnn + chip8->V[0];
            break;
        case 0xC000:
            chip8->V[x] = (rand() % 256) & kk;
            break;
        case 0xD000: {
            int spriteWidth, spriteHeight;
            // If in extended mode and n==0, draw a 16x16 sprite.
            if (extended_mode && n == 0) {
                spriteWidth = 16;
                spriteHeight = 16;
            } else {
                spriteWidth = 8;
                spriteHeight = n;
            }
            chip8->V[0xF] = 0;
            if (spriteWidth == 16) {
                // SCHIP 16x16 sprite: assume 32 bytes, 2 bytes per row.
                for (int row = 0; row < 16; row++) {
                    uint8_t byte1 = chip8->memory[chip8->I + row * 2];
                    uint8_t byte2 = chip8->memory[chip8->I + row * 2 + 1];
                    for (int col = 0; col < 16; col++) {
                        int pixelBit = (col < 8)
                            ? ((byte1 & (0x80 >> col)) != 0)
                            : ((byte2 & (0x80 >> (col - 8))) != 0);
                        if (pixelBit) {
                            int posX = (chip8->V[x] + col) % screen_width;
                            int posY = (chip8->V[y] + row) % screen_height;
                            int idx = posY * MAX_WIDTH + posX;
                            if (chip8->display[idx] == 1)
                                chip8->V[0xF] = 1;
                            chip8->display[idx] ^= 1;
                        }
                    }
                }
            } else {
                // Standard 8xN sprite.
                for (int row = 0; row < spriteHeight; row++) {
                    uint8_t spriteByte = chip8->memory[chip8->I + row];
                    for (int col = 0; col < spriteWidth; col++) {
                        int pixelBit = (spriteByte & (0x80 >> col)) != 0;
                        if (pixelBit) {
                            int posX = (chip8->V[x] + col) % screen_width;
                            int posY = (chip8->V[y] + row) % screen_height;
                            int idx = posY * MAX_WIDTH + posX;
                            if (chip8->display[idx] == 1)
                                chip8->V[0xF] = 1;
                            chip8->display[idx] ^= 1;
                        }
                    }
                }
            }
            break;
        }
        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x9E:
                    if (chip8->keys[chip8->V[x]])
                        chip8->pc += 2;
                    break;
                case 0xA1:
                    if (!chip8->keys[chip8->V[x]])
                        chip8->pc += 2;
                    break;
                default:
                    break;
            }
            break;
        case 0xF000:
            switch (opcode & 0x00FF) {
                case 0x07:
                    chip8->V[x] = chip8->delay_timer;
                    break;
                case 0x0A: {
                    int key_pressed = 0;
                    SDL_Event event;
                    while (!key_pressed) {
                        while (SDL_PollEvent(&event)) {
                            if (event.type == SDL_KEYDOWN) {
                                switch (event.key.keysym.sym) {
                                    case SDLK_1: chip8->V[x] = 0x1; key_pressed = 1; break;
                                    case SDLK_2: chip8->V[x] = 0x2; key_pressed = 1; break;
                                    case SDLK_3: chip8->V[x] = 0x3; key_pressed = 1; break;
                                    case SDLK_4: chip8->V[x] = 0xC; key_pressed = 1; break;
                                    case SDLK_q: chip8->V[x] = 0x4; key_pressed = 1; break;
                                    case SDLK_w: chip8->V[x] = 0x5; key_pressed = 1; break;
                                    case SDLK_e: chip8->V[x] = 0x6; key_pressed = 1; break;
                                    case SDLK_r: chip8->V[x] = 0xD; key_pressed = 1; break;
                                    case SDLK_a: chip8->V[x] = 0x7; key_pressed = 1; break;
                                    case SDLK_s: chip8->V[x] = 0x8; key_pressed = 1; break;
                                    case SDLK_d: chip8->V[x] = 0x9; key_pressed = 1; break;
                                    case SDLK_f: chip8->V[x] = 0xE; key_pressed = 1; break;
                                    case SDLK_z: chip8->V[x] = 0xA; key_pressed = 1; break;
                                    case SDLK_x: chip8->V[x] = 0x0; key_pressed = 1; break;
                                    case SDLK_c: chip8->V[x] = 0xB; key_pressed = 1; break;
                                    case SDLK_v: chip8->V[x] = 0xF; key_pressed = 1; break;
                                    default: break;
                                }
                            } else if (event.type == SDL_QUIT) {
                                exit(0);
                            }
                        }
                    }
                    break;
                }
                case 0x15:
                    chip8->delay_timer = chip8->V[x];
                    break;
                case 0x18:
                    chip8->sound_timer = chip8->V[x];
                    break;
                case 0x1E:
                    chip8->I += chip8->V[x];
                    break;
                case 0x29:
                    chip8->I = 0x50 + (chip8->V[x] * 5);
                    break;
                case 0x33: {
                    uint8_t value = chip8->V[x];
                    chip8->memory[chip8->I]     = value / 100;
                    chip8->memory[chip8->I + 1] = (value / 10) % 10;
                    chip8->memory[chip8->I + 2] = value % 10;
                    break;
                }
                case 0x55:
                    for (int i = 0; i <= x; i++) {
                        chip8->memory[chip8->I + i] = chip8->V[i];
                    }
                    break;
                case 0x65:
                    for (int i = 0; i <= x; i++) {
                        chip8->V[i] = chip8->memory[chip8->I + i];
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (chip8->delay_timer > 0)
        chip8->delay_timer--;
    if (chip8->sound_timer > 0)
        chip8->sound_timer--;
}

int mapKey(SDL_Keycode key) {
    switch (key) {
        case SDLK_1: return 0x1;
        case SDLK_2: return 0x2;
        case SDLK_3: return 0x3;
        case SDLK_4: return 0xC;
        case SDLK_q: return 0x4;
        case SDLK_w: return 0x5;
        case SDLK_e: return 0x6;
        case SDLK_r: return 0xD;
        case SDLK_a: return 0x7;
        case SDLK_s: return 0x8;
        case SDLK_d: return 0x9;
        case SDLK_f: return 0xE;
        case SDLK_z: return 0xA;
        case SDLK_x: return 0x0;
        case SDLK_c: return 0xB;
        case SDLK_v: return 0xF;
        default: return -1;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ROM file>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    initializeChip8(&chip8);
    if (!loadROM(&chip8, argv[1]))
        return 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL could not initialize: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec desired, obtained;
    desired.freq = AUDIO_FREQUENCY;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 2048;
    desired.callback = audio_callback;
    desired.userdata = NULL;
    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (audio_dev == 0) {
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(audio_dev, 0);
    }

    SDL_Window *window = SDL_CreateWindow("cupid-8 Chip8 Emulator",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          screen_width * WINDOW_SCALE,
                                          screen_height * WINDOW_SCALE,
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window could not be created: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    g_window = window; // Set the global window pointer.
    
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int running = 1;
    SDL_Event event;
    const int cycleDelay = 2;
    uint32_t timer_last = SDL_GetTicks();
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
            if (event.type == SDL_KEYDOWN) {
                int keyIndex = mapKey(event.key.keysym.sym);
                if (keyIndex != -1)
                    chip8.keys[keyIndex] = 1;
            }
            if (event.type == SDL_KEYUP) {
                int keyIndex = mapKey(event.key.keysym.sym);
                if (keyIndex != -1)
                    chip8.keys[keyIndex] = 0;
            }
        }

        emulateCycle(&chip8);
        drawGraphics(renderer, &chip8);
        SDL_Delay(cycleDelay);
        if (SDL_GetTicks() - timer_last >= 16) {
            if (chip8.delay_timer > 0)
                chip8.delay_timer--;
            if (chip8.sound_timer > 0)
                chip8.sound_timer--;
            timer_last = SDL_GetTicks();
        }
    }

    SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
