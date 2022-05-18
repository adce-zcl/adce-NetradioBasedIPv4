#ifndef SERVER_CONF_H_
#define SERVER_CONF_H_
#define DEFAULT_MEDIADIR "/var/media"
#define DEFAULT_IFNAME "ens33"      // 网卡名称
enum
{
    RUN_DAEMON = 1,     // 后台
    RUN_FOREGROUND      // 前台
};

struct server_conf_st   // 服务器的信息设置
{
    char *rcvport;      // 默认的接收端口
    char *mgroup;       // 默认的多播组
    char *media_dir;    // 媒体库的位置,是自己定义的位置
    char runmode;       // 运行模式 -- 前台-后台
    char *ifname;       // 指定默认的网卡设备
};
// extern可以拓展变量的作用域，但是在头文件中的变量，不需要再拓展
// 只有在.c文件中声明定义的变量，可以放到.h文件里extern进行作用域的拓展
struct server_conf_st server_conf;      // 服务器信息实例    
int serversocket;                       // 服务器socket
struct sockaddr_in sndaddr;             // 发送端的地址,服务器地址
#endif