
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winnt.h>
#include <shellAPI.h>
#include "monitor.h"
#include "config.h"
#include "pcre\pcre.h"
#include "common\xt_log.h"
#include "common\xt_memory_pool.h"
#include "common\xt_list_with_lock.h"

extern config               g_conf;                         // 全局的配置
extern list_with_lock_node  g_event_list;                   // 全局的队列


int monitor_thread_init(p_monitor monitor)
{
    int i;
    int offset;
    char *error;
    pcre *pcre_data;

    for (i = 0; i < monitor->whitelist[i][0] != '\0'; i++)
    {
        pcre_data = pcre_compile(monitor->whitelist[i], 0, &error, &offset, NULL);

        if (pcre_data == NULL)
        {
            ERR("PCRE init fail pattern:%s offste:%d %s", monitor->whitelist[i], offset, error);
            return -1;
        }

        monitor->whitelist_pcre[i] = pcre_data;
    }

    for (i = 0; i < monitor->blacklist[i][0] != '\0'; i++)
    {
        pcre_data = pcre_compile(monitor->blacklist[i], 0, &error, &offset, NULL);

        if (pcre_data == NULL)
        {
            ERR("PCRE init fail pattern:%s offste:%d %s", monitor->blacklist[i], offset, error);
            return -2;
        }

        monitor->blacklist_pcre[i] = pcre_data;
    }

    return 0;
}

int get_type(wchar_t *path, int path_len, int path_size, const wchar_t *filename, int filename_len)
{
    wcsncpy_s(&(path[path_len]), path_size - path_len - 1, filename, filename_len);

    DWORD attr = GetFileAttributesW(path);  // 得到文件属性

    if (attr == 0xFFFFFFFF)
    {
        return TYPE_IS_NULL; // 无文件
    }

    return (attr&FILE_ATTRIBUTE_DIRECTORY) ? TYPE_IS_DIR : TYPE_IS_FILE;
}

int proc_whitelist(p_monitor monitor, const char* filename)
{
    int i;
    int ret;
    int pos[16];
    int len = sizeof(pos)/sizeof(pos[0]);

    for (i = 0; i < monitor->whitelist[i][0] != '\0'; i++)
    {
        ret = pcre_exec(monitor->whitelist_pcre[i], NULL, filename, (int)strlen(filename), 0, 0, pos, len);

        if (ret > 0) // <0发生错误，==0没有匹配上，>0返回匹配到的元素数量
        {
            return 0;
        }
    }

    DBG("whitelist fail file:%s", filename);
    return -1;
}

int proc_blacklist(p_monitor monitor, const char* filename)
{
    int i;
    int ret;
    int pos[16];
    int len = sizeof(pos)/sizeof(pos[0]);

    for (i = 0; i < monitor->blacklist[i][0] != '\0'; i++)
    {
        ret = pcre_exec(monitor->blacklist_pcre[i], NULL, filename, (int)strlen(filename), 0, 0, pos, len);

        if (ret > 0) // <0发生错误，==0没有匹配上，>0返回匹配到的元素数量
        {
            DBG("blacklist fail file:%s", filename);
            return -1;
        }
    }

    return 0;
}

void* monitor_thread(void *param)
{
    int mnt_id = (unsigned int)(unsigned __int64)param;
    p_monitor monitor = &(g_conf.mnt[mnt_id]);
    char *path = monitor->localpath;

    if (0 != monitor_thread_init(monitor))
    {
        return (void*)-1;
    }

    // 打开目录，得到目录的句柄
    HANDLE handle = CreateFile(path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);

    if (handle == INVALID_HANDLE_VALUE)
    {
        ERR("open %s fail", path);
        return (void*)-2;
    }

    DBG("open %s success", path);

    int ret = 0;
    int len = 10 * 1024 * 1024;
    char *buf = (char*)malloc(len);
    FILE_NOTIFY_INFORMATION *notify = NULL;

    int cmd;
    int size;
    int type;
    int last_cmd = 0;
    int last_tick = 0;
    char filename[1000];
    char last_filename[1000];
    p_event_head head;

    int wpath_size;
    int wpath_len;
    wchar_t wpath[512];

    wpath_size = sizeof(wpath) / 2;
    wpath_len = MultiByteToWideChar(CP_ACP, 0, path, strlen(path), wpath, wpath_size);
    wpath[wpath_len] = 0;

    while (true)
    {
        notify = (FILE_NOTIFY_INFORMATION*)buf;

        // 设置监控类型,只有用UNICODE类型的函数
        if (!ReadDirectoryChangesW(handle,
            notify,
            len,
            true,
            FILE_NOTIFY_CHANGE_CREATION |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME,
            &ret,
            NULL,
            NULL))
        {
            ERR("ReadDirectoryChangesW fail,%d", GetLastError());
            Sleep(100);
            continue;
        }

        if (0 == ret)
        {
            ERR("ReadDirectoryChangesW fail,overflow");
            continue;
        }

        while (true)
        {
            type = get_type(wpath, wpath_len, wpath_size, notify->FileName, notify->FileNameLength / 2);
            size = WideCharToMultiByte(CP_UTF8, 0, notify->FileName, notify->FileNameLength / 2, filename, sizeof(filename) - 1, NULL, NULL);
            filename[size] = '\0';
            cmd = 0;

            switch (notify->Action)
            {
            case FILE_ACTION_ADDED: // 新建文件或目录
            {
                cmd = CMD_CREATE;
                break;
            }
            case FILE_ACTION_REMOVED: // 删除文件或目录
            {
                cmd = CMD_DELETE;
                break;
            }
            case FILE_ACTION_RENAMED_OLD_NAME: // 重命名，旧名
            {
                strcpy_s(last_filename, sizeof(last_filename) - 1, filename);
                break;
            }
            case FILE_ACTION_RENAMED_NEW_NAME: // 重命名，新名
            {
                cmd = CMD_RENAME;
                filename[size] = '|';
                filename[size + 1] = '\0';
                strcat_s(filename, sizeof(filename) - 1, last_filename); // 格式:新名|旧名
                break;
            }
            case FILE_ACTION_MODIFIED: // 修改文件或目录,同时可能会有多个修改命令
            {
                if ((type == TYPE_IS_DIR) ||
                    (last_cmd == CMD_MODIFY && 0 == strcmp(last_filename, filename) && (GetTickCount() - last_tick) < 500))
                {
                    cmd = 0;
                }
                else
                {
                    cmd = CMD_MODIFY;
                    strcpy_s(last_filename, sizeof(last_filename) - 1, filename);
                }

                break;
            }
            default:
            {
                break;
            }
            }

            if (0 != cmd && 0 == proc_whitelist(monitor, filename) && 0 == proc_blacklist(monitor, filename))
            {
                last_cmd = cmd;
                last_tick = GetTickCount();
                DBG("tick:%u cmd:%u type:%d file:%s", last_tick, last_cmd, type, filename);

                size = memory_pool_get(sizeof(event_head), (void**)&head);
                head->cmd = cmd;
                head->type = type;
                head->mnt_id = mnt_id;
                strcpy_s(&(head->filename[0]), size, filename);
                list_with_lock_push(&g_event_list, head);
            }

            if (0 == notify->NextEntryOffset)
            {
                break;
            }

            notify = (FILE_NOTIFY_INFORMATION*)((char*)notify + notify->NextEntryOffset);
        }
    }
}
