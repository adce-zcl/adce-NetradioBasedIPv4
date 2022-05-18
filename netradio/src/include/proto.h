/**
 * @file proto.h
 * @author your name (you@domain.com)
 * @brief 使用udp广播 -- 报式传输
 * @version 0.1
 * @date 2022-02-27
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef PROTO_H_
#define PROTO_H_
#include "site_type.h"
#define DEFAULT_MGROUP "224.2.2.2"          // 默认的多播地址
#define DEFAULT_RCVPORT "1989"              // 默认的接收端口
#define CHNNRNUM 100                        // 总的频道数量
#define MINCHNID 1                          // 最小的频道号
#define LISTCHNID 0                         // 节目单的频道号为0
#define MAXCHNID 100                        // 最大的节目单号

#define MSG_CHANNEL_MAX (65536-20-8)                // 数据报的最大长度
#define MAX_DATA (MSG_CHANNEL_MAX-sizeof(chnid_t))  // 数据的最大长度

#define MSG_LIST_MAX (65535-20-8)                   // 节目单报的最大长度
#define MAX_ENTRY (MSG_LIST_MAX-sizeof(chnid_t))    // 节目单里数据的最大长度

// 这些定义是需要在网络上传输的包的结构体


// 有两种传输数据的包：数据，节目单
struct msg_channel_st       // 频道歌曲包
{
    chnid_t chnid;          // ID号 must between [MINCHNID,MAXCHNID]
    uint8_t data[1];        // 数据,最大长度为MAX_DATA,代表歌曲的内容
}__attribute__((packed));   // 不对齐

struct msg_listentry_st     // list节目单，每一条记录的报，一个结构体代表一个媒体信息
{
    chnid_t chnid;          // 每一个频道的频道ID
    uint16_t len;           // 表明当前结构体的大小，用来区分连续的结构体
    uint8_t desc[1];        // 最大长度为MAX_ENTRY,频道描述信息
}__attribute__((packed));

struct msg_list_st          // 总的节目单结构体
{
    chnid_t chnid;          // must be LISTCHNID,只能为0
    struct msg_listentry_st entry[1]; 

}__attribute__((packed));



#endif