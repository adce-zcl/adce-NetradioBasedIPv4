/**
 * @file medialib.h
 * @author your name (you@domain.com)
 * @brief 媒体库
 * 负责解析媒体库，从媒体库中分析出来有多少个频道目录
 * 以及每一个频道的描述信息；
 * 所有的信息都是从这个模块里面获取的；
 * 其他两个模块是将媒体库的内容通过socket往外面发送
 * 
 * 推荐流量控制从根本上做起，所以建议从媒体库这里做流量控制
 * @version 0.1
 * @date 2022-02-28
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef MEDIALIB_H_
#define MEDIALIB_H_
#include "../include/proto.h"
#include <stdint.h>
#include <stddef.h>     // size_t

// 记录每一个频道的描述; 
// 如ch1- 体育,描述文件为.text文件
struct mlib_listentry_st
{
    chnid_t chnid;      // 频道号
    char *desc;         // 频道描述，不通过网络传输，自己使用
};
// 创建频道数组,用于下一步解析每一个频道
// 服务端的接口
int mlib_getchnlist(struct mlib_listentry_st **,int *);

// 释放空间,释放每一个频道的内存
int mlib_freechnlist(struct mlib_listentry_st*);

// 读取每一个频道的信息                
int mlib_readchnl(chnid_t,void*,size_t);

// 从每一个频道中,再解析出多个歌曲单
// struct channel_context_st 结构体是对用于隐藏的,因为它声明在.c文件中
static struct channel_context_st *pathtoentry(const char *path);

#endif