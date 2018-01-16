
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "config.h"
#include "cjson\cJSON.h"

int get_file_size(const char *filename)
{
    struct _stat buf;

    return (_stat(filename, &buf) == 0) ? buf.st_size : 0;
}

int get_config_data(const char *filename, char *buf, int len)
{
    if (NULL == filename || NULL == buf)
    {
        ERR("filename or buf NULL");
        return -1;
    }

    FILE *fp = NULL;

    if (0 != fopen_s(&fp, filename, "rb"))
    {
        ERR("open %s fail", filename);
        return -2;
    }

    if (len != fread(buf, 1, len, fp))
    {
        fclose(fp);
        ERR("read %s fail", filename);
        return -3;
    }

    fclose(fp);
    return 0;
}

int parse_config_data(const char *data, p_config conf)
{
    if (NULL == data || NULL == conf)
    {
        printf("%s data or conf NULL\n", __FUNCTION__);
        return -1;
    }

    cJSON *root = cJSON_Parse(data);

    if (NULL == root)
    {
        printf("%s parse json fail\n", __FUNCTION__);
        return -2;
    }

    cJSON *log = cJSON_GetObjectItem(root, "log");
    cJSON *server = cJSON_GetObjectItem(root, "server");
    cJSON *monitor = cJSON_GetObjectItem(root, "monitor");

    if (NULL == log || NULL == server || NULL == monitor)
    {
        cJSON_Delete(root);
        printf("%s no log,server,monitor node\n", __FUNCTION__);
        return -3;
    }

    if (0 != xt_log_config(".\\", root, &(conf->log)))
    {
        cJSON_Delete(root);
        printf("%s parse log node error\n", __FUNCTION__);
        return -3;
    }

    int server_max = sizeof(conf->srv) / sizeof(conf->srv[0]);
    int monitor_max = sizeof(conf->mnt) / sizeof(conf->mnt[0]);
    int server_count = cJSON_GetArraySize(server);
    int monitor_count = cJSON_GetArraySize(monitor);

    if (server_count > server_max)
    {
        cJSON_Delete(root);
        printf("%s server.item count > %d", __FUNCTION__, server_max);
        return -5;
    }

    if (monitor_count > monitor_max)
    {
        cJSON_Delete(root);
        printf("%s monitor.item count > %d", __FUNCTION__, monitor_max);
        return -6;
    }

    int i;
    int j;
    int len;
    int cmd_count;
    int whitelist_count;
    int blacklist_count;
    int cmd_count_max = sizeof(conf->srv[0].cmd) / sizeof(conf->srv[0].cmd[0]);
    int whitelist_count_max = sizeof(conf->mnt[0].whitelist) / sizeof(conf->mnt[0].whitelist[0]);
    int blacklist_count_max = sizeof(conf->mnt[0].blacklist) / sizeof(conf->mnt[0].blacklist[0]);
    cJSON *cmd;
    cJSON *item;
    cJSON *addr;
    cJSON *port;
    cJSON *user;
    cJSON *pass;
    cJSON *localpath;
    cJSON *remotpath;
    cJSON *whitelist;
    cJSON *blacklist;
    cJSON *cmd_item;
    cJSON *cmd_item_cmd;
    cJSON *cmd_item_sleep;
    p_server srv;
    p_monitor mnt;

    for (i = 0; i < server_count; i++)
    {
        item = cJSON_GetArrayItem(server, i);
        addr = cJSON_GetObjectItem(item, "server");
        port = cJSON_GetObjectItem(item, "port");
        user = cJSON_GetObjectItem(item, "user");
        pass = cJSON_GetObjectItem(item, "pass");
        cmd = cJSON_GetObjectItem(item, "cmd");

        if (NULL == addr || NULL == port || NULL == user || NULL == pass)
        {
            cJSON_Delete(root);
            printf("%s no server.item[%d].addr,port,user,pass,type node", __FUNCTION__, i);
            return -7;
        }

        srv = &(conf->srv[i]);
        srv->port = port->valueint;
        strcpy_s(srv->addr, sizeof(srv->addr)-1, addr->valuestring);
        strcpy_s(srv->user, sizeof(srv->user)-1, user->valuestring);
        strcpy_s(srv->pass, sizeof(srv->pass)-1, pass->valuestring);

        cmd_count = cJSON_GetArraySize(cmd);

        if (cmd_count > cmd_count_max)
        {
            cJSON_Delete(root);
            ERR("%s no server.item[%d].cmd count > %d", __FUNCTION__, cmd_count_max);
            return -8;
        }

        for (j = 0; j < cmd_count; j++)
        {
            cmd_item = cJSON_GetArrayItem(cmd, j);
            cmd_item_cmd = cJSON_GetObjectItem(cmd_item, "cmd");
            cmd_item_sleep = cJSON_GetObjectItem(cmd_item, "sleep");

            srv->cmd[j].sleep = cmd_item_sleep->valueint;
            strcpy_s(srv->cmd[j].cmd, sizeof(srv->cmd[j].cmd)-1, cmd_item_cmd->valuestring);
        }
    }

    for (i = 0; i < monitor_count; i++)
    {
        item = cJSON_GetArrayItem(monitor, i);
        addr = cJSON_GetObjectItem(item, "server");
        localpath = cJSON_GetObjectItem(item, "localpath");
        remotpath = cJSON_GetObjectItem(item, "remotepath");
        whitelist = cJSON_GetObjectItem(item, "whitelist");
        blacklist = cJSON_GetObjectItem(item, "blacklist");

        if (NULL == addr || NULL == localpath || NULL == remotpath || NULL == whitelist || NULL == blacklist)
        {
            cJSON_Delete(root);
            printf("%s no server.item[%d].server,localpath,remotepath,whitelist,blacklist node", __FUNCTION__, i);
            return -9;
        }

        mnt = &(conf->mnt[i]);
        strcpy_s(mnt->localpath, sizeof(mnt->localpath)-1, localpath->valuestring);
        strcpy_s(mnt->remotepath, sizeof(mnt->remotepath)-1, remotpath->valuestring);

        len = (int)strlen(mnt->localpath);

        if (mnt->localpath[len - 1] != '\\') // 当不是\结尾时添加
        {
            mnt->localpath[len] = '\\';
            mnt->localpath[len + 1] = '\0';
        }

        len = (int)strlen(mnt->remotepath);

        if (mnt->remotepath[len - 1] != '/') // 当不是/结尾时添加
        {
            mnt->remotepath[len] = '/';
            mnt->remotepath[len + 1] = '\0';
        }

        // 查找server
        for (j = 0; conf->srv[j].addr[0] != 0 && j < server_count; j++)
        {
            if (0 == strcmp(addr->valuestring, conf->srv[j].addr))
            {
                mnt->server = &(conf->srv[j]);
                break;
            }
        }

        if (NULL == mnt->server)
        {
            cJSON_Delete(root);
            printf("%s no server.item[%d].server error", __FUNCTION__, i);
            return -10;
        }

        whitelist_count = cJSON_GetArraySize(whitelist);
        blacklist_count = cJSON_GetArraySize(blacklist);

        if (whitelist_count > whitelist_count_max)
        {
            cJSON_Delete(root);
            printf("%s no monitor.item[%d].whitelist count > %d", __FUNCTION__, i, whitelist_count_max);
            return -11;
        }

        if (blacklist_count > blacklist_count_max)
        {
            cJSON_Delete(root);
            ERR("%s no monitor.item[%d].blacklist count > %d", __FUNCTION__, i, blacklist_count_max);
            return -12;
        }

        for (j = 0; j < whitelist_count; j++)
        {
            item = cJSON_GetArrayItem(whitelist, j);
            strcpy_s(mnt->whitelist[j], sizeof(mnt->whitelist[j])-1, item->valuestring);
        }

        for (j = 0; j < blacklist_count; j++)
        {
            item = cJSON_GetArrayItem(blacklist, j);
            strcpy_s(mnt->blacklist[j], sizeof(mnt->blacklist[j])-1, item->valuestring);
        }
    }

    cJSON_Delete(root);
    return 0;
}

int load_config(p_config conf)
{
    if (NULL == conf)
    {
        printf("%s param null\n", __FUNCTION__);
        return -1;
    }

    char filename[512] = "";
    GetModuleFileName(NULL, filename, sizeof(filename)-1);

    char *ptr = strrchr(filename, '\\');
    strcpy_s(ptr + 1, sizeof(filename) - (ptr - filename) - 1, "conf.json");

    int size = get_file_size(filename);

    if (size <= 0)
    {
        printf("%s filename:%s size:0\n", __FUNCTION__, filename);
        return -2;
    }

    char *buf = (char*)malloc(size + 1);

    if (0 != get_config_data(filename, buf, size))
    {
        return -3;
    }

    if (0 != parse_config_data(buf, conf))
    {
        free(buf);
        return -4;
    }

    free(buf);

    //--------------------------------------------------------------------

    int i;
    int j;

    for (i = 0; conf->srv[i].addr[0] != '\0'; i++)
    {
        p_server srv = &(conf->srv[i]);
        printf("srv %s %d %s %s\n", srv->addr, srv->port, srv->user, srv->pass);

        for (j = 0; srv->cmd[j].sleep != 0; j++)
        {
            p_command cmd = &(srv->cmd[j]);
            printf("    cmd %s %d\n", cmd->cmd, cmd->sleep);
        }
    }

    for (i = 0; conf->mnt[i].localpath[0] != '\0'; i++)
    {
        p_monitor mnt = &(conf->mnt[i]);
        printf("mnt %s %s\n", mnt->localpath, mnt->remotepath);

        for (j = 0; mnt->whitelist[j][0] != '\0'; j++)
        {
            printf("    whitelist %s\n", mnt->whitelist[j]);
        }

        for (j = 0; mnt->blacklist[j][0] != '\0'; j++)
        {
            printf("    blacklist %s\n", mnt->blacklist[j]);
        }
    }

    return 0;
}
