#ifndef COMMANDS_H
#define COMMANDS_H

void cmd_help(void);
void cmd_clear(void);
void cmd_echo(const char* args);
void cmd_about(void);
void cmd_reboot(void);
void cmd_shutdown(void);
void cmd_color(const char* args);
void cmd_kernel(const char* args);
void cmd_mem(void);
void cmd_uptime(void);
void cmd_sysinfo(void);

#endif
