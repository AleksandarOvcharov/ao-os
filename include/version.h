#ifndef VERSION_H
#define VERSION_H

#define KERNEL_VERSION_MAJOR 1
#define KERNEL_VERSION_MINOR 2
#define KERNEL_VERSION_PATCH 0

#define KERNEL_VERSION_STRING "1.2.0"
#define KERNEL_CODENAME "Aurora"
#define KERNEL_BUILD_DATE __DATE__

void kernel_print_version(void);
void kernel_print_full_info(void);

#endif
