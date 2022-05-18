#include "client.h"
#include "../include/proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
// 基本设置
struct client_conf_st client_conf = {\
    .rcvport = DEFAULT_RCVPORT,\
    .mgroup = DEFAULT_MGROUP,\
    .player_cmd = DEFAULT_PLAYERCMD
};

static ssize_t writen(int fd, const char* buf,size_t len)
{
    int ret = 0;
    int pos = 0;
    while(len > 0)
    {
        ret = write(fd,buf+pos,len);
        if(ret < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            perror("write()");
            return -1;
        }
        len -= ret;
        pos += ret;
    }
    return pos;
}


/**
 * 如果不想使用224.2.2.2多播地址
 * -M --mgroup 指定多播组
 * -P --port 指定接收端口
 * -p --player 指定播放器
 * -H --help 帮助
 */
int main(int argc, char **argv)
{
    /*
     * 初始化
     * 级别：默认值，配置文件，环境变量，命令行参数
     */
    // 分析命令行
    int index = 0;
    int c = 0;
    struct option argarr[]={\
    {"port",1,NULL,'P'}, \
    {"mgroup",1,NULL,'M'},\
    {"player_cmd",1,NULL,'p'},\
    {"help",0,NULL,'H'},\
    {NULL,0,NULL,0}};
    while (1)
    {
        c = getopt_long(argc,argv,"P:M:p:H",argarr,&index);
        if(c < 0)
        {
            break;
        }
        switch (c)
        {
        case 'P':
        {
            client_conf.rcvport = optarg;
            break;
        }
        case 'M':
        {
            client_conf.mgroup = optarg;
            break;
        }
        case 'p':
        {
            client_conf.player_cmd = optarg;
            break;
        }
        case 'H':
        {
            printf("-P --port\n-M --mgroup\n-p --player_cmd\n");
            exit(0);
        }
        default:
            abort();    // 如果都不是，自己给自己发一个结束的信号
        }
    }
    
    // 建立socket
    int clientsocket = socket(AF_INET,SOCK_DGRAM,0);
    if(clientsocket < 0)
    {
        perror("socket()");
        exit(1);
    }
    
    // 设置socket属性
    struct ip_mreqn mreqn;
    inet_pton(AF_INET,client_conf.mgroup,&mreqn.imr_multiaddr);
    inet_pton(AF_INET,"0.0.0.0",&mreqn.imr_address);
    mreqn.imr_ifindex = if_nametoindex("eth33");    // 网卡设备索引号
    int set = setsockopt(clientsocket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreqn,sizeof(mreqn));
    int value = 1;
    int set2 = setsockopt(clientsocket,IPPROTO_IP,IP_MULTICAST_LOOP,&value,sizeof(value));
    if(set < 0 || set2 < 0)
    {
        perror("setsockopt()");
        exit(1);
    }

    // bind
    struct sockaddr_in localaddr;
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(atoi(client_conf.rcvport));
    inet_pton(AF_INET,"0.0.0.0",&localaddr.sin_addr.s_addr);
    int bin = bind(clientsocket,(void*)&localaddr,sizeof(localaddr));
    if(bin < 0)
    {
        perror("bind()");
        exit(1);
    }
    
    // 建立管道
    int pd[2];
    int pip = pipe(pd);
    if(pip < 0)
    {
        perror("pipe()");
        exit(1);
    }

    // fork
    pid_t pid = fork();
    if(pid < 0)
    {
        perror("fork()");
        exit(1);
    }
    if(pid == 0)    // child
    {
        // 调用解码器
        close(clientsocket);
        close(pd[1]);   // 关闭写端
        dup2(pd[0],0);  // 重定向，把pd[0] 作为 0 号文件描述符，即管道读作为标准输入
        if(pd[0] > 0)   // 重定向之后，就可以关了
        {
            close(pd[0]);
        }
        // 通过调用shell，间接调用播放器
        execl("/bin/sh","sh","-c",client_conf.player_cmd,NULL);
        perror("execl()");
        exit(0);
    }
    else            // parent
    {
        close(pd[0]);
        // 从网络上收包，发送给子进程

        // 收节目单
        struct msg_list_st *msg_list;
        msg_list = malloc(MSG_LIST_MAX);
        if(msg_list == NULL)
        {
            perror("malloc()");
            exit(1);
        }
        struct sockaddr_in serveraddr;
        socklen_t serveraddr_len;
        int len = 0;
        while (1)
        {
            len = recvfrom(clientsocket,msg_list,MSG_LIST_MAX,0,(void*)&serveraddr,&serveraddr_len);
            if(len < sizeof(struct msg_list_st))    // 小于一个结构体，不正确
            {
                fprintf(stderr,"数据包太小,已经丢弃.\n");
                continue;
            }
            if(msg_list->chnid != LISTCHNID)    // 不是节目单，放弃重收
            {
                fprintf(stderr,"不是节目单包,是第%d号频道包.\n",msg_list->chnid);
                continue;
            }
            break;  // 收到节目单
        }

        // 打印节目单并且选择频道
        // 没办法使用数组进行传输,
        // 网络传输是流式的,只能在包中记录长度,自己做分割.
        struct msg_listentry_st *pos = NULL;
        for (pos = msg_list->entry; \
        (char *)pos < (((char *)msg_list) + len); \
        pos = (void *)(((char *)pos) + ntohs(pos->len)))
        {
            printf("%d号频道:%s\n",pos->chnid,pos->desc);
        }
        free(msg_list);
        printf("选择节目单：");
        int chosenid;
        // 选择
        while (chosenid < 1)
        {
            int ret = scanf("%d",&chosenid);
            if(ret != 1)
            {
                exit(1);
            }
        }
        printf("已经选择%d号频道.\n",chosenid);
        // 收频道报，发送给子进程
        struct msg_channel_st *msg_channel = NULL;
        msg_channel = malloc(MSG_CHANNEL_MAX);
        if(msg_channel == NULL)
        {
            perror("malloc()");
            exit(1);
        }
        // 收报
        while (1)
        {
            int len = recvfrom(clientsocket,msg_channel,MSG_CHANNEL_MAX,0,(void*)&serveraddr,&serveraddr_len);
            if(len < sizeof(struct msg_channel_st))
            {
                fprintf(stderr,"频道包太小,已经丢弃!\n");
                continue;
            }
            if(msg_channel->chnid == chosenid)  // 找到目标报文
            {
                // 写入到管道
                fprintf(stdout,"接收频道:%d.\n",msg_channel->chnid);
                if(writen(pd[1],msg_channel->data,len-sizeof(chnid_t)) < 0)
                {
                    exit(1);
                }
            }
        }
        free(msg_channel);
        close(clientsocket);
    }
    exit(0);
}