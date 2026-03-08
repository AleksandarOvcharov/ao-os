#include "klog.h"
#include "vga.h"

static const char* log_level_strings[] = {
    "[INFO]  ",
    "[WARN]  ",
    "[ERROR] ",
    "[DEBUG] "
};

static uint8_t log_level_colors[] = {
    0x0A,
    0x0E,
    0x0C,
    0x0B
};

void klog(log_level_t level, const char* message) {
    uint8_t old_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t saved_color = old_color;
    
    terminal_setcolor(log_level_colors[level]);
    terminal_writestring(log_level_strings[level]);
    
    terminal_setcolor(old_color);
    terminal_writestring(message);
    terminal_writestring("\n");
    
    terminal_setcolor(saved_color);
}

void klog_info(const char* message) {
    klog(LOG_INFO, message);
}

void klog_warn(const char* message) {
    klog(LOG_WARN, message);
}

void klog_error(const char* message) {
    klog(LOG_ERROR, message);
}

void klog_debug(const char* message) {
    klog(LOG_DEBUG, message);
}
