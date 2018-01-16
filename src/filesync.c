#include <stdio.h>
#include <stdlib.h>
#include "common\xt_ssh2.h"
#include "common\xt_sftp.h"
#include "common\xt_zmodem.h"
#include "common\xt_log.h"
#include "common\xt_memory_pool.h"
#include "common\xt_list_with_lock.h"
#include "config.h"
#include "monitor.h"

#define SSHINITED 666

config              g_conf = { 0 };     // 配置信息
list_with_lock_node g_event_list;       // 队列
pthread_mutex_t     g_cs;               // 锁
time_t              g_last_time = 0;    // 上次执行命令时间
char                g_buff[1024*1024];  // 输出缓冲区

int init()
{
    if (load_config(&g_conf) != 0)
    {
        printf("%s load conf.json fail\n", __FUNCTION__);
        return -1;
    }

    if (xt_log_init(&(g_conf.log)) != 0)
    {
        printf("%s init log fail\n", __FUNCTION__);
        return -2;
    }

    list_with_lock_init(&g_event_list, "event_list");
    memory_pool_init("pool1", 1000);
    pthread_mutex_init(&g_cs, NULL);

    DBG("init log,list,memory_pool success");
    return 0;
}

int start_monitor()
{
    int i;
    pthread_t tid;

    for (i = 0; g_conf.mnt[i].server != NULL; i++)
    {
        pthread_create(&tid, NULL, monitor_thread, (void*)(__int64)i);
    }

    return (i == 0) ? -1 : 0;
}

int send_cmd(p_my_ssh_param param, const char *cmd, int sleep, const char *local_filename, const char *remote_filename)
{
    pthread_mutex_lock(&g_cs);

    int ret = ssh_send_data(param, cmd, (int)strlen(cmd));

    if (ret < 0)
    {
        printf("%s error ret:%d\n", cmd, ret);
        pthread_mutex_unlock(&g_cs);
        return ret;
    }

    ssh_send_data(param, "\r", 1);

    Sleep(sleep);

    ret = ssh_recv_data(param, g_buff, sizeof(g_buff) - 1);

    int t1;
    int t2;
    int len = 0;

    if (0 == strncmp(cmd, "rz", 2))
    {
        char *ptr = strchr(g_buff, '*');

        if (NULL != ptr)
        {
            strcpy_s(ptr, sizeof(g_buff) - (ptr - g_buff) - 1, "\n");
            printf(g_buff);

            t1 = (int)time(NULL);

            ret = file_put(param, local_filename, remote_filename, &len);

            t2 = (int)time(NULL);

            t1 = (t1 == t2) ? 1 : (t2 - t1);

            sprintf_s(g_buff, sizeof(g_buff) - 1,
                "Starting zmodem transfer.Press Ctrl + C to cancel.\n"
                "Transferring %s\n"
                "  100%%\t%d B\t%d B/s\t%d Second\t%d Errors\n\n",
                remote_filename,
                len,
                len / t1,
                t1,
                ret);

            printf(g_buff);

            ssh_recv_data(param, g_buff, sizeof(g_buff) - 1);
        }
    }
    else if (0 == strncmp(cmd, "sz", 2))
    {
        char *ptr = strchr(g_buff, '*');

        if (NULL != ptr)
        {
            strcpy_s(ptr, sizeof(g_buff) - (ptr - g_buff) - 1, "");

            if (0 != strncmp(g_buff, cmd, strlen(cmd)) && (ptr = strstr(g_buff, "\r\n")) != NULL)
            {
                char *ptr1 = ptr + 3;
                strcpy_s(ptr, sizeof(g_buff) - (ptr - g_buff) - 1, ptr1);
            }

            printf(g_buff);

            t1 = (int)time(NULL);

            ret = file_get(param, local_filename, &len);

            t2 = (int)time(NULL);

            t1 = (t1 == t2) ? 1 : (t2 - t1);

            sprintf_s(g_buff, sizeof(g_buff) - 1,
                "Starting zmodem transfer.Press Ctrl + C to cancel.\n"
                "Transferring %s\n"
                "  100%%\t%d B\t%d B/s\t%d Second\t%d Errors\n\n",
                local_filename,
                len,
                len / t1,
                t1,
                ret);

            printf(g_buff);

            ssh_recv_data(param, g_buff, sizeof(g_buff) - 1);
        }
    }

    printf(g_buff);
    g_last_time = time(NULL);
    pthread_mutex_unlock(&g_cs);
    return ret;
}

int outputCallback(void *param, const char *data, unsigned int len)
{
    printf("%s\n", data);
    return 0;
}

int sshCallback(void *param)
{
    int i;
    p_my_ssh_param ssh_param = (p_my_ssh_param)param;
    p_server server = (p_server)ssh_param->param;

    // ssh初始命令
    for (i = 0; server->cmd[i].sleep != 0; i++)
    {
        send_cmd(ssh_param, server->cmd[i].cmd, server->cmd[i].sleep, NULL, NULL);
    }

    ssh_param->param3 = (void*)SSHINITED;

    while (ssh_param->run)
    {
        if ((time(NULL) - g_last_time) > 60)
        {
            send_cmd(ssh_param, "", 100, NULL, NULL);
        }

        Sleep(100);
    }

    printf("ssh exit\n");
    return 0;
}

void process_path(char *path)
{
    char *ptr = path;
    while (*ptr != '\0')
    {
        if (*ptr == '\\')
        {
            *ptr = '/';
        }

        ptr++;
    }
}

int process_event()
{
    p_server server;
    p_monitor monitor;
    p_event_head head;
    p_my_ssh_param param;
    char *ptr;
    char temp[1024];
    char local_filename[512];
    char remote_filename[512];

    while (true)
    {
        head = (p_event_head)list_with_lock_pop(&g_event_list);

        if (NULL == head)
        {
            sleep(1);
            continue;
        }

        monitor = &(g_conf.mnt[head->mnt_id]);
        server = monitor->server;

        if (NULL == server->ssh_param)
        {
            param = (p_my_ssh_param)malloc(sizeof(my_ssh_param));
            param->run = FALSE;
            param->type = SSH_TYPE_SSH; // SSH_TYPE_SSH,SSH_TYPE_SFTP
            param->server_addr = server->addr;
            param->server_port = server->port;
            param->username = server->user;
            param->password = server->pass;
            param->data_cb = outputCallback;
            param->worker_cb = sshCallback;
            param->param = server;
            server->ssh_param = param;

            _beginthread(ssh_thread_func, 0, param);

            while (param->param3 != (void*)SSHINITED)
            {
                Sleep(100);
            }

            zmodem_set(ssh_send_data, ssh_recv_data);
        }

        switch (head->cmd)
        {
        case CMD_CREATE:
        {
            if (head->type == TYPE_IS_DIR)
            {
                sprintf_s(temp, sizeof(temp) - 1, "mkdir %s%s", monitor->remotepath, head->filename);
                process_path(temp);
                send_cmd(server->ssh_param, temp, 100, NULL, NULL);
            }
            else
            {
                sprintf_s(temp, sizeof(temp) - 1, "> %s%s", monitor->remotepath, head->filename);
                process_path(temp);
                send_cmd(server->ssh_param, temp, 100, NULL, NULL);
            }
            break;
        }
        case CMD_DELETE: // 删除文件或目录
        {
            sprintf_s(temp, sizeof(temp) - 1, "rm -rf %s%s", monitor->remotepath, head->filename);
            process_path(temp);
            send_cmd(server->ssh_param, temp, 100, NULL, NULL);
            break;
        }
        case CMD_RENAME: // 重命名文件或目录
        {
            ptr = strchr(head->filename, '|');
            *ptr++ = '\0'; // 指向旧文件名
            sprintf_s(temp, sizeof(temp) - 1, "mv -f %s%s %s%s", monitor->remotepath, ptr, monitor->remotepath, head->filename);
            process_path(temp);
            send_cmd(server->ssh_param, temp, 100, NULL, NULL);
            break;
        }
        case CMD_MODIFY: // 删除文件或目录
        {
            sprintf_s(local_filename, sizeof(local_filename) - 1, "%s%s", monitor->localpath, head->filename);
            sprintf_s(remote_filename, sizeof(remote_filename) - 1, "%s%s", monitor->remotepath, head->filename);
            process_path(remote_filename);
            send_cmd(server->ssh_param, "rz -y", 100, local_filename, remote_filename);
            break;
        }
        default:
        {
            break;
        }
        }
    }
}

// monitor->queue->process->sshclient
int main(int argc, char *argv[])
{
    if (init() != 0)
    {
        printf("%s init fail\n", __FUNCTION__);
        return -1;
    }

    if (start_monitor() != 0) // 可监控多个目录
    {
        ERR("startMonitor error");
        return -2;
    }

    process_event(); // 内部启动SSH客户端
    return 0;
}
