#include "async_net_framework.h"
#include "cm_process.h"
#include "cm_log.h"
#include "cm_debug.h"
#include "comm_struct.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static LogFile stLogFile;
#define LOG(fmt, args...) \
	do { \
		Log(&stLogFile, LOG_FORMAT_TYPE_TIME, "[%s][%d]:%s() "fmt, \
				__FILE__, __LINE__, __FUNCTION__, ## args);\
	}while(0) \

static int HandlePkgHeadServer(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iBytesRecved, int *piPkgLen)
{
	LOG("HandlePkgHeadServer");

	Pkg *stPkg = (Pkg *)pPkg;
	LOG("recv iBytesRecved %d head:stx %d ver %d cmd %d len %u", 
			iBytesRecved, stPkg->cStx, stPkg->stHead.cVer, stPkg->stHead.wCmd, stPkg->stHead.dwLength);

	//!!must return piPkgLen
	*piPkgLen = stPkg->stHead.dwLength;

	return 0;
}

static int HandlePkgServer(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iPkgLen)
{
	LOG("HandlePkgServer");

	LOG("iPkgLen %d", iPkgLen);
	//LOG("dump :\n%s", DumpPackage(pPkg, iPkgLen));

	int iRet = SendTcpPkg(pstScd, pUserInfo, pPkg, iPkgLen);
	LOG("SendTcpPkg iRet %d", iRet);

	return 0;
}

static int HandleAcceptServer(SocketClientDef *pstScd, void *pUserInfo)
{
	LOG("HandleAcceptServer");
	return 0;
}

static int HandleConnectServer(SocketClientDef *pstScd, void *pUserInfo)
{
	LOG("HandleConnectServer");
	return 0;
}

static int HandleLoopServer()
{
	LOG("HandleLoopServer");
	return 0;
}

//int (*HandleLoop) (SocketClientDef *pstScd, void *pUserInfo);
static int HandleUdpPkgServer(SocketClientDef *pstScd, void *pUserInfo, int iUdpName, void *pPkg, int iPkgLen)
{
	LOG("HandleUdpPkgServer");

	LOG("iPkgLen %d", iPkgLen);
	//LOG("dump :\n%s", DumpPackage(pPkg, iPkgLen));

	int iRet = 0;
	iRet = sendto(pstScd->iSocket, pPkg, iPkgLen, 0, (const struct sockaddr *)&pstScd->stClientAddr, sizeof(struct sockaddr));
	LOG("sendto %d", iRet);

	return 0;
}

static int HandleCloseServer(SocketClientDef *pstScd, void *pUserInfo)
{
	LOG("HandleCloseServer");
	return 0;
}

//TODO warning
static SrvCallBack stServerCallBack = {
	HandlePkgHead:HandlePkgHeadServer,
	HandlePkg:HandlePkgServer,
	HandleAccept:HandleAcceptServer,
	HandleConnect:HandleConnectServer,
	HandleLoop:HandleLoopServer,
	HandleUdpPkg:HandleUdpPkgServer,
	HandleClose:HandleCloseServer
};

#define TCP_ENTRY_NUM 1
#define UDP_ENTRY_NUM 1

int main(int argc, char *argv[])
{
	int i = 0, iRet = 0;

	//init log
	InitLogFile(&stLogFile, "/tmp/log/test_server", LOG_SHIFT_BY_SIZE, 5, 500000);
	LOG("main begin");

	//init tcp listen
	int iTcpNum = TCP_ENTRY_NUM;
	ListenEntry stTcpListenEntry[TCP_ENTRY_NUM];
	i = 0;
	/*
	strncpy(stTcpListenEntry[i].sIp, "192.168.1.108", sizeof(stTcpListenEntry[i].sIp));
	stTcpListenEntry[i].iPort = 8080;
	stTcpListenEntry[i].iName = i;
	i++;
	strncpy(stTcpListenEntry[i].sIp, "192.168.1.108", sizeof(stTcpListenEntry[i].sIp));
	stTcpListenEntry[i].iPort = 8081;
	stTcpListenEntry[i].iName = i;
	i++;
	*/
	strncpy(stTcpListenEntry[i].sIp, "127.0.0.1", sizeof(stTcpListenEntry[i].sIp));
	stTcpListenEntry[i].iPort = 8080;
	stTcpListenEntry[i].iName = i;
	i++;

	//init udp listen
	int iUdpNum = UDP_ENTRY_NUM;
	ListenEntry stUdpListenEntry[UDP_ENTRY_NUM];
	i = 0;
	/*
	strncpy(stUdpListenEntry[i].sIp, "192.168.1.108", sizeof(stUdpListenEntry[i].sIp));
	stUdpListenEntry[i].iPort = 8080;
	stUdpListenEntry[i].iName = i;
	i++;
	strncpy(stUdpListenEntry[i].sIp, "192.168.1.108", sizeof(stUdpListenEntry[i].sIp));
	stUdpListenEntry[i].iPort = 8081;
	stUdpListenEntry[i].iName = i;
	i++;
	*/
	strncpy(stUdpListenEntry[i].sIp, "127.0.0.1", sizeof(stUdpListenEntry[i].sIp));
	stUdpListenEntry[i].iPort = 8080;
	stUdpListenEntry[i].iName = i;
	i++;

	DaemonInit();

	/*
	int AsyncNetFrameworkInit(void *pUserInfoBuf, int iUserInfoBufLen, int iUserInfoLen,
			LogFile *pstLog, int iLogLevel,
			ListenEntry *pstTcpListenEntrys, int iTcpNum, int iPkgHeadLenAsSrv, int iMaxAcceptSocketNum,
			ListenEntry *pstUdpListenEntrys, int iUdpNum,
			ClientDef *pstClientDefs, int iClientNum, int iPkgHeadLenAsClt,
			SrvCallBack *pstCallBack);
	*/
	iRet = AsyncNetFrameworkInit(NULL, 0, 4, 
			&stLogFile, 2, 
			stTcpListenEntry, iTcpNum, sizeof(PkgHead), 500,
			//NULL, 0, 
			stUdpListenEntry, iUdpNum,
			NULL, 0, 0, 
			&stServerCallBack);

	AsyncNetFrameworkLoop();

	return 0;
}
