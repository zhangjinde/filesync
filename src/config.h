
#include "common/xt_log.h"

enum
{
    TYPE_IS_NULL,
    TYPE_IS_FILE,
    TYPE_IS_DIR,
    CMD_CREATE,
    CMD_DELETE,
    CMD_RENAME,
    CMD_MODIFY,
};

typedef struct _event_head
{
    unsigned char cmd;
    unsigned char type;
    unsigned char mnt_id;
    char filename[1];

}event_head, *p_event_head;

typedef struct _command
{
    int sleep;
    char cmd[32];

}command, *p_command;

typedef struct _server
{
    int port;
    char addr[32];
    char user[32];
    char pass[32];
    void *ssh_param;
    command cmd[16];

}server, *p_server;

typedef struct _monitor
{
    char localpath[512];
    char remotepath[512];
    char whitelist[32][256];
    char blacklist[32][256];
    void *whitelist_pcre[32];
    void *blacklist_pcre[32];
    p_server server;

}monitor, *p_monitor;

typedef struct _config
{
    server srv[8];
    monitor mnt[8];
    log_info log;

}config, *p_config;

int load_config(p_config conf);

int get_file_size(const char *filename);
