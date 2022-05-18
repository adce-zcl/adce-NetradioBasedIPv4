#include <pthread.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "thr_channel.h"
#include "server_conf.h"
#include "../include/proto.h"

// 线程号和频道号对应,每一个线程负责一个频道
struct thr_channel_ent_st
{
    chnid_t chnid;
    pthread_t tid;
};

// 结构体数组,和线程数量相关
struct thr_channel_ent_st thr_channel[CHNNRNUM];
static int tid_nextpos = 0;

void *thr_channel_snder(void* ptr)
{
    struct mlib_listentry_st *ent = ptr;
    struct msg_channel_st *sbufp = NULL;      // 真正包含歌曲的数据报
    sbufp = malloc(MSG_CHANNEL_MAX);
    if(sbufp == NULL)
    {
        syslog(LOG_ERR,"thr_channel_snder中malloc()函数失败:%s.",strerror(errno));
        exit(1);
    }
    sbufp->chnid = ent->chnid;
    while(1)
    {
        // 自定义一个函数
        // 读
        //len = mlib_readchn(ent->chnid, sbufp->data, MAX_DATA);
        int len = mlib_readchnl(ent->chnid,sbufp->data,MAX_DATA);
        syslog(LOG_DEBUG,"mlib_readchnl()读到的字节数:%d.",len);
        // 发
        if(sendto(serversocket,sbufp,len+sizeof(chnid_t),0,(void*)&sndaddr,sizeof(sndaddr)) < 0)
        {
            syslog(LOG_ERR,"thr_channel(%s)中sendto()函数失败:%s.",ent->chnid,strerror(errno));
            break;
        }
        sched_yield();      // 主动出让调度器,当循环阻塞时
    }
    pthread_exit(NULL);
}

// 为每一个频道创建一个线程
// 这个函数在外围应该被循环调用
int thr_channel_create(struct mlib_listentry_st *ptr)
{
    int err = pthread_create(&thr_channel[tid_nextpos].tid,NULL,thr_channel_snder,ptr);
    if(err)
    {
        syslog(LOG_WARNING,"thr_channel_create中pthread_create()函数失败:%s.",strerror(errno));
        return -err;
    }
    thr_channel[tid_nextpos].chnid = ptr->chnid;
    tid_nextpos++;
    return 0;
}

int thr_channel_destroy(struct mlib_listentry_st *ptr)
{
    // 主要目的是把当前线程销毁
    for (int i = 0; i < CHNNRNUM; i++)
    {
        if(thr_channel[i].chnid == ptr->chnid)
        {
            if(pthread_cancel(thr_channel[i].tid) < 0)
            {
                syslog(LOG_ERR,"pthread_cancel()函数失败,线程频道号:%d.",ptr->chnid);
                return -ESRCH;
            }
        }
        pthread_join(thr_channel[i].tid,NULL);
        thr_channel[i].chnid = -1;
        return 0;
    }
}

int thr_channel_destroyall(void)
{
    for (int i = 0; i < CHNNRNUM; i++)
    {
        if(thr_channel[i].chnid > 0)
        {
            if(pthread_cancel(thr_channel[i].tid) < 0)
            {
                syslog(LOG_ERR,"pthread_cancel():函数失败,线程频道号%d.",thr_channel[i].chnid);
                return -ESRCH;
            }
            pthread_join(thr_channel[i].tid,NULL);
            thr_channel[i].chnid = -1;
        }
    }
    
}