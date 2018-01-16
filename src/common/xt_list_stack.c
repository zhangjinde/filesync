/*************************************************
 * XT
 * Copyright (C) 2013-2015 XT. Co., Ltd.
 * File name:      xt_list_stack.c
 * Author:         张海涛
 * Version:        1.0.0
 * Date:           2015.8.29
 * Description:    双向循环列表实现,包含头节点,非线程安全
*************************************************/

#include "xt_list_stack.h"
#include "xt_log.h"

#ifdef WIN32
#include <Windows.h>
#define CAS(ptr,oldval,newval)  (oldval == InterlockedCompareExchange((long*)ptr, newval, oldval))
#else
#define CAS(ptr,oldval,newval)  __sync_bool_compare_and_swap(ptr, oldval, newval)
#endif


/**
 *\brief        栈初始化
 *\param[out]   p_stack stack 栈
 *\return       int 0成功,其它失败
 */
int stack_init(p_stack stack)
{
    if (NULL == stack)
    {
        ERR("input param is null");
        return -1;
    }

    stack->top = NULL;
    stack->count = 0;
    return 0;
}

/**
 *\brief     节点入栈
 *\param[in] p_stack stack 栈
 *\param[in] p_stack_node node 节点
 *\return    int 0成功,其它失败
 */
int stack_push(p_stack stack, p_node node)
{
    if (NULL == stack || NULL == node)
    {
        ERR("input param is null");
        return -1;
    }

    /*
    p_stack_node top = NULL;

    do
    {
        top = stack->top;

        node->next = top;

    } while (!CAS(&(stack->top), top, node));
    */

    node->next = stack->top;
    stack->top = node;
    stack->count++;
    return 0;
}

/**
 *\brief     节点出栈
 *\param[in] p_stack stack 栈
 *\return    void* 数据或NULL
 */
p_node stack_pop(p_stack stack)
{
    if (NULL == stack)
    {
        ERR("input param is null");
        return NULL;
    }

    /*
    do
    {
        top = stack->top;

        if (NULL == top)
        {
            pthread_mutex_unlock(&stack->mutex);
            return NULL;
        }

    } while (!CAS(&(stack->top), top, top->next));
    */

    p_node top = stack->top;

    if (NULL == top)
    {
        return NULL;
    }

    stack->top = top->next;
    stack->count--;
    return top;
}


/**
 *\brief        遍历链表
 *\param[in]    p_list list 链表
 *\param[in]    LIST_TRAVERSE_PROC proc 执行函数
 *\param[in]    void *param 自定义数据
 *\return       int 0成功,其它失败
 */
int list_traverse(p_list list, LIST_TRAVERSE_PROC proc, void *param)
{
    p_node first = NULL;
    p_node node = NULL;

    if (NULL == list || NULL == proc)
    {
        return -1;
    }

    first = list->head;

    proc(first, param);

    if (first == first->next) // 只有一个节点
    {
        return 0;
    }

    node = first->next;

    while (node != first && 0 == proc(node, param))
    {
        node = node->next;
    }

    return 0;
}

/**
 *\brief        在节点后插入新节点
 *\param[in]    p_list_node curr 当前结点
 *\param[in]    p_list_node node 新节点
 *\return       p_list_node 新节点
 */
static p_node list_next_insert(p_node curr, p_node node)
{
    if (NULL != curr)
    {
        node->prev = curr;
        node->next = curr->next;
        curr->next->prev = node;
        curr->next = node;
    }
    else
    {
        node->prev = node;
        node->next = node;
    }

    return node;
}

/**
 *\param[in]    p_list_node node 结点
 *\param[in]    void *data 数据
 *\return       p_list_node 下一节点
 */
static p_node list_delete(p_node node)
{
    if (node->next == node) // 只有一个节点
    {
        return NULL;
    }

    p_node next = node->next;
    node->prev->next = node->next;
    node->next->prev = node->prev;

    return next;
}

/**
 *\brief        链表初始化
 *\param[in]    p_list list 链表
 *\return       int 0成功,其它失败
 */
int list_init(p_list list)
{
    if (NULL == list)
    {
        return -1;
    }

    list->count = 0;
    list->head = NULL;
    return 0;
}

/**
 *\brief        在链表头部添加数据
 *\param[in]    p_list list 链表
 *\param[in]    p_node node 链表结点
 *\return       int 0成功,其它失败
 */
int list_head_push(p_list list, p_node node)
{
    if (list->head == NULL)
    {
        node->next = node;
        node->prev = node;
        list->head = node;
    }
    else
    {
        list->head = list_next_insert(list->head->prev, node);
    }

    list->count++;
    return 0;
}

/**
 *\brief        在链表尾部添加数据
 *\param[in]    p_list list 链表
 *\param[in]    p_node node 链表结点
 *\return       int 0成功,其它失败
 */
int list_tail_push(p_list list, p_node node)
{
    if (list->head == NULL)
    {
        list->head = list_next_insert(NULL, node);
    }
    else
    {
        list_next_insert(list->head->prev, node);
    }

    list->count++;
    return 0;
}

/**
 *\brief        从链表头部得到数据
 *\param[in]    p_list list 链表
 *\return       p_node 结点
 */
p_node list_head_pop(p_list list)
{
    if (NULL == list || NULL == list->head)
    {
        return NULL;
    }

    p_node node = list->head;
    list->head = list_delete(node);
    list->count--;
    return node;
}

/**
 *\brief        从链表尾部得到数据
 *\param[in]    p_list list 链表
 *\return       p_node 结点
 */
p_node list_tail_pop(p_list list)
{
    if (NULL == list || NULL == list->head)
    {
        return NULL;
    }

    p_node node = list->head->prev;

    if (NULL == list_delete(node))
    {
        list->head = NULL;
    }

    list->count--;
    return node;
}
