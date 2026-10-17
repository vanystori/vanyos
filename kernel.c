// kernel.c

struct multiboot_tag {
    unsigned int type;
    unsigned int size;
};

struct multiboot_tag_framebuffer {
    unsigned int type;
    unsigned int size;
    unsigned long long framebuffer_addr;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;
};

// Global variables for screen metrics
unsigned int *physical_fb = 0;
unsigned int fb_width = 0;
unsigned int fb_height = 0;
unsigned int fb_pitch = 0;

// Allocate a 16MB backbuffer pool in BSS memory to support resolutions up to 4K
#define BACKBUFFER_SIZE (4096 * 2160)
unsigned int backbuffer[BACKBUFFER_SIZE];

// Simple 8x8 bitmap for a classic arrow mouse cursor (1 = white, 0 = transparent)
unsigned char mouse_cursor_bitmap[8] = {
    0b10000000,
    0b11000000,
    0b11100000,
    0b11110000,
    0b11111000,
    0b11111100,
    0b11001100,
    0b00000000
};

// Mouse track coordinates
int mouse_x = 100;
int mouse_y = 100;

// High-speed memory copy function (replaces standard library memcpy)
void kernel_memcpy(void *dest, const void *src, unsigned int n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (n--) {
        *d++ = *s++;
    }
}

// Low-level hardware assembly I/O communication functions
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Draw a single pixel safely into the backbuffer
void draw_pixel_backbuffer(int x, int y, unsigned int color) {
    if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height) {
        // Pitch / 4 converts byte-pitch into a 32-bit pixel array boundary offset
        backbuffer[y * (fb_pitch / 4) + x] = color;
    }
}

// Render a simple rectangle
void draw_rect(int start_x, int start_y, int width, int height, unsigned int color) {
    for (int y = start_y; y < start_y + height; y++) {
        for (int x = start_x; x < start_x + width; x++) {
            draw_pixel_backbuffer(x, y, color);
        }
    }
}

// Blit the cursor bitmap onto the backbuffer coordinates
void draw_mouse() {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if ((mouse_cursor_bitmap[y] >> (7 - x)) & 1) {
                draw_pixel_backbuffer(mouse_x + x, mouse_y + y, 0xFFFFFFFF); // White pointer
            }
        }
    }
}

// Push the finished frame instantly from RAM to the physical display monitor
void swap_buffers() {
    unsigned int screen_size_bytes = fb_height * fb_pitch;
    kernel_memcpy(physical_fb, backbuffer, screen_size_bytes);
}

// Basic PS/2 Mouse Initialization sequence
void init_mouse() {
    unsigned char status;

    // Enable the auxiliary mouse device
    outb(0x64, 0xA8);
    
    // Tell controller we want to send a command to the mouse
    outb(0x64, 0xD4);
    outb(0x60, 0xF4); // Enable packet streaming command
    
    // Read acknowledgment from keyboard controller
    status = inb(0x60);
    (void)status; // Mute warning
}

// Simple polling fallback tracker to handle mouse data packets smoothly
void poll_mouse() {
    // Check if data is ready in the PS/2 output buffer (Bit 0 must be 1, Bit 5 must be 1 for mouse)
    if ((inb(0x64) & 0x21) == 0x21) {
        unsigned char flags = inb(0x60);
        char d_x = (char)inb(0x60);
        char d_y = (char)inb(0x60);

        // Verify the packet is valid alignment
        if (!(flags & 0x40) && !(flags & 0x80)) { 
            mouse_x += d_x;
            mouse_y -= d_y; // Invert axis because screen Y goes down

            // Keep mouse cursor boundary pinned inside screen dimensions
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int)fb_width)  mouse_x = fb_width - 1;
            if (mouse_y >= (int)fb_height) mouse_y = fb_height - 1;
        }
    }
}

void kernel_main(unsigned int magic, unsigned int addr) {
    if (magic != 0x36d76289) {
        return; 
    }

    struct multiboot_tag_framebuffer *fb = 0;
    unsigned int *tag_ptr = (unsigned int *)(unsigned long)(addr + 8);

    while (1) {
        struct multiboot_tag *tag = (struct multiboot_tag *)tag_ptr;
        if (tag->type == 0) break;
        if (tag->type == 8) {
            fb = (struct multiboot_tag_framebuffer *)tag;
            break;
        }
        unsigned int next_tag_offset = (tag->size + 7) & ~7;
        tag_ptr = (unsigned int *)((unsigned char *)tag_ptr + next_tag_offset);
    }

    if (!fb) return;

    // Cache physical variables globally
    physical_fb = (unsigned int *)(unsigned long)fb->framebuffer_addr;
    fb_width = fb->framebuffer_width;
    fb_height = fb->framebuffer_height;
    fb_pitch = fb->framebuffer_pitch;

    init_mouse();

    // The Master GUI Compositing Loop
    while (1) {
        poll_mouse();

        // 1. Clear frame / Draw desktop background (Windows Classic Blue)
        draw_rect(0, 0, fb_width, fb_height, 0x00005A9E);

        // 2. Render a gray UI Windows Taskbar at the bottom edge dynamically
        draw_rect(0, fb_height - 45, fb_width, 45, 0x00C0C0C0);

        // 3. Render a primitive sample Window box component
        draw_rect(150, 150, 400, 250, 0x00DFDFDF);     // Main window body
        draw_rect(150, 150, 400, 30, 0x00000080);      // Dark blue header bar

        // 4. Layer the mouse vector indicator cleanly on top
        draw_mouse();

        // 5. Swap the backbuffer into display sight all at once
        swap_buffers();
        
        // Small delay loop to prevent QEMU execution lockups
        for (volatile int i = 0; i < 50000; i++);
    }
}
