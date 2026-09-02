// kernel.c
// The core 32-bit Protected Mode kernel of AlphaOS.
// Runs in VGA Mode 13h (320x200 256-color graphics) double-buffered.

#include "idt.h"
#include "isr.h"
#include "font.h"

#define VGA_MEM ((volatile unsigned char*)0xA0000)
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

// Double buffer in memory (placed at safe physical address 0x20000)
#define backbuffer ((unsigned char*)0x20000)

// Global Theme and Random Declarations
static unsigned char current_theme = 1; // 0: Classic Blue, 1: Matrix Green, 2: Cyber Purple, 3: Stealth Dark
unsigned int rand();

// Low-Level Port I/O Helpers
unsigned char port_byte_in(unsigned short port) {
    unsigned char result;
    __asm__ volatile("in %%dx, %%al" : "=a" (result) : "d" (port));
    return result;
}

void port_byte_out(unsigned short port, unsigned char data) {
    __asm__ volatile("out %%al, %%dx" : : "a" (data), "d" (port));
}

// Global OS & GUI State
static int mouse_x = 160;
static int mouse_y = 100;
static int start_menu_visible = 0;
static int win_visible = 1;
static int win_x = 65;
static int win_y = 25;
static int win_w = 190;
static int win_h = 130;

// Terminal text buffer inside the window (12 rows, 22 columns)
#define TERM_ROWS 12
#define TERM_COLS 22
static char term_buffer[TERM_ROWS][TERM_COLS];
static int term_cursor_x = 0;
static int term_cursor_y = 0;

static unsigned int timer_ticks = 0;

// Keyboard buffer (Circular queue)
#define KEY_BUFFER_SIZE 256
static unsigned char key_buffer[KEY_BUFFER_SIZE];
static volatile int key_head = 0;
static volatile int key_tail = 0;

// String helpers
int strlen(const char *s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return s1[i] - s2[i];
}

void strcpy(char *dest, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void reverse(char s[]) {
    int i, j;
    char c;
    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

void int_to_ascii(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) str[i++] = '-';
    str[i] = '\0';
    reverse(str);
}

// Configures a VGA Palette color using DAC registers
void set_palette_color(unsigned char index, unsigned char r, unsigned char g, unsigned char b) {
    port_byte_out(0x3C8, index);
    port_byte_out(0x3C9, r); // Red (0-63)
    port_byte_out(0x3C9, g); // Green (0-63)
    port_byte_out(0x3C9, b); // Blue (0-63)
}

// Setup custom colors for Windows XP theme
void setup_xp_palette() {
    // Standard basic colors
    set_palette_color(0,   0,  0,  0);  // 0: Black
    set_palette_color(15,  63, 63, 63); // 15: White
    
    // XP specific colors
    set_palette_color(200, 15, 30, 55); // Sky deep blue
    set_palette_color(201, 28, 45, 63); // Sky light blue
    set_palette_color(202, 0,  45, 0);  // Hill deep green
    set_palette_color(203, 10, 55, 10); // Hill light green
    set_palette_color(204, 5,  18, 48); // Taskbar blue
    set_palette_color(205, 10, 30, 58); // Taskbar light highlight
    set_palette_color(206, 12, 45, 12); // Start button green
    set_palette_color(207, 20, 55, 20); // Start button bright green
    set_palette_color(208, 0,  22, 50); // Window title blue
    set_palette_color(209, 52, 10, 10); // Close button red
    set_palette_color(210, 58, 58, 60); // Start menu left pane (light gray)
    set_palette_color(211, 40, 45, 55); // Start menu right pane (blue-gray)
    set_palette_color(212, 58, 30, 5);  // Shutdown button orange
    set_palette_color(213, 24, 24, 28); // Text viewport shadow/gray border
}

// Basic Graphics Drawing primitives
void put_pixel(int x, int y, unsigned char color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        backbuffer[y * SCREEN_WIDTH + x] = color;
    }
}

void draw_rect(int x, int y, int w, int h, unsigned char color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            put_pixel(x + j, y + i, color);
        }
    }
}

void draw_circle(int xc, int yc, int r, unsigned char color) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        // Draw 8 octant segments
        put_pixel(xc+x, yc+y, color); put_pixel(xc-x, yc+y, color);
        put_pixel(xc+x, yc-y, color); put_pixel(xc-x, yc-y, color);
        put_pixel(xc+y, yc+x, color); put_pixel(xc-y, yc+x, color);
        put_pixel(xc+y, yc-x, color); put_pixel(xc-y, yc-x, color);
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

// Fill circle for clouds
void draw_filled_circle(int xc, int yc, int r, unsigned char color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                put_pixel(xc + x, yc + y, color);
            }
        }
    }
}

// Text Rendering primitives
void draw_char(int x, int y, char c, unsigned char color) {
    if (c < 32 || c > 126) return;
    int font_idx = c - 32;
    for (int row = 0; row < 8; row++) {
        unsigned char row_bits = font8x8[font_idx][row];
        for (int col = 0; col < 8; col++) {
            if (row_bits & (0x80 >> col)) {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_string(int x, int y, const char* str, unsigned char color) {
    int i = 0;
    while (str[i] != '\0') {
        draw_char(x + i * 8, y, str[i], color);
        i++;
    }
}

// Bliss Wallpaper Renderer
void draw_bliss_wallpaper() {
    // 1. Sky Gradient (deep blue to light blue)
    for (int y = 0; y < 180; y++) {
        unsigned char color = 200; // Deep blue
        if (y > 40) color = 201;    // Light blue
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            backbuffer[y * SCREEN_WIDTH + x] = color;
        }
    }
    
    // 2. Draw Clouds
    draw_filled_circle(90,  35, 12, 15);
    draw_filled_circle(110, 30, 16, 15);
    draw_filled_circle(130, 33, 10, 15);
    
    draw_filled_circle(240, 25, 14, 15);
    draw_filled_circle(260, 22, 18, 15);
    draw_filled_circle(280, 26, 12, 15);

    // 3. Draw Green Hills using parabolic equations
    // Main Hill
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        // Main hill equation: peak in middle
        int dx = x - 160;
        int hill_y = 120 + (dx * dx) / 300;
        if (hill_y < 180) {
            for (int y = hill_y; y < 180; y++) {
                put_pixel(x, y, 203); // Light Green
            }
            // Shadow under main hill peak
            put_pixel(x, hill_y, 202);     // Deep Green edge
            put_pixel(x, hill_y + 1, 202);
        }
    }

    // Left Secondary Hill (lower and offset to left)
    for (int x = 0; x < 150; x++) {
        int dx = x - 40;
        int hill_y = 145 + (dx * dx) / 100;
        if (hill_y < 180) {
            for (int y = hill_y; y < 180; y++) {
                put_pixel(x, y, 202); // Deep Green
            }
        }
    }
}

// Desktop Icon Renderer
void draw_desktop_icons() {
    // Icon 1: My Computer
    // Monitor screen
    draw_rect(14, 12, 12, 8, 213); // outer border
    draw_rect(15, 13, 10, 6, 201); // light blue screen
    draw_rect(19, 20, 2, 3, 213);  // stand
    draw_rect(16, 23, 8, 1, 213);  // stand base
    draw_string(4, 27, "Computer", 15);
    
    // Icon 2: Internet Explorer
    // Gold ring (draw yellow ellipse outline)
    draw_circle(20, 56, 6, 14); // Gold ring
    // Blue 'e' symbol
    draw_circle(19, 57, 5, 200); // Blue 'e'
    draw_rect(15, 56, 8, 2, 200);  // crossbar
    draw_string(6, 67, "Internet", 15);

    // Icon 3: Recycle Bin
    draw_rect(15, 95, 10, 9, 213);  // Bucket body
    draw_rect(14, 94, 12, 1, 15);   // Rim
    draw_rect(17, 97, 1, 6, 0);     // vertical stripes
    draw_rect(20, 97, 1, 6, 0);
    draw_rect(22, 97, 1, 6, 0);
    draw_string(9, 107, "Recycle", 15);
}

// Taskbar & System Tray Clock Renderer
void draw_taskbar() {
    // 1. Draw Taskbar base (Row 180 to 199)
    draw_rect(0, 180, SCREEN_WIDTH, 20, 204); // Dark taskbar blue
    draw_rect(0, 180, SCREEN_WIDTH, 1, 205);  // Highlight light blue line
    
    // 2. Draw Start Button (Green)
    draw_rect(0, 181, 46, 19, 206);
    draw_rect(0, 181, 46, 1, 207);  // Top border highlight
    draw_rect(45, 181, 1, 19, 0);   // Right shadow border
    draw_string(5, 187, "start", 15); // "start" in white

    // 3. Draw System Tray (Clock area)
    draw_rect(255, 181, 65, 19, 201); // Light blue-green tray
    draw_rect(255, 181, 1, 19, 200);  // Left shadow line
    
    // Format and display real-time clock
    int total_sec = timer_ticks / 100;
    int sec = total_sec % 60;
    int min = (total_sec / 60) % 60;
    int hour = (total_sec / 3600) % 24;
    
    char time_str[16];
    time_str[0] = '0' + (hour / 10);
    time_str[1] = '0' + (hour % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (min / 10);
    time_str[4] = '0' + (min % 10);
    time_str[5] = ':';
    time_str[6] = '0' + (sec / 10);
    time_str[7] = '0' + (sec % 10);
    time_str[8] = '\0';
    
    draw_string(262, 187, time_str, 15);
}

// Draggable/Floating Terminal Window Renderer
void draw_terminal_window() {
    if (!win_visible) return;
    
    // 1. Draw Window shadow (gray)
    draw_rect(win_x + 2, win_y + 2, win_w, win_h, 0);
    
    // 2. Draw Main Window frame (gray background)
    draw_rect(win_x, win_y, win_w, win_h, 213);
    
    // 3. Draw blue Title Bar
    draw_rect(win_x + 1, win_y + 1, win_w - 2, 14, 208);
    draw_string(win_x + 5, win_y + 4, "AlphaOS Terminal", 15); // white text
    
    // 4. Draw red Close Button
    draw_rect(win_x + win_w - 14, win_y + 2, 11, 11, 209);
    draw_string(win_x + win_w - 11, win_y + 4, "X", 15);
    
    // 5. Draw white terminal text viewport
    draw_rect(win_x + 4, win_y + 18, win_w - 8, win_h - 22, 15); // white text viewport
    draw_rect(win_x + 3, win_y + 17, win_w - 6, 1, 200);        // inner borders
    draw_rect(win_x + 3, win_y + 17, 1, win_h - 20, 200);
    
    // 6. Render terminal text rows
    for (int r = 0; r < TERM_ROWS; r++) {
        int ty = win_y + 20 + r * 8;
        int tx = win_x + 6;
        for (int c = 0; c < TERM_COLS; c++) {
            char ch = term_buffer[r][c];
            if (ch == '\0') break;
            draw_char(tx + c * 8, ty, ch, 0); // Black text on white viewport
        }
    }
    
    // 7. Render active blinking cursor (drawn as a black block)
    if ((timer_ticks / 50) % 2 == 0) {
        int cx = win_x + 6 + term_cursor_x * 8;
        int cy = win_y + 20 + term_cursor_y * 8;
        draw_rect(cx, cy + 6, 7, 2, 0); // small cursor line
    }
}

// Start Menu Panel Renderer
void draw_start_menu() {
    if (!start_menu_visible) return;
    
    // Draw shadow
    draw_rect(2, 62, 120, 120, 0);
    
    // Draw menu panel background
    draw_rect(0, 60, 120, 120, 210); // light gray
    
    // Draw Blue header
    draw_rect(0, 60, 120, 16, 208);
    draw_string(6, 64, "AlphaOS User", 15);
    
    // Draw Program options (Left pane)
    draw_string(4, 84,  "[1] Terminal", 0);
    draw_string(4, 100, "[2] Matrix", 0);
    draw_string(4, 116, "[3] Theme", 0);
    
    // Draw vertical divider
    draw_rect(76, 76, 1, 89, 213);
    
    // Draw Places panel (Right pane - blue-gray)
    draw_rect(77, 76, 43, 89, 211);
    draw_string(80, 84,  "Files", 15);
    draw_string(80, 100, "Run...", 15);
    
    // Draw Orange Shutdown/Reboot bar (Bottom pane)
    draw_rect(0, 165, 120, 15, 212);
    draw_string(6, 169, "[R] Reboot   [H] Halt", 15);
}

// Render Mouse Pointer (Classic white arrow with black border)
void draw_mouse_cursor() {
    int mx = mouse_x;
    int my = mouse_y;
    
    // Tip
    put_pixel(mx, my, 0);
    // Row 1
    put_pixel(mx, my+1, 0); put_pixel(mx+1, my+1, 15); put_pixel(mx+2, my+1, 0);
    // Row 2
    put_pixel(mx, my+2, 0); put_pixel(mx+1, my+2, 15); put_pixel(mx+2, my+2, 15); put_pixel(mx+3, my+2, 0);
    // Row 3
    put_pixel(mx, my+3, 0); put_pixel(mx+1, my+3, 15); put_pixel(mx+2, my+3, 15); put_pixel(mx+3, my+3, 15); put_pixel(mx+4, my+3, 0);
    // Row 4
    put_pixel(mx, my+4, 0); put_pixel(mx+1, my+4, 15); put_pixel(mx+2, my+4, 15); put_pixel(mx+3, my+4, 0);
    // Row 5
    put_pixel(mx, my+5, 0); put_pixel(mx+1, my+5, 0); put_pixel(mx+3, my+5, 15); put_pixel(mx+4, my+5, 0);
    // Row 6
    put_pixel(mx, my+6, 0); put_pixel(mx+4, my+6, 0);
}

// Swap Double Buffer to Screen
void refresh_screen() {
    // Copy the entire backbuffer to the physical VGA graphics memory
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        VGA_MEM[i] = backbuffer[i];
    }
}

// Complete Desktop Redraw Sequence
void redraw_desktop() {
    draw_bliss_wallpaper();
    draw_desktop_icons();
    draw_terminal_window();
    draw_start_menu();
    draw_taskbar();
    draw_mouse_cursor();
    refresh_screen();
}

// Keyboard circular buffer accessors
int key_available() {
    return key_head != key_tail;
}

unsigned char read_key_buffer() {
    if (!key_available()) return 0;
    unsigned char scancode = key_buffer[key_tail];
    key_tail = (key_tail + 1) % KEY_BUFFER_SIZE;
    return scancode;
}

// Prints a character inside the graphical terminal window
void term_print_char(char c) {
    if (c == '\n') {
        term_cursor_x = 0;
        term_cursor_y++;
        if (term_cursor_y >= TERM_ROWS) {
            // Scroll rows up
            for (int r = 0; r < TERM_ROWS - 1; r++) {
                for (int col = 0; col < TERM_COLS; col++) {
                    term_buffer[r][col] = term_buffer[r+1][col];
                }
            }
            // Clear last row
            for (int col = 0; col < TERM_COLS; col++) {
                term_buffer[TERM_ROWS - 1][col] = '\0';
            }
            term_cursor_y = TERM_ROWS - 1;
        }
        return;
    }
    
    if (c == '\b') {
        if (term_cursor_x > 0) {
            term_cursor_x--;
            term_buffer[term_cursor_y][term_cursor_x] = '\0';
        }
        return;
    }
    
    if (term_cursor_x >= TERM_COLS - 1) {
        term_cursor_x = 0;
        term_cursor_y++;
        if (term_cursor_y >= TERM_ROWS) {
            // Scroll
            for (int r = 0; r < TERM_ROWS - 1; r++) {
                for (int col = 0; col < TERM_COLS; col++) {
                    term_buffer[r][col] = term_buffer[r+1][col];
                }
            }
            for (int col = 0; col < TERM_COLS; col++) {
                term_buffer[TERM_ROWS - 1][col] = '\0';
            }
            term_cursor_y = TERM_ROWS - 1;
        }
    }
    
    term_buffer[term_cursor_y][term_cursor_x] = c;
    term_cursor_x++;
    term_buffer[term_cursor_y][term_cursor_x] = '\0';
}

// Prints string to terminal window
void term_print(const char* str) {
    int i = 0;
    while (str[i] != '\0') {
        term_print_char(str[i]);
        i++;
    }
}

// Clears the terminal window viewport
void term_clear() {
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            term_buffer[r][c] = '\0';
        }
    }
    term_cursor_x = 0;
    term_cursor_y = 0;
}

// Redirect stubs for ISR panics
void clear_log_area() {
    term_clear();
}

void print_log(const char* str) {
    term_print(str);
}

// Animated Matrix Falling Code Screensaver (Fills full screen in Graphics Mode)
void matrix_screensaver() {
    // Clear screen for Matrix
    draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    refresh_screen();
    
    int matrix_y[40]; // 40 columns (8 pixels wide = 320 pixels)
    int cols = 40;
    
    for (int x = 0; x < cols; x++) {
        matrix_y[x] = -(rand() % 30);
    }
    
    // Drain keyboard buffer
    while (key_available()) read_key_buffer();
    
    while (1) {
        // Exit screensaver on any keypress
        if (key_available()) {
            unsigned char code = read_key_buffer();
            if (!(code & 0x80)) {
                break;
            }
        }
        
        // Draw falling drops
        for (int x = 0; x < cols; x++) {
            matrix_y[x] += 1;
            
            // Draw head
            if (matrix_y[x] >= 0 && matrix_y[x] < 25) {
                char ch = (rand() % 93) + 33;
                draw_char(x * 8, matrix_y[x] * 8, ch, 15); // White head
            }
            
            // Draw trail
            for (int t = 1; t < 12; t++) {
                int py = matrix_y[x] - t;
                if (py >= 0 && py < 25) {
                    char ch = (rand() % 93) + 33;
                    draw_char(x * 8, py * 8, ch, (t == 1) ? 10 : 2); // green/light green
                }
            }
            
            // Clear tail
            int ty = matrix_y[x] - 12;
            if (ty >= 0 && ty < 25) {
                draw_rect(x * 8, ty * 8, 8, 8, 0); // black square
            }
            
            // Reset column
            if (matrix_y[x] >= 25) {
                matrix_y[x] = -(rand() % 15);
            }
        }
        
        refresh_screen();
        
        // Delay loop
        for (volatile int d = 0; d < 600000; d++);
    }
    
    // Drain keyboard buffer again before returning
    while (key_available()) read_key_buffer();
}

// Shell Command Interpreter for Terminal Window
void execute_command(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        term_print("Commands:\n");
        term_print(" help   - Show list\n");
        term_print(" about  - OS info\n");
        term_print(" clear  - Clear screen\n");
        term_print(" matrix - Screensaver\n");
        term_print(" ticks  - Timer ticks\n");
        term_print(" divide - Trigger panic\n");
        term_print(" reboot - Reset system\n");
        term_print(" halt   - Turn off CPU\n");
    } else if (strcmp(cmd, "about") == 0) {
        term_print("AlphaOS v1.2 XP GUI\n");
        term_print("====================\n");
        term_print("Running in VGA 320x200\n");
        term_print("256-color graphics.\n");
        term_print("Arrow keys = Move mouse\n");
        term_print("Spacebar   = Mouse click\n");
    } else if (strcmp(cmd, "clear") == 0) {
        term_clear();
    } else if (strcmp(cmd, "ticks") == 0) {
        char ticks_str[32];
        int_to_ascii(timer_ticks, ticks_str);
        term_print("Timer Ticks: ");
        term_print(ticks_str);
        term_print("\n");
    } else if (strcmp(cmd, "divide") == 0) {
        term_print("Crashing...\n");
        for (volatile int d = 0; d < 10000000; d++);
        volatile int divisor = 0;
        volatile int dividend = 42;
        volatile int quotient = dividend / divisor;
        (void)quotient;
    } else if (strcmp(cmd, "reboot") == 0) {
        term_print("Rebooting...\n");
        for (volatile int d = 0; d < 5000000; d++);
        port_byte_out(0x64, 0xFE);
        while(1) {
            __asm__ volatile("cli");
            __asm__ volatile("lidt (%0)" : : "r"(0));
            __asm__ volatile("int $3");
        }
    } else if (strcmp(cmd, "halt") == 0) {
        term_print("Halted safely.\n");
        redraw_desktop();
        __asm__ volatile("cli");
        while (1) {
            __asm__ volatile("hlt");
        }
    } else if (strlen(cmd) > 0) {
        term_print("Unknown: ");
        term_print(cmd);
        term_print("\nType 'help'\n");
    }
}

// Mouse Click Collision Handler
void handle_mouse_click() {
    int mx = mouse_x;
    int my = mouse_y;
    
    // 1. Check if clicked Start Button (x: 0-45, y: 180-200)
    if (mx >= 0 && mx <= 45 && my >= 180 && my <= 200) {
        start_menu_visible = !start_menu_visible;
        return;
    }
    
    // 2. Check if clicked inside Start Menu
    if (start_menu_visible) {
        // Option 1: Terminal (x: 0-75, y: 80-95)
        if (mx >= 0 && mx <= 75 && my >= 80 && my <= 95) {
            win_visible = 1;
            start_menu_visible = 0;
            return;
        }
        // Option 2: Matrix (x: 0-75, y: 96-111)
        if (mx >= 0 && mx <= 75 && my >= 96 && my <= 111) {
            start_menu_visible = 0;
            matrix_screensaver();
            return;
        }
        // Option 3: Theme (x: 0-75, y: 112-127)
        if (mx >= 0 && mx <= 75 && my >= 112 && my <= 127) {
            // Change colors
            current_theme = (current_theme + 1) % 4;
            if (current_theme == 0) {
                set_palette_color(204, 5, 10, 40); // Darker taskbar
            } else if (current_theme == 1) {
                set_palette_color(204, 5, 18, 48); // Classic Blue
            } else if (current_theme == 2) {
                set_palette_color(204, 25, 5, 30); // Cyber Purple taskbar
            } else {
                set_palette_color(204, 15, 15, 18); // Dark theme taskbar
            }
            start_menu_visible = 0;
            return;
        }
        // Option Reboot: (x: 0-60, y: 165-180)
        if (mx >= 0 && mx <= 60 && my >= 165 && my <= 180) {
            start_menu_visible = 0;
            execute_command("reboot");
            return;
        }
        // Option Halt: (x: 61-120, y: 165-180)
        if (mx >= 61 && mx <= 120 && my >= 165 && my <= 180) {
            start_menu_visible = 0;
            execute_command("halt");
            return;
        }
        
        // Clicked outside menu options
        if (mx > 120 || my < 60 || my > 180) {
            start_menu_visible = 0;
        }
        return;
    }
    
    // 3. Check if clicked My Computer Icon (x: 5-45, y: 12-40)
    if (mx >= 5 && mx <= 45 && my >= 12 && my <= 40) {
        win_visible = 1;
        return;
    }
    
    // 4. Check if clicked IE Icon (x: 5-45, y: 55-80)
    if (mx >= 5 && mx <= 45 && my >= 55 && my <= 80) {
        win_visible = 1;
        term_clear();
        term_print("Connecting to standard\nweb... error 404.\nTry 'help'.\n");
        return;
    }

    // 5. Check if clicked Recycle Bin Icon (x: 5-45, y: 95-120)
    if (mx >= 5 && mx <= 45 && my >= 95 && my <= 120) {
        win_visible = 1;
        term_clear();
        term_print("Recycle Bin is empty.\n");
        return;
    }
    
    // 6. Check if clicked Terminal window close button
    if (win_visible) {
        // Red close button (x: win_x + win_w - 14 to win_x + win_w - 3, y: win_y + 2 to win_y + 13)
        if (mx >= win_x + win_w - 14 && mx <= win_x + win_w - 3 &&
            my >= win_y + 2 && my <= win_y + 13) {
            win_visible = 0;
            return;
        }
        
        // Clicked window title bar to trigger small drag or focus (optional)
        if (mx >= win_x && mx <= win_x + win_w && my >= win_y && my <= win_y + 15) {
            // Drag window to mouse position
            win_x = mx - win_w / 2;
            win_y = my - 7;
            if (win_x < 0) win_x = 0;
            if (win_y < 0) win_y = 0;
            if (win_x + win_w > SCREEN_WIDTH) win_x = SCREEN_WIDTH - win_w;
            if (win_y + win_h > 180) win_y = 180 - win_h;
            return;
        }
    }
}

// Timer Callback Handler (IRQ 0)
void timer_callback(struct registers* r) {
    timer_ticks++;
    // Redraw the desktop screen 30 times a second to keep graphics fluid
    if (timer_ticks % 3 == 0) {
        redraw_desktop();
    }
}

// Keyboard Callback Handler (IRQ 1)
void keyboard_callback(struct registers* r) {
    unsigned char scancode = port_byte_in(0x60);
    int next = (key_head + 1) % KEY_BUFFER_SIZE;
    if (next != key_tail) {
        key_buffer[key_head] = scancode;
        key_head = next;
    }
}

// Keyboard scan code to ASCII table for shift key tracking
static const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,   '*',   0,
  ' '
};

static const char scancode_to_ascii_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,   '*',   0,
  ' '
};

// Simple pseudo-random number generator seed
static unsigned int rand_seed = 12345;
unsigned int rand() {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed / 65536) % 32768;
}

// Kernel Main Entry Point
void kernel_main() {
    // 1. Initialize custom Windows XP VGA colors
    setup_xp_palette();
    
    // 2. Clear terminal buffer text
    term_clear();
    term_print("Welcome to AlphaOS!\n");
    term_print("Windows XP Clone UI\n");
    term_print("Type 'help' in shell.\n\n");
    
    // 3. Install CPU Interrupt Descriptor Table
    idt_install();
    
    // 4. Remap 8259 PIC vectors
    pic_remap();
    
    // 5. Register timer and keyboard interrupt callbacks
    register_interrupt_handler(32, timer_callback);
    register_interrupt_handler(33, keyboard_callback);
    
    // 6. Configure PIT clock to 100 Hz
    pit_init(100);
    
    // 7. Enable system hardware interrupts
    __asm__ volatile("sti");
    
    char command_buffer[64];
    int cmd_len = 0;
    command_buffer[0] = '\0';
    
    term_print("AlphaOS> ");
    
    int shift_pressed = 0;
    
    // Draw initial screen
    redraw_desktop();
    
    // Main Operating System Event Processing loop
    while (1) {
        int needs_redraw = 0;
        
        if (key_available()) {
            unsigned char scancode = read_key_buffer();
            
            // Check for Shift Key Press
            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = 1;
            }
            // Check for Shift Key Release
            else if (scancode == 0xAA || scancode == 0xB6) {
                shift_pressed = 0;
            }
            // Check for Arrow keys to move the mouse cursor
            else if (scancode == 0x48) { // Up
                mouse_y -= 6;
                if (mouse_y < 0) mouse_y = 0;
                needs_redraw = 1;
            }
            else if (scancode == 0x50) { // Down
                mouse_y += 6;
                if (mouse_y > SCREEN_HEIGHT - 4) mouse_y = SCREEN_HEIGHT - 4;
                needs_redraw = 1;
            }
            else if (scancode == 0x4B) { // Left
                mouse_x -= 6;
                if (mouse_x < 0) mouse_x = 0;
                needs_redraw = 1;
            }
            else if (scancode == 0x4D) { // Right
                mouse_x += 6;
                if (mouse_x > SCREEN_WIDTH - 4) mouse_x = SCREEN_WIDTH - 4;
                needs_redraw = 1;
            }
            // Spacebar acts as Mouse Left-Click
            else if (scancode == 0x39) {
                handle_mouse_click();
                needs_redraw = 1;
            }
            // Ignore other key releases (highest bit set)
            else if (scancode & 0x80) {
                // Ignore release
            }
            // Direct regular keyboard input to the active terminal window
            else if (win_visible) {
                char ascii = 0;
                if (scancode < 58) {
                    ascii = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
                }
                
                if (ascii != 0) {
                    if (ascii == '\n') {
                        // Submit command
                        term_print("\n");
                        
                        // Execute command
                        execute_command(command_buffer);
                        
                        // Reset command buffer
                        cmd_len = 0;
                        command_buffer[0] = '\0';
                        
                        term_print("AlphaOS> ");
                    } else if (ascii == '\b') {
                        if (cmd_len > 0) {
                            cmd_len--;
                            command_buffer[cmd_len] = '\0';
                            term_print_char('\b');
                        }
                    } else {
                        if (cmd_len < 12) { // limit command size to fit terminal viewport width
                            command_buffer[cmd_len] = ascii;
                            cmd_len++;
                            command_buffer[cmd_len] = '\0';
                            term_print_char(ascii);
                        }
                    }
                    needs_redraw = 1;
                }
            }
        }
        
        if (needs_redraw) {
            redraw_desktop();
        }
        
        // Put CPU to sleep until next interrupt
        __asm__ volatile("hlt");
    }
}
