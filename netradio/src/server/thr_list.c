#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "thr_list.h"
#include "medialib.h"
#include "server_conf.h"
#include "../include/proto.h"

// 线程号
static pthread_t tid_list;
// 节目单包含的节目数量
static int nr_list_ent;
// 节目单信息数组，每一条存储一个节目频道信息
static struct mlib_listentry_st *list_ent;

void *thr_list(void *p)
{
    int totalsize = sizeof(chnid_t);    // 总节目单总长度,先计算长度,后申请内存
    struct msg_list_st *entlistp = NULL;
    struct msg_listentry_st *entryp = NULL;

    for (int i = 0; i < nr_list_ent; i++)
    {
        totalsize += sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);
    }
    entlistp = malloc(totalsize);
    if(entlistp == NULL)
    {
        syslog(LOG_ERR,"thr_list中的malloc()函数返回失败:%s.",strerror(errno));
        exit(1);
    }
    entlistp->chnid = LISTCHNID;        // 0号,总节目单
    entryp = entlistp->entry;           // 每一个频道的频道信息,数组指针

    for (int i = 0; i < nr_list_ent; i++)
    {
        int size = sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);
        entryp->chnid = list_ent[i].chnid;
        entryp->len = htons(size);
        strcpy(entryp->desc,list_ent[i].desc);
        entryp = (void*)((char*)entryp + size);
        syslog(LOG_DEBUG,"频道节目单长度:%d\n",entryp->len);
    }

    while (1)
    {   
        // 使用serversocket发送到sndaddr地址信息的地方,就是广播发送
        syslog(LOG_INFO, "thr_list sndaddr :%d\n", sndaddr.sin_addr.s_addr);
        int ret = sendto(serversocket,entlistp,totalsize,0,(void*)&sndaddr,sizeof(sndaddr));
        if(ret < 0)
        {
            syslog(LOG_WARNING,"sendto()网络发送失败:%s.",strerror(errno));
            //break;
        }
        else
        {
            syslog(LOG_DEBUG,"sendto()网络发送成功,已经发送 %d 字节.",ret);
        }
        sleep(1);
    }
}

int thr_list_create(struct mlib_listentry_st *listp,int nr_ent)      // 创建线程
{
    int err;
    list_ent = listp;
    nr_list_ent = nr_ent;
    err = pthread_create(&tid_list,NULL,thr_list,NULL);
    if(err)
    {
        syslog(LOG_ERR,"thr_list_create()中的pthread_create()函数返回失败:%s.",strerror(err));
        return -1;
    }
    return 0;
}


int thr_list_destory(void)
{
    pthread_cancel(tid_list);
    pthread_join(tid_list,NULL);
    return 0;
}