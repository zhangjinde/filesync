

typedef struct _monitor_param
{
    char path[512];
    void *event_list;

}monitor_param, *p_monitor_param;

void* monitor_thread(void *param);
