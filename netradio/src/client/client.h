#ifndef CLIENT_H_
#define CLIENT_H_
#define DEFAULT_PLAYERCMD "/usr/bin/mpg123 - > /dev/null"     // 播放器, - 表示播放标准输入的内容

struct client_conf_st
{
    char *rcvport;      // 接收端口
    char *mgroup;       // 多播地址
    char *player_cmd;   // 播放器
};


#endif