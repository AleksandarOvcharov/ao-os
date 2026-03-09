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
void cmd_diskinfo(void);
void cmd_sconsole(const char* args);
void cmd_ls(void);
void cmd_cat(const char* args);
void cmd_write(const char* args);
void cmd_rm(const char* args);
void cmd_touch(const char* args);
void cmd_edit(const char* args);
void cmd_checkfs(void);
void cmd_install(void);
void cmd_mkdir(const char* args);
void cmd_rmdir(const char* args);
void cmd_cd(const char* args);
void cmd_pwd(void);
void cmd_divan(void);

#endif
