/**
 * 由server将媒体信息上传到广播地址
 * 客户端连接广播地址,从广播地址接收
 * 默认一个局域网都在一个广播域
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "../include/proto.h"
#include "server_conf.h"
#include "thr_list.h"
#include "thr_channel.h"
#include "medialib.h"
/**
 * -M 指定多播组
 * -P 指定接收端口
 * -F 前台运行
 * -D 指定媒体库位置
 * -I 指定网卡设备 eth33
 * -H 显示帮助
 */
struct server_conf_st server_conf = {
    .rcvport = DEFAULT_RCVPORT,
    .mgroup = DEFAULT_MGROUP,
    .media_dir = DEFAULT_MEDIADIR,
    .runmode = RUN_DAEMON,
    .ifname = DEFAULT_IFNAME
};

static void daemoncreate(void);
static void serversignalset(void);
static int daemonize(void);
static void daemon_exit(int s);     // 信号处理函数
static int socket_init(void);       // 初始化socket
// 这个数组指针,只在server.c中使用,所以修饰成static
// 它表示一个媒体库列表,表示媒体库ID和desc描述
static struct mlib_listentry_st *list;

int main(int argc, char **argv)
{
    /*写系统日志*/
    // 1.指定人物
    // 2.还需要稍带的信息
    // 3.当前的消息来源
    openlog("netradio",LOG_PID | LOG_PERROR,LOG_DAEMON);

    /*设置信号*/
    serversignalset();

    /*命令行分析*/
    while (1)
    {
        int c = getopt(argc,argv,"M:P:FD:I:H");
        if(c < 0)
        {
            break;
        }
        switch (c)
        {
        case 'M':{
            server_conf.mgroup = optarg;
            break;
        }
        case 'P':{
            server_conf.rcvport = optarg;
            break;
        }
        case 'F':{
            server_conf.runmode = RUN_FOREGROUND;
            break;
        }
        case 'D':{
            server_conf.media_dir = optarg;
            break;
        }
        case 'I':{
            server_conf.ifname = optarg;
            break;
        }
        case 'H':{
            printf("\t\t* -M 指定多播组\n\
                    * -P 指定接收端口\n\
                    * -F 前台运行\n\
                    * -D 指定媒体库位置\n\
                    * -I 指定网卡设备 eth33\n\
                    * -H 显示帮助\n");
            exit(0);
            break;
        }
        default:{
            printf("参数错误\n");
            abort();
        }
        }
    }

    /*守护进程实现*/
    daemoncreate();

    /*socket初始化*/
    // 调用自定义函数
    socket_init();
    syslog(LOG_DEBUG,"socket初始化成功.");
    // 先处理每一个频道的信息,再发送节目单和各个频道信息

    /*获取频道信息*/
    int list_size;
    // 全局变量数组指针list,存放每一个频道的信息
    int err = mlib_getchnlist(&list,&list_size);
    if(err)
    {
        syslog(LOG_ERR,"mlib_getchnlist()函数返回失败:%s.",strerror(errno));
        exit(1);
    }


    /*创建节目单线程*/
    if(thr_list_create(list,list_size))
    {
        syslog(LOG_ERR,"thr_list_create()函数返回失败.");
        exit(1);
    }
   
    
    /*创建频道线程*/
    // 一个频道对应一个线程
    int i;
    for ( i = 0; i < list_size; i++)
    {
        err = thr_channel_create(list+i);
        if(err)
        {
            syslog(LOG_ERR,"thr_channel_create()函数返回失败:%s.",strerror(errno));
            exit(1);
        }
    }
    syslog(LOG_DEBUG,"%d个频道线程被创建.主进程阻塞等待.",i);

    // 服务器进程不能结束
    while (1)
    {
        pause();
    }
}

void serversignalset(void)
{
    // 定义信号
    struct sigaction sa;
    sa.sa_handler = daemon_exit;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask,SIGTERM);
    sigaddset(&sa.sa_mask,SIGINT);
    sigaddset(&sa.sa_mask,SIGQUIT);

    sigaction(SIGTERM,&sa,NULL);
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGQUIT,&sa,NULL);
}

void daemoncreate(void)
{
    // 守护进程必须由信号杀死，所以要选择我们想要的信号和处理函数
    if(server_conf.runmode == RUN_DAEMON)
    {
        if(daemonize() < 0)        // 自定义守护进程函数
        {
            perror("守护进程实现失败!\n");
            exit(1);
        }
    }
    else if(server_conf.runmode == RUN_FOREGROUND)
    {
        syslog(LOG_DEBUG,"守护进程实现成功.");
    }
    else
    {
        syslog(LOG_ERR,"运行模式参数非法.");
        exit(1);
    }
}

// 一般都是杀死父进程，子进程变成孤儿进程，然后将子进程脱离控制终端
int daemonize(void)
{
    pid_t pid = fork();
    if(pid < 0)
    {
        // perror("fork()");
        // 1.写级别,一般以ERR和WORING为分界线，ERR以上进程结束，WORING以下继续运行
        // 2.要挟的内容
        syslog(LOG_ERR,"进程fork失败:%s.",strerror(errno));
        return -1;
    }
    else if(pid > 0)        // parent
    {
        exit(0);
    }
    else                    // child
    {
        // 因为守护进程是不会使用标准输入输出和出错的，都是写系统日志
        // 关闭文件流,打开空设备，重定向到0,1,2号设备上，来关闭0,1,2文件流
        int fd = open("/dev/null",O_RDWR);
        if(fd < 0)
        {
            syslog(LOG_WARNING,"子进程打开/dev/null空文件设备失败:%s.",strerror(errno));
            return -2;
        }
        dup2(fd,0);
        dup2(fd,1);
        dup2(fd,2);
        if(fd > 2)
        {
            close(fd);      // 防止内存泄漏
        }
        setsid();           // 脱离父进程的group,自己创建会话
        chdir("/");         // 把当前进程的工作路径改为一个绝对有的路径
        umask(0);           // 通常把umask值关掉
    }
    return 0;
}

// 信号处理函数,使得守护进程由异常终止转变为正常终止
void daemon_exit(int s)
{
    thr_list_destory();
    thr_channel_destroyall();
    mlib_freechnlist(list);
    syslog(LOG_DEBUG,"截获信号:%d,程序结束.",s);
    closelog();
    close(serversocket);
    exit(0);
}

int socket_init(void)
{
    // 创建socket
    serversocket = socket(AF_INET,SOCK_DGRAM,0);
    if(serversocket < 0)
    {
        syslog(LOG_ERR,"服务器套接字创建失败:%s.",strerror(errno));
        exit(1);
    }
    syslog(LOG_DEBUG,"服务器套接字创建成功.");

    // 设置socket属性
    struct ip_mreqn mreq;
    // 多播组，需要点分式转大整数
    inet_pton(AF_INET,server_conf.mgroup,&mreq.imr_multiaddr);
    // 本机地址
    inet_pton(AF_INET,"0.0.0.0",&mreq.imr_address);
    // 网卡号
    mreq.imr_ifindex = if_nametoindex(server_conf.ifname);

    // 1.套接字
    // 2.该层协议
    // 3.创建多播组
    // 4.需要的结构体
    int rset = setsockopt(serversocket,IPPROTO_IP,IP_MULTICAST_IF,&mreq,sizeof(mreq));
    if(rset < 0)
    {
        syslog(LOG_ERR,"套接字属性设置失败:%s.",strerror(errno));
        exit(1);
    }
    syslog(LOG_DEBUG,"套接字属性设置成功.");
    
    // 接收端地址信息的初始化也就是广播地址
    // 客户端是不需要bind的,服务器向广播域发送数据报
    // 服务器就相当于客户端,所以不需要bind
    sndaddr.sin_family = AF_INET;
    sndaddr.sin_port = htons(atoi(server_conf.rcvport));
    inet_pton(AF_INET,server_conf.mgroup,&sndaddr.sin_addr.s_addr);

    syslog(LOG_DEBUG,"服务器地址信息初始化成功.");
    return 0;
}