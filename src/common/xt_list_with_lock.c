/*************************************************
 * XT
 * Copyright (C) 2013-2015 XT. Co., Ltd.
 * File name:      xt_list_with_lock.c
 * Author:         张海涛
 * Version:        1.0.0
 * Date:           2015.8.29
 * Encoding:       utf-8
 * Description:    列表定义,包含头节点,线程安全
*************************************************/

#include "xt_list_with_lock.h"
#include "xt_log.h"
#include <stdlib.h>
#include <string.h>

/**
 *\param[in]    p_list_with_lock_node list 链表的头结点
 *\param[in]    const char *name 链表名
 *\return       int 0成功,其它失败
 */
int list_with_lock_init(p_list_with_lock_node list, const char *name)
{
    if (NULL == list || NULL == name)
    {
        ERR("input param is null");
        return -1;
    }

    strcpy_s(list->name, sizeof(list->name) - 1, name);
    list->list = (p_list)malloc(sizeof(struct _list));
    list->free_node = (p_stack)malloc(sizeof(struct _stack));
    list->node_count = 0;
    list_init(list->list);
    stack_init(list->free_node);
    pthread_mutex_init(&(list->lock), NULL);
    return 0;
}

/**
 *\param[in]    p_list_with_lock_node list 链表的头结点
 *\return       int 0成功,其它失败
 */
int list_with_lock_uninit(p_list_with_lock_node list)
{
    if (NULL == list)
    {
        ERR("input param is null");
        return -1;
    }

    list->node_count = 0;
    free(list->list);
    free(list->free_node);
    pthread_mutex_destroy(&(list->lock));
    return 0;
}

/**
 *\param[in]    p_list_with_lock_node list 链表的头结点
 *\param[in]    void *data 数据
 *\return       int 0成功,其它失败
 */
int list_with_lock_push(p_list_with_lock_node list, void *data)
{
    if (NULL == list)
    {
        ERR("input param is null");
        return -1;
    }

    pthread_mutex_lock(&(list->lock));

    p_node node = stack_pop(list->free_node);

    if (NULL == node)
    {
        list->node_count++;
        node = (p_node)malloc(sizeof(struct _node));
    }

    node->data = data;

    int ret = list_tail_push(list->list, node);

    pthread_mutex_unlock(&(list->lock));

    return ret;
}

/**
 *\param[in]    p_list_with_lock_node list 链表的头结点
 *\return       void *data 数据
 */
void* list_with_lock_pop(p_list_with_lock_node list)
{
    if (NULL == list)
    {
        ERR("input param is null");
        return NULL;
    }

    void *data = NULL;

    pthread_mutex_lock(&(list->lock));

    p_node node = list_head_pop(list->list);

    if (NULL != node)
    {
        data = node->data;
        stack_push(list->free_node, node);
    }

    pthread_mutex_unlock(&(list->lock));

    return data;
}

/**
 *\param[in]    p_list_with_lock_node list 链表的头结点
 *\param[in]    bool clean 是否清除节点
 *\return       int 0成功,其它失败
 */
int list_check(p_list_with_lock_node list, bool clean)
{
    if (NULL == list)
    {
        return -1;
    }

    if (!clean)
    {
        list->node_stat += list->list->count;
        list->node_stat_times++;
    }
    else
    {
        p_node node;
        unsigned int max;

        max = (unsigned int)(list->node_stat * 1.2 / list->node_stat_times);

        pthread_mutex_lock(&(list->lock));

        while (list->free_node->count > max)
        {
            node = stack_pop(list->free_node);

            if (NULL == node)
            {
                break;
            }

            free(node);
            list->node_count--;
        }

        pthread_mutex_unlock(&(list->lock));

        list->node_stat = 0;
        list->node_stat_times = 0;
    }

    return 0;
}
