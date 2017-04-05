#ifndef _ASYNC_BASE_H_
#define _ASYNC_BASE_H_



#include <net/if.h> // time
#include <arpa/inet.h> //sockaddr_in
#include <unistd.h> // close
#include <stdio.h> // FILE

#include "cm_log.h"

#ifdef __cplusplus
extern "C" {
#endif 

struct _AnfMng;
typedef struct _AnfMng AnfMng;


//iFlag参数值
#define ANF_FLAG_READ 0x1
#define ANF_FLAG_WRITE 0x2
#define ANF_FLAG_ERROR 0x4


#define EPOLLIN 0x1
#define EPOLLOUT 0x2
#define EPOLLHUP 0x4 
#define EPOLLERR 0x8

/*
 * 初始化异步io管理器
 * 参数说明：
 *      pstLog：日志结构
 *      iLogLevel：日志级别
 *      iMaxSocketNum：最大操作fd数
 */
AnfMng *AnfInit(LogFile *pstLog, int iLogLevel, int iMaxSocketNum);

/*
 * fd事件操作
 */
int AnfAddFd(AnfMng *pstAnfMng, int iFd, int iFlag);
int AnfDelFd(AnfMng *pstAnfMng, int iFd);
int AnfModFd(AnfMng *pstAnfMng, int iFd, int iFlag);

/*
 * 等待事件
 * 参数说明：
 *   iTimeoutMSec：超时时间（单位毫秒）
 * 返回值：
 *   负数：失败
 *   正数：发生事件的fd个数
 */
int AnfWaitForFd(AnfMng *pstAnfMng, int iTimeoutMSec);

/*
 * 调用完AnfWaitForFd后处理所有发生的事件
 * 参数说明：
 *      piPos：当前处理的fd位置，此值会被函数＋＋修改
 *      piFlag：当前fd的事件
 *  返回值：
 *      负数：失败
 *      正数：当前处理的含有事件fd
 */
int AnfGetReadyFd(AnfMng *pstAnfMng, int *piPos, int *piFlag);

#ifdef __cplusplus
}
#endif 



#endif
