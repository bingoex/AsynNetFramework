#ifndef _ASYNC_NET_FRAMEWORK_H_
#define _ASYNC_NET_FRAMEWORK_H_



#include "async_base.h"
#include "cm_net.h"


#ifdef __SERVER__
#define RECVBUF_LEN 1024 * 1024 * 2
#define SENDBUF_LEN 1024 * 1024 * 2
#else
#define RECVBUF_LEN 1024 * 1024 * 2
#define SENDBUF_LEN 1024 * 1024 * 2
#endif

/* max num of as client connect to server */
#define MAX_CLTDEF 500

typedef enum {
	SOCKET_UNUSED,
	SOCKET_TCP_ACCEPT,
	SOCKET_TCP_LISTEN,
	SOCKET_TCP_CONNECTING,
	SOCKET_TCP_CONNECTED,
	SOCKET_TCP_RECONNECT_WAIT,
	SOCKET_UDP
} SocketStat;

typedef struct {
	const int iSocket;
	//const struct sockaddr_in stClientAddr;
	const struct sockaddr stClientAddr;
	const time_t tCreateTime;
	const time_t tLastAccessTime;
	const SocketStat stat;
} SocketClientDef;


typedef struct {
	char sServerIp[20];
	int iServerPort;
	int iTcpClientId;
	int iProtoType;
} ClientDef;

/* 客户端结构体 */
typedef struct {
	int iClientNum;
	int aiSocket[MAX_CLTDEF];
	ClientDef astClientDef[MAX_CLTDEF];
	SocketStat aStat[MAX_CLTDEF];
	time_t atLastConnectTime[MAX_CLTDEF];
} ClientDefMng;

/* 监听结构体 */
typedef struct {
	int iSocket;
	//struct sockaddr_in stClientAddr;
	struct sockaddr stClientAddr;
	time_t tCreateTime;
	time_t tLastAccessTime;
	SocketStat stat;
	int iBytesRecved;
	int iBytesSend;
	int iPkgLen;
	char RecvBuf[RECVBUF_LEN];
	char SendBuf[SENDBUF_LEN];
	int iClientIndex; // if context is client, index astClientDef
} SocketContext;



typedef struct {
	int (*HandlePkgHead) (SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iBytesRecved, int *piPkgLen);
	int (*HandlePkg) (SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iPkgLen);
	int (*HandleAccept) (SocketClientDef *pstScd, void *pUserInfo);
	int (*HandleConnect) (SocketClientDef *pstScd, void *pUserInfo);
	int (*HandleLoop) ();
	int (*HandleUdpPkg) (SocketClientDef *pstScd, void *pUserInfo, int iUdpName, void *pPkg, int iPkgLen);
	int (*HandleClose) (SocketClientDef *pstScd, void *pUserInfo);
} SrvCallBack;


typedef struct {
	char sIp[20];
	int iPort;
	int iName;
}ListenEntry;

#ifdef __cplusplus
extern "C" {
#endif 

/*
 * 初始化框架
 *      1、参数检查
 *      2、初始化监听结构体
 *      3、初始化客户端结构体
 *      4、初始化回调函数
 *      5、初始化资源大小
 * 参数说明：
 *      pUserInfoBuf：用户session存储地址，如果为NULL则框架负责new (UserInfo just for server)
 *      iUserInfoBufLen：用户session总大小
 *      iUserInfoLen：单用户session大小
 *
 *      pstLog：框架日志结构
 *      iLogLevel：框架日志级别
 *
 *      pstTcpListenEntrys：tcp监听结构体(ip\端口区分)
 *      iTcpNum：tcp监听个数
 *      iPkgHeadLenAsSrv：每个请求包的包头大小（目前框架只支持固定大小包头）
 *      iMaxAcceptSocketNum：最大Accept请求数
 *
 *      pstUdpListenEntrys：udp监听结构体
 *      iUdpNum：udp监听个数
 *
 *      pstClientDefs：框架作为客户端发包结构体(目前只支持tcp，udp可直接发包)
 *      iClientNum：作为client的个数
 *      iPkgHeadLenAsClt：发包包头长度
 *
 *      pstCallBack：回调函数
 */
int AsyncNetFrameworkInit(void *pUserInfoBuf, int iUserInfoBufLen, int iUserInfoLen,
		LogFile *pstLog, int iLogLevel,
		ListenEntry *pstTcpListenEntrys, int iTcpNum, int iPkgHeadLenAsSrv, int iMaxAcceptSocketNum,
		ListenEntry *pstUdpListenEntrys, int iUdpNum,
		ClientDef *pstClientDefs, int iClientNum, int iPkgHeadLenAsClt,
		SrvCallBack *pstCallBack);

/*
 * 启动框架
 *      1、初始化AnfInit结构
 *      2、监听端口，并加入多路复用
 *      3、初始化客户端发包结构，并加入多路复用
 *      4、wait收发包多路复用fd
 *      5、调用相应回调函数
 */
int AsyncNetFrameworkLoop();

/*
 * 在收包处理函数中调用，回复客户端内容
 */
int SendTcpPkg(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iPkgLen);

// TODO
int SendUdpPkg(SocketClientDef *pstScd, ListenEntry *pstListenEntry, 
		void *pUserInfo, void *pPkg, int iPkgLen);

/*
 * 获取用户session，紧支持服务器端
 */
int GetContext(int iSocket, SocketClientDef **ppstScd, void **ppUserInfo);

/*
 * 设置超时时间
 */
void SetWaitTimeout(int iTimeoutMSec);

#ifdef __cplusplus
}
#endif 



#endif
