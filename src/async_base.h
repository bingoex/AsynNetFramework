#ifndef _ASYNC_BASE_H_
#define _ASYNC_BASE_H_

#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <net/ethernet.h>
#include <net/if.h>       /* for ifconf */  
#include <netinet/in.h>   /* for sockaddr_in */  
#include <sys/socket.h>  
#include <sys/types.h>  
#include <sys/ioctl.h>  
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "cm_log.h"

#ifdef __cplusplus
extern "C" {
#endif 

struct _AnfMng;
typedef struct _AnfMng AnfMng;

#define ANF_FLAG_READ 0x1
#define ANF_FLAG_WRITE 0x2
#define ANF_FLAG_ERROR 0x4


#define EPOLLIN 0x1
#define EPOLLOUT 0x2
#define EPOLLHUP 0x4 
#define EPOLLERR 0x8

AnfMng *AnfInit(LogFile *pstLog, int iLogLevel, int iMaxSocketNum);

int AnfAddFd(AnfMng *pstAnfMng, int iFd, int iFlag);
int AnfDelFd(AnfMng *pstAnfMng, int iFd);
int AnfModFd(AnfMng *pstAnfMng, int iFd, int iFlag);

int AnfWaitForFd(AnfMng *pstAnfMng, int iTimeoutMSec);
int AnfGetReadyFd(AnfMng *pstAnfMng, int *piPos, int *piFlag);

#ifdef __cplusplus
}
#endif 


#endif
