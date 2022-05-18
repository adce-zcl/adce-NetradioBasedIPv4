#include <stdio.h>
#include <stdlib.h>
#include <glob.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include "medialib.h"
#include "mytbf.h"
#include "server_conf.h"
#define PATHSIZE 1024
#define LINEBUFSIZE 1024
#define MP3_BITRATE 1024*128 //correct bps:128*1024
// 媒体库最完整的结构体
// 解析目录结构，有多少种mp3,有多少txt文件
// 获取当前节目单的内容
struct channel_context_st
{
    chnid_t chnid;      // 频道号
    char *desc;         // 描述
    glob_t mp3glob;     // 解析出来的所有的mp3,可以存目录,也可以存具体的文件(带全路径)
    int pos;            // 播放的这首歌，要有下标表示
    int fd;             // 打开这个文件所用到的文件描述符
    off_t offset;       // 数据是一段一段发出去的，要记录offset
    mytbf_t *tbf;       // 流量控制，一定要进行流量控制，不然就是cat这个文件，一股脑的全发出去
};

// 声明结构体数组
static struct channel_context_st channel[MAXCHNID+1];

int mlib_getchnlist(struct mlib_listentry_st **result,int *resnum)      // 创建频道数组
{
    struct mlib_listentry_st *mlibptr;      // 给用户的目录,数组指针
    struct channel_context_st *channelptr;  // 每一个频道的具体信息(频道是指ch1等目录)

    // 初始化channel数组的内容
    int i;
    for ( i = 0; i < MAXCHNID+1; i++)
    {
        channel[i].chnid = -1;      // -1表示未启用
    }

    // 检查媒体库的位置
    // 1.往哪里输出 2.大小 3.格式(解析成一个新的串) 4.来源
    char path[PATHSIZE];
    glob_t globres;
    snprintf(path,PATHSIZE,"%s/*",server_conf.media_dir);
    // 1.路径 2.特殊要求 3.出错路径不关心 4.解析结果
    // globres里面解析出来的就是路径加个数
    if(glob(path,0,NULL,&globres))
    {
        // 错误
        syslog(LOG_ERR,"glob()函数出错:%s",strerror(errno));
        return -1;
    }

    // 对每一个目录再解析,拿到每一个mp3和txt文件
    mlibptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
    if(mlibptr == NULL)
    {
        syslog(LOG_ERR,"mlibptr malloc函数失败.");
        exit(1);
    }

    int num = 0;
    for (int i = 0; i < globres.gl_pathc; i++)
    {
        // 现在拿到的每一个名字就是 "/var/media/ch1" 这样的一个目录
        // 要从这个目录下面拿到mp3和txt文件
        // 将这个目录下的mp3文件和desc文件信息存放到channelptr中
        channelptr = pathtoentry(globres.gl_pathv[i]);       // 自定义函数把路径变成每一条记录
        if(channelptr != NULL)
        {
            syslog(LOG_INFO,"添加频道:%d",channelptr->chnid);
            // 把channelptr添加到channel数组中,频道号就是下标
            memcpy(channel+channelptr->chnid,channelptr,sizeof(*channelptr));
            mlibptr[num].chnid = channelptr->chnid;
            mlibptr[num].desc = strdup(channelptr->desc);
            num++;
        }
    }
    // 回填result和resnum,把mlibptr的内存复制到result中
    *result = realloc(mlibptr,sizeof(struct mlib_listentry_st) * num);
    if(*result == NULL)
    {
        syslog(LOG_ERR,"realloc函数失败.");
        exit(1);
    }
    *resnum = num;
    return 0;
}

int mlib_freechnlist(struct mlib_listentry_st *ptr)     // 释放空间
{
    free(ptr);
}

static int open_next(chnid_t chnid)
{
    for (int i = 0; i < channel[chnid].mp3glob.gl_pathc; i++)
    {
        // 失败了或者读完了,才去读下一个,一共有gl_pathc首歌
        channel[chnid].pos++;
        if(channel[chnid].pos == channel[chnid].mp3glob.gl_pathc)
        {
            // 已经是最大值了,转了一圈,一个都没有打开
            channel[chnid].pos = 0;
            break;
        }

        close(channel[chnid].fd);
        channel[chnid].fd = open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],O_RDONLY);
        if(channel[chnid].fd < 0)
        {
            // 打开又失败了
            syslog(LOG_WARNING,"open(%s):%s.",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
        }
        else
        {
            // 成功打开
            channel[chnid].offset = 0;      // 这个文件从头开始读
            return 0;
        }
    }
    syslog(LOG_ERR,"None of mp3 in channel %d is available.",chnid);
}

int mlib_readchnl(chnid_t chnid,void* buf,size_t size)     // 读取每一个频道的信息
{
    // 仿照read原语
    // 这个环节进行了流量控制
    int len = 0;
    int next_ret = 0;
    if(channel[chnid].tbf == NULL)
    {
        syslog(LOG_ERR,"channel[%d].tbf == NULL",chnid);
        return -1;
    }
    int tbf_size = mytbf_fetchtoken(channel[chnid].tbf,size);

    while(1)
    {
        // 从offset偏移量开始读,读tbfsize个,因为只有这么多令牌同
        len = pread(channel[chnid].fd,buf,tbf_size,channel[chnid].offset);
        if(len < 0)
        {
            // 这个错了,就读下一个
            syslog(LOG_WARNING,"media file %s pread():%s.",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
            next_ret = open_next(chnid);
        }
        else if(len == 0)
        {
            // 结束了,继续读下一首歌
            syslog(LOG_DEBUG,"media file %s is over.",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos]);
            next_ret = open_next(chnid);
        }
        else
        {
            // 真正读到了代发送的内容
            channel[chnid].offset += len;
            break;
        }
    }
    // 消耗了len个令牌,把剩余的归还
    if(tbf_size-len > 0)
    {
        mytbf_returntoken(channel[chnid].tbf,tbf_size-len);
    }
    return len;
    
}

// static修饰,是一个内部函数
// 解析出来的结果就会填充到channel数组中,这就是所有的频道信息
// 流媒体是以频道为单位的,客户端进入广播,具体是在播放该频道下的哪首歌是不确定的
// 所以,只要能顺序循环播放当前频道的歌曲不停止就可以了,或者只播放一变.
struct channel_context_st *pathtoentry(const char *path)
{
    // 这个函数被循环调用,一次只解析一个频道
    // 解析path/desc.txt path/*.mp3
    char pathstr[PATHSIZE]={'\0'};
    char linebuf[LINEBUFSIZE]={'\0'};
    FILE *fp = NULL;
    struct channel_context_st *me;
    // static修饰,多次调用,只分配一次内存,值可以累计
    static chnid_t curr_id = MINCHNID;
    // 申请内存
    me = malloc(sizeof(*me));
    if(me == NULL)
    {
        syslog(LOG_ERR,"pathtoentry()中malloc()函数返回失败:%s.",strerror(errno));
        return NULL;
    }
    
    // 先解析desc文件--频道描述文件
    strcpy(pathstr,path);
    strcat(pathstr,"/desc.txt");
    fp = fopen(pathstr,"r");
    if(fp == NULL)
    {
        syslog(LOG_INFO,"%s:不是频道目录,没有desc文件.",path);
        free(me);
        return NULL;
    }
    if(fgets(linebuf,LINEBUFSIZE,fp) == NULL)       // 已经打开了
    {
        syslog(LOG_INFO,"%s:不是频道目录,没有desc文件或desc文件为空.",path);
        fclose(fp);
        free(me);
        return NULL;
    }
    fclose(fp);
    // linebuf中就是desc.txt中的内容,把它拷贝到me->desc中
    me->desc = strdup(linebuf);

    // 开始分析mp3文件
    strcpy(pathstr,path);
    strcat(pathstr,"/*.mp3");
    // 解析出所有的mp3文件,全路径存放在me当中
    if(glob(pathstr,0,NULL,&me->mp3glob) != 0)      // glob返回失败
    {
        curr_id++;                                  // 解析下一个频道
        syslog(LOG_ERR,"%s:这个目录中没有mp3文件或者glob函数出错.",path);
        free(me);
        return NULL;
    }
    // 初始化令牌桶
    me->tbf = mytbf_init(MP3_BITRATE/8,MP3_BITRATE/8*10);
    if(me->tbf == NULL)
    {
        syslog(LOG_ERR,"mytbf_init()令牌桶初始化失败:%s.",strerror(errno));
        free(me);
        return NULL;
    }
    me->pos = 0;                                    // 当前播放的是第一个
    me->offset = 0;                                 // 刚开始播放的歌曲的偏移量是0
    me->fd = open(me->mp3glob.gl_pathv[me->pos],O_RDONLY);
    if(me->fd < 0)
    {
        syslog(LOG_WARNING,"%s:无法打开mp3文件.",me->mp3glob.gl_pathv[me->pos]);
        free(me);
        return NULL;
    }
    me->chnid = curr_id;
    curr_id++;
    return me;
}