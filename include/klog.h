#ifndef KLOG_H
#define KLOG_H

typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG
} log_level_t;

void klog(log_level_t level, const char* message);
void klog_info(const char* message);
void klog_warn(const char* message);
void klog_error(const char* message);
void klog_debug(const char* message);

#endif
