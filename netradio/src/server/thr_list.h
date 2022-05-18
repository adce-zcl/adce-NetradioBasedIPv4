/**
 * @file thr_list.h
 * @author your name (you@domain.com)
 * @brief 节目单线程只创建一个,所以只运行一次
 * @version 0.1
 * @date 2022-03-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef THR_LIST_H_
#define THR_LIST_H_
#include "medialib.h"
// 线程函数,只在当前文件中运行
static void *thr_list(void *p);
// 这个是线程创建函数,服务器的接口
int thr_list_create(struct mlib_listentry_st *,int);
// 销毁函数
int thr_list_destory(void);
#endif