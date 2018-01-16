/*************************************************
 * XT
 * Copyright (C) 2013-2015 XT. Co., Ltd.
 * File name:      xt_memory_pool.c
 * Author:         张海涛
 * Version:        1.0.0
 * Date:           2015.8.29
 * Description:    内存池实现
*************************************************/

#include "xt_memory_pool.h"
#include "xt_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define MEMORY_POOL_FLAG        0x12345678
#define MEMORY_POOL_BLUK        100

typedef struct _memory_pool
{
    int id;                         // ID
    char name[64];                  // 池名称
    p_stack free;                   // 空闲内存
    p_stack node;                   // 用于保存节点
    unsigned int mem_size;          // 内存大小
    unsigned int free_stat;         // 空闲内存统计,数据不准确,用于释放多余内存
    unsigned int free_stat_times;   // 统计次数
    unsigned int node_count;        // 节点数量
    unsigned int malloc_count;      // 分配内存数量
    pthread_mutex_t lock;           // 锁

} memory_pool, *p_memory_pool;

typedef struct _memory_head
{
    int flag;
    int size;
    int id;

}memory_head, *p_memory_head;

static int         g_pool_count = 0;
static char        g_pool_info[256] = "";
static memory_pool g_pool[64];


/**
 *\brief        创建内存池
 *\param[in]    p_memory_pool pool 内存池
 *\return       int 0成功,其它失败
 */
static int memory_pool_add_memory(p_memory_pool pool)
{
    int i;
    p_node node;
    p_memory_head head;

    for (i = 0; i < MEMORY_POOL_BLUK; i++)
    {
        node = stack_pop(pool->node);

        if (NULL == node)
        {
            pool->node_count++;
            node = (p_node)malloc(sizeof(struct _node));
        }

        head = (p_memory_head)malloc(pool->mem_size + sizeof(memory_head) + 64);

        if (NULL == head)
        {
            stack_push(pool->node, node);
            ERR("malloc mem fail errno:%d", errno);
            continue;
        }

        head->id = pool->id;
        head->size = pool->mem_size;
        head->flag = MEMORY_POOL_FLAG;
        node->data = head;
        pool->malloc_count++;
        stack_push(pool->free, node);
    }

    return 0;
}

/**
 *\brief        创建内存池
 *\param[in]    const char *name 池名称
 *\param[in]    int size 内存块大小
 *\return       int 0成功,其它失败
 */
int memory_pool_init(const char *name, int size)
{
    if (NULL == name || size <= 0)
    {
        ERR("input param is null");
        return -1;
    }

    p_memory_pool pool = &(g_pool[g_pool_count]);

    pool->id = g_pool_count++;
    pool->mem_size = size;
    pool->node_count = 0;
    pool->malloc_count = 0;
    pool->free = (p_stack)malloc(sizeof(struct _stack));
    pool->node = (p_stack)malloc(sizeof(struct _stack));
    strcpy_s(pool->name, sizeof(pool->name) - 1, name);
    stack_init(pool->free);
    stack_init(pool->node);
    pthread_mutex_init(&(pool->lock), NULL);

    return memory_pool_add_memory(pool);
}

/**
 *\brief        删除内存池
 *\return       int 0成功,其它失败
 */
int memory_pool_uninit()
{
    int i;
    p_node node;
    p_memory_pool pool;

    for (i = 0; i < g_pool_count; i++)
    {
        pool = &(g_pool[i]);

        while (true)
        {
            node = stack_pop(pool->free);

            if (NULL == node)
            {
                break;
            }

            if (NULL != node->data)
            {
                free(node->data);
            }

            free(node);
        }

        while (true)
        {
            node = stack_pop(pool->node);

            if (NULL == node)
            {
                break;
            }

            free(node);
        }

        pool->name[0] = '\0';
        pool->mem_size = 0;
        pool->node_count = 0;
        pool->malloc_count = 0;
        free(pool->free);
        free(pool->node);
        stack_init(pool->free);
        stack_init(pool->node);

        pthread_mutex_destroy(&(pool->lock));
    }

    g_pool_count = 0;

    return 0;
}

/**
 *\brief        从内存池得到内存
 *\param[in]    unsigned int size 请求大小
 *\param[in]    void **mem 内存块
 *\return       int <0失败, >0内存块大小
 */
int memory_pool_get(unsigned int size, void **mem)
{
    int i;
    p_node node;
    p_memory_head head;
    p_memory_pool pool = NULL;

    if (NULL == mem)
    {
        return -1;
    }

    for (i = 0; i < g_pool_count; i++)
    {
        if (size <= g_pool[i].mem_size)
        {
            pool = &(g_pool[i]);
            break;
        }
    }

    if (NULL == pool)
    {
        head = (p_memory_head)malloc(size + sizeof(memory_head) + 64);
        head->id = MEMORY_POOL_FLAG;
        head->flag = MEMORY_POOL_FLAG;
        head->size = size;
    }
    else
    {
        pthread_mutex_lock(&(pool->lock));

        while (true)
        {
            node = (p_node)stack_pop(pool->free);

            if (NULL == node)
            {
                memory_pool_add_memory(pool);
                continue;
            }

            head = node->data;
            stack_push(pool->node, node);
            break;
        }

        pthread_mutex_unlock(&(pool->lock));
    }

    *mem = head + 1;
    return head->size;
}

/**
 *\brief        回收内存到内存池
 *\param[in]    void * mem 内存块
 *\return       int 0-成功，其它失败
 */
int memory_pool_put(void *mem)
{
    p_node node;
    p_memory_head head;
    p_memory_pool pool;

    if (NULL == mem)
    {
        ERR("input param is null");
        return -1;
    }

    head = (p_memory_head)mem;

    if ((--head)->flag != MEMORY_POOL_FLAG)
    {
        ERR("%p dont pool mem", mem);
        return -2;
    }

    if (MEMORY_POOL_FLAG == head->id) // malloc
    {
        free(head);
        return 0;
    }

    pool = &g_pool[head->id];

    pthread_mutex_lock(&(pool->lock));

    node = stack_pop(pool->node);

    if (NULL == node)
    {
        pool->node_count++;
        node = (p_node)malloc(sizeof(struct _node));
    }

    node->data = head;

    stack_push(pool->free, node);

    pthread_mutex_unlock(&(pool->lock));

    return 0;
}

/**
 *\brief        检测释放空闲内存
 *\param[in]    bool clean 是否释放空闲内存
 *\return       int 0成功,其它失败
 */
int memory_pool_check(bool clean)
{
    int i;
    unsigned int temp;
    p_node node;
    p_memory_pool pool;

    if (!clean)
    {
        temp = 0;
        g_pool_info[0] = '\0';

        for (i = 0; i < g_pool_count; i++)
        {
            pool = &g_pool[i];
            pool->free_stat += pool->free->count;
            pool->free_stat_times++;

            temp += sprintf_s(&(g_pool_info[temp]), sizeof(g_pool_info) - temp - 1, 
                    "%u-%u-%-5u ", pool->malloc_count, pool->node_count, pool->free->count);
        }
    }
    else
    {
        for (i = 0; i < g_pool_count; i++)
        {
            pool = &g_pool[i];
            temp = (unsigned int)(pool->free_stat * 0.8 / pool->free_stat_times);

            pthread_mutex_lock(&(pool->lock));

            while (pool->free->count > temp)
            {
                node = (p_node)stack_pop(pool->free);

                if (NULL == node)
                {
                    break;
                }

                if (NULL != node->data)
                {
                    free(node->data);
                    pool->malloc_count--;
                }

                free(node);
                pool->node_count--;
            }

            pthread_mutex_unlock(&(pool->lock));

            pool->free_stat = 0;
            pool->free_stat_times = 0;
        }
    }

    return 0;
}

/**
 *\brief        内存使用信息
 *\return       char* 信息
 */
char* memory_pool_info()
{
    return g_pool_info;
}
