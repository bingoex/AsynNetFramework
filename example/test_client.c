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

static int HandlePkgHeadClient(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iBytesRecved, int *piPkgLen)
{
	LOG("HandlePkgHeadClient");

	Pkg *stPkg = (Pkg *)pPkg;
	LOG("recv iBytesRecved %d head stx %d ver %d cmd %d len %u", 
			iBytesRecved, stPkg->cStx, stPkg->stHead.cVer, stPkg->stHead.wCmd, stPkg->stHead.dwLength);
	
	*piPkgLen = stPkg->stHead.dwLength;

	return 0;
}

static int HandlePkgClient(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iPkgLen)
{
	LOG("HandlePkgClient");

	LOG("iPkgLen %d", iPkgLen);
	//LOG("dump :\n%s", DumpPackage(pPkg, iPkgLen));

	return 0;
}

static int HandleAcceptClient(SocketClientDef *pstScd, void *pUserInfo)
{
	LOG("HandleAcceptClient");
	return 0;
}

static int HandleConnectClient(SocketClientDef *pstScd, void *pUserInfo)
{
	//sent pkg
	LOG("HandleConnectClient");
	static Pkg stPkg;
	int iRet = 0;

	memset(&stPkg, 0, sizeof(Pkg));
	stPkg.cStx = 0x1;
	stPkg.stHead.cVer = 1;
	stPkg.stHead.wCmd = 0xef;
	stPkg.stHead.dwLength = sizeof(Pkg);
	//stPkg.stHead.dwLength = 1 +sizeof(PkgHead) + 1;
	//stPkg.stHead.dwLength = 1048577;
	//stPkg.sBody[0] = 0x2;
	stPkg.cEtx = 0x2;

	iRet = SendTcpPkg(pstScd, pUserInfo, &stPkg, stPkg.stHead.dwLength);

	//LOG("SendTcpPkg iRet %d dumpPkg:\n%s", iRet, DumpPackage(&stPkg, stPkg.stHead.dwLength));
	LOG("SendTcpPkg iRet %d ", iRet);
	
	return 0;
}

static int HandleLoopClient()
{
	LOG("HandleLoopClient");
	return 0;
}

//int (*HandleLoop) (SocketClientDef *pstScd, void *pUserInfo);
static int HandleUdpPkgClient(SocketClientDef *pstScd, void *pUserInfo, int iUdpName, void *pPkg, int iPkgLen)
{
	LOG("HandleUdpPkgClient");
	return 0;
}

static int HandleCloseClient(SocketClientDef *pstScd, void *pUserInfo)
{
	LOG("HandleCloseClient");
	return 0;
}

static SrvCallBack stClientCallBack = {
	HandlePkgHead:HandlePkgHeadClient,
	HandlePkg:HandlePkgClient,
	HandleAccept:HandleAcceptClient,
	HandleConnect:HandleConnectClient,
	HandleLoop:HandleLoopClient,
	HandleUdpPkg:HandleUdpPkgClient,
	HandleClose:HandleCloseClient
};

#define CLIENT_NUM 3
int main(int argc, char *argv[])
{
	int i = 0, iRet = 0;

	//init log
	InitLogFile(&stLogFile, "/tmp/log/test_client", LOG_SHIFT_BY_SIZE, 5, 500000);
	LOG("main begin");

	i = 0;
	ClientDef stClientDef[CLIENT_NUM];
	strncpy(stClientDef[i].sServerIp, "127.0.0.1", sizeof(stClientDef[i].sServerIp));
	stClientDef[i].iServerPort = 8080;
	stClientDef[i].iTcpClientId = i;
	i++;



	//DaemonInit();

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
			NULL, 0, 0, 0,
			NULL, 0, 
			stClientDef, i, sizeof(PkgHead), 
			&stClientCallBack);

	AsyncNetFrameworkLoop();

	return 0;
}
