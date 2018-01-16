/*************************************************
 * XT
 * Copyright (C) 2013-2015 XT. Co., Ltd.
 * File name:      xt_list_stack.h
 * Author:         张海涛
 * Version:        1.0.0
 * Date:           2015.8.29
 * Description:    双向循环列表定义,包含头节点,非线程安全
*************************************************/

#ifndef _XT_LIST_STACK_H_
#define _XT_LIST_STACK_H_

#ifndef bool
#define bool int
#endif

typedef struct _node
{
    void *data;
    struct _node *next;
    struct _node *prev;

} node, *p_node;

typedef struct _stack
{
    unsigned int count;
    p_node top;

} stack, *p_stack;

typedef struct _list
{
    unsigned int count;
    struct _node *head;

} list, *p_list;

typedef int(*LIST_TRAVERSE_PROC)(p_node node, void *param);

/**
 *\brief        栈初始化
 *\param[out]   p_stack stack 栈
 *\return       int 0成功,其它失败
 */
int stack_init(p_stack stack);

/**
 *\brief     节点入栈
 *\param[in] p_stack stack 栈
 *\param[in] p_stack_node node 节点
 *\return    int 0成功,其它失败
 */
int stack_push(p_stack stack, p_node node);

/**
 *\brief     节点出栈
 *\param[in] p_stack stack 栈
 *\return    void* 数据或NULL
 */
p_node stack_pop(p_stack stack);

/**
 *\brief        链表初始化
 *\param[in]    p_list list 链表
 *\return       int 0成功,其它失败
 */
int list_init(p_list list);

/**
 *\brief        在链表头部添加数据
 *\param[in]    p_list list 链表
 *\param[in]    p_node node 链表结点
 *\return       int 0成功,其它失败
 */
int list_head_push(p_list list, p_node node);

/**
 *\brief        在链表尾部添加数据
 *\param[in]    p_list list 链表
 *\param[in]    p_node node 链表结点
 *\return       int 0成功,其它失败
 */
int list_tail_push(p_list list, p_node node);

/**
 *\brief        从链表头部得到数据
 *\param[in]    p_list list 链表
 *\return       p_node 结点
 */
p_node list_head_pop(p_list list);

/**
 *\brief        从链表尾部得到数据
 *\param[in]    p_list list 链表
 *\return       p_node 结点
 */
p_node list_tail_pop(p_list list);

/**
 *\brief        遍历链表
 *\param[in]    p_list list 链表
 *\param[in]    LIST_TRAVERSE_PROC proc 执行函数
 *\param[in]    void *param 自定义数据
 *\return       int 0成功,其它失败
 */
int list_traverse(p_list list, LIST_TRAVERSE_PROC proc, void *param);

#endif
