#ifndef PANIC_H
#define PANIC_H

void panic(const char* message);
void panic_assert(const char* file, int line, const char* desc);

#define PANIC(msg) panic(msg)
#define ASSERT(condition) ((condition) ? (void)0 : panic_assert(__FILE__, __LINE__, #condition))

#endif
