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

//As client connect to server
#define MAX_CLTDEF 500

#define TIMEOUT_CONNECT 5

#define TIME_RECONNECT 10


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
	const struct sockaddr_in stClientAddr;
	const time_t tCreateTime;
	const time_t tLastAccessTime;
	const SocketStat stat;
} SocketClientDef;

typedef struct {
	char sServerIp[20];
	int iServerPort;
	int iTcpClientId;
} ClientDef;

typedef struct {
	int iClientNum;
	int aiSocket[MAX_CLTDEF];
	ClientDef astClientDef[MAX_CLTDEF];
	SocketStat aStat[MAX_CLTDEF];
	time_t atLastConnectTime[MAX_CLTDEF];
} ClientDefMng;

typedef struct {
	int iSocket;
	struct sockaddr_in stClientAddr;
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
	//int (*HandleLoop) (SocketClientDef *pstScd, void *pUserInfo);
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

int AsyncNetFrameworkInit(void *pUserInfoBuf, int iUserInfoBufLen, int iUserInfoLen,
		LogFile *pstLog, int iLogLevel,
		ListenEntry *pstTcpListenEntrys, int iTcpNum, int iPkgHeadLenAsSrv, int iMaxAcceptSocketNum,
		ListenEntry *pstUdpListenEntrys, int iUdpNum,
		ClientDef *pstClientDefs, int iClientNum, int iPkgHeadLenAsClt,
		SrvCallBack *pstCallBack);

int AsyncNetFrameworkLoop();

int SendTcpPkg(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iPkgLen);

int SendUdpPkg(SocketClientDef *pstScd, const struct sockaddr_in *pstAddr, 
		void *pUserInfo, void *pPkg, int iPkgLen);

int GetContext(int iSocket, SocketClientDef **ppstScd, void **ppUserInfo);

void SetWaitTimeout(int iTimeoutMSec);

#ifdef __cplusplus
}
#endif 


#endif
