#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "async_net_framework.h"

#define MAX_TCP_PORT 10
#define MAX_UDP_PORT 10

typedef struct {
	LogFile *pstLog;
	int iLogLevel;

	int iTimeoutMSec; //ms 
	int iPkgHeadLenAsSrv;
	int iPkgHeadLenAsClt;
	int iUserInfoLen; //TODO
	void *aUserInfo; //TODO

	
	int iTcpNum;
	ListenEntry stTcpListenEntrys[MAX_TCP_PORT];
	//struct in_addr stTcpListenAddr;
	//unsigned short ushTcpPort[MAX_TCP_PORT];

	int iUdpNum;
	ListenEntry stUdpListenEntrys[MAX_UDP_PORT];
	//struct in_addr stUdpListenAddr;
	//unsigned short ushUdpPort[MAX_UDP_PORT];
	//int aiUdpName[MAX_UDP_PORT];
	int aiUdpSocket[MAX_UDP_PORT]; // index iUdpNum

	ClientDefMng stCltMng;

	AnfMng *pstAnfMng;
	
	int iMaxAcceptSocketNum; //just for accept
	int iCurAcceptSocketNum;

	int iTotalTcpNum; // iMaxAcceptSocketNum + stCltMng.iClientDefNum

	int iMaxFdNum; // iTotalTcpNum + 64

	SocketContext *astSocketContext;// index iSocket
	SrvCallBack stCallBack;
} SrvConfig;

static SrvConfig stSrvConfig;
SrvConfig *pstSrvConfig = &stSrvConfig;

#define LOG(fmt, args...) \
	do { \
		if (pstSrvConfig->pstLog) { \
			Log(pstSrvConfig->pstLog, LOG_FORMAT_TYPE_TIME, "%s:%d:%s "fmt, \
					__FILE__, __LINE__, __FUNCTION__, ## args);\
		} \
	}while(0) \

static int HandlePkgHeadDefault(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iBytesRecved, int *piPkgLen)
{
	LOG("HandlePkgHeadDefalut");
	return 0;
}

static int HandlePkgDefault(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iPkgLen)
{
	LOG("HandlePkgDefalut");
	return 0;
}

static int HandleAcceptDefault(SocketClientDef *pstScd, void *pUserInfo)
{
	LOG("HandleAcceptDefalut");
	return 0;
}

static int HandleConnectDefault(SocketClientDef *pstScd, void *pUserInfo)
{
	LOG("HandleConnectDefault");
	return 0;
}

static int HandleLoopDefault()
{
	LOG("HandleLoopDefault");
	return 0;
}

//int (*HandleLoop) (SocketClientDef *pstScd, void *pUserInfo);
static int HandleUdpPkgDefault(SocketClientDef *pstScd, void *pUserInfo, int iUdpName, void *pPkg, int iPkgLen)
{
	LOG("HandleUdpPkgDefault");
	return 0;
}

static int HandleCloseDefault(SocketClientDef *pstScd, void *pUserInfo, char *sErrInfo)
{
	LOG("HandleCloseDefault");
	return 0;
}

static void SetCallBack(SrvCallBack *pstDst, SrvCallBack *pstSrc)
{
	static SrvCallBack stDefaultCallBack = {
		HandlePkgHead:HandlePkgHeadDefault,
		HandlePkg:HandlePkgDefault,
		HandleAccept:HandleAcceptDefault,
		HandleConnect:HandleConnectDefault,
		HandleLoop:HandleLoopDefault,
		HandleUdpPkg:HandleUdpPkgDefault,
		HandleClose:HandleCloseDefault
	};

#define SET_CALLBACK(x) \
	do { \
		if (pstSrc->x) { \
			pstDst->x = pstSrc->x; \
		} else { \
			pstDst->x = stDefaultCallBack.x; \
		} \
	} while (0);

	memset(pstDst, 0, sizeof(*pstDst));
	SET_CALLBACK(HandlePkgHead);
	SET_CALLBACK(HandlePkg);
	SET_CALLBACK(HandleAccept);
	SET_CALLBACK(HandleConnect);
	SET_CALLBACK(HandleLoop);
	SET_CALLBACK(HandleUdpPkg);
	SET_CALLBACK(HandleClose);


	//justs for test
	LOG("try to call pstDst->HandleLoop");
	(pstDst->HandleLoop)();

#undef SET_CALLBACK
}

static void SetLimit(SrvConfig *pstSrvConfig)
{
	struct rlimit rlim;
	getrlimit(RLIMIT_NOFILE, &rlim);

	rlim.rlim_max = rlim.rlim_cur = pstSrvConfig->iMaxFdNum;
	if (setrlimit(RLIMIT_NOFILE, &rlim)) {
		perror("setlimit failed");
	}

	return;
}

int AsyncNetFrameworkInit (void *pUserInfoBuf, int iUserInfoBufLen, int iUserInfoLen,
		LogFile *pstLog, int iLogLevel,
		ListenEntry *pstTcpListenEntrys, int iTcpNum, int iPkgHeadLenAsSrv, int iMaxAcceptSocketNum,
		ListenEntry *pstUdpListenEntrys, int iUdpNum,
		ClientDef *pstClientDefs, int iClientNum, int iPkgHeadLenAsClt,
		SrvCallBack *pstCallBack)
{
	if (iTcpNum < 0 || iUdpNum < 0 || iClientNum < 0 || iMaxAcceptSocketNum < 0) {
		LOG("Error argument");
		return -1;
	}

	if (MAX_TCP_PORT < iTcpNum || MAX_UDP_PORT < iUdpNum) {
		LOG("iTcpNum or iUdpNum too max");
		return -4;
	}

	memset(pstSrvConfig, 0, sizeof(*pstSrvConfig));

	pstSrvConfig->iUdpNum = iUdpNum;
	pstSrvConfig->iTcpNum = iTcpNum;

	pstSrvConfig->pstLog = pstLog;
	pstSrvConfig->iLogLevel = iLogLevel;

	pstSrvConfig->iTimeoutMSec = 1000;

	pstSrvConfig->iMaxAcceptSocketNum = iMaxAcceptSocketNum;
	pstSrvConfig->iTotalTcpNum = iMaxAcceptSocketNum + iClientNum + iTcpNum;
	pstSrvConfig->iMaxFdNum = pstSrvConfig->iTotalTcpNum + iUdpNum + 64;
	pstSrvConfig->iPkgHeadLenAsSrv = iPkgHeadLenAsSrv;
	pstSrvConfig->iPkgHeadLenAsClt = iPkgHeadLenAsClt;
	pstSrvConfig->iUserInfoLen = iUserInfoLen;

	//TODO
	if ((iMaxAcceptSocketNum && iPkgHeadLenAsSrv <= 0)
			|| (iClientNum && iPkgHeadLenAsClt <= 0)) {
		LOG("iPkgHeadLenAsSrv iPkgHeadLenAsClt error");
		return -2;
	}

	if (!pUserInfoBuf) {
		pstSrvConfig->aUserInfo = malloc(iUserInfoLen * pstSrvConfig->iMaxFdNum);
		if (!pstSrvConfig->aUserInfo) {
			perror("malloc aUserInfo failed");
			return -5;
		}
	} else {
		if (iUserInfoBufLen < iUserInfoLen * pstSrvConfig->iMaxFdNum) {
			return -7;
		}

		pstSrvConfig->aUserInfo = pUserInfoBuf;
	}

	int i = 0;
	for (i = 0; i < iTcpNum; i++) {
		pstSrvConfig->stTcpListenEntrys[i] = pstTcpListenEntrys[i];
		//TODO
		LOG("ip %s port %d id %d", pstSrvConfig->stTcpListenEntrys[i].sIp, 
				pstSrvConfig->stTcpListenEntrys[i].iPort, pstSrvConfig->stTcpListenEntrys[i].iName);
	}

	for (i = 0; i < iUdpNum; i++) {
		pstSrvConfig->stUdpListenEntrys[i] = pstUdpListenEntrys[i];
	}

	memset(&(pstSrvConfig->stCltMng), 0, sizeof(ClientDefMng));
	pstSrvConfig->stCltMng.iClientNum = iClientNum;
	for (i = 0; i < iClientNum; i++) {
		if (pstClientDefs[i].iTcpClientId > MAX_CLTDEF || pstClientDefs[i].iTcpClientId < 0) {
			LOG("ip %s port %d iTcpClientId is out of [0, %d] ", pstClientDefs[i].sServerIp, pstClientDefs[i].iServerPort, MAX_CLTDEF);
			return -9;
		}
		memcpy(&((pstSrvConfig->stCltMng.astClientDef)[pstClientDefs[i].iTcpClientId]), 
					&(pstClientDefs[i]), sizeof(ClientDef));
	}

	SetCallBack(&(pstSrvConfig->stCallBack), pstCallBack);

	SetLimit(pstSrvConfig);

	return 0;
}

int InitListenSocket()
{
	int i = 0, iSocket, iRet = 0;
	SocketConext *pContext = NULL;

	//init tcp listen socket
	for (i = 0; i < pstSrvConfig->iTcpNum; i++) {

		iRet = CreateTcpSocketEx(&iSocket, pstSrvConfig->stTcpListenEntrys[i].sIp,
				pstSrvConfig->stTcpListenEntrys[i].iPort, NO_BLOCK);

		if (!iRet || iSocket <= 0) {
			LOG("CreateTcpSocketEx failed iRet(%d) ip %s port %d id %d", iRet, pstSrvConfig->stTcpListenEntrys[i].sIp, 
					pstSrvConfig->stTcpListenEntrys[i].iPort, pstSrvConfig->stTcpListenEntrys[i].iName);
			return -11;
		}

		pContext = &((pstSrvConfig->astSocketContext)[iSocket]);
		pContext->stat = SOCKET_TCP_LISTEN;
		pContext->iSocket = iSocket;
		pContext->tCreateTime = pContext->tLastAccessTime = time(NULL);

		if ((iRet = AnfAddFd(pstSrvConfig->pstAnfMng, iSocket, ANF_FLAG_READ | ANF_FLAG_ERROR)) < 0) {
			LOG("AnfAddFd failed iRet(%d) ip %s port %d id %d iSock %d", iRet, pstSrvConfig->stTcpListenEntrys[i].sIp, 
					pstSrvConfig->stTcpListenEntrys[i].iPort, pstSrvConfig->stTcpListenEntrys[i].iName, iSocket);
			return -13;
		}
	}

	//init udp listen socket
	for (i = 0; i < pstSrvConfig->iUdpNum; i++) {

		iRet = CreateUdpSocketEx(&iSocket, pstSrvConfig->stUdpListenEntrys[i].sIp,
				pstSrvConfig->stUdpListenEntrys[i].iPort, NO_BLOCK);

		if (!iRet || iSocket <= 0) {
			LOG("CreateUdpSocketEx failed iRet(%d) ip %s port %d id %d", iRet, pstSrvConfig->stUdpListenEntrys[i].sIp, 
					pstSrvConfig->stUdpListenEntrys[i].iPort, pstSrvConfig->stUdpListenEntrys[i].iName);
			return -11;
		}

		pstSrvConfig->aiUdpSocket[i] = iSocket
		pContext = &((pstSrvConfig->astSocketContext)[iSocket]);
		pContext->stat = SOCKET_UDP;
		pContext->iSocket = iSocket;
		pContext->tCreateTime = pContext->tLastAccessTime = time(NULL);

		if ((iRet = AnfAddFd(pstSrvConfig->pstAnfMng, iSocket, ANF_FLAG_READ | ANF_FLAG_ERROR)) < 0) {
			LOG("AnfAddFd failed iRet(%d) ip %s port %d id %d iSock %d", iRet, pstSrvConfig->stUdpListenEntrys[i].sIp, 
					pstSrvConfig->stUdpListenEntrys[i].iPort, pstSrvConfig->stUdpListenEntrys[i].iName, iSocket);
			return -13;
		}
	}

	return 0;
}

int InitClientSocket()
{
	return 0;
}

int AsyncNetFrameworkLoop()
{
	int i = 0, iRet = 0;

	pstSrvConfig->astSocketContext = malloc(sizeof(*(pstSrvConfig->astSocketContext)) * pstSrvConfig->iMaxFdNum);
	if (!(pstSrvConfig->astSocketContext)) {
		perror("astSocketContext malloc failed");
		return -1;
	}

	for (i = 0; i < pstSrvConfig.iMaxFdNum; i++) {
		pstSrvConfig->astSocketContext[i].stat = SOCKET_UNUSED;
	}

	pstSrvConfig->pstAnfMng = AnfInit(pstSrvConfig->pstLog, pstSrvConfig->iLogLevel, pstSrvConfig->iMaxFdNum);
	if (!pstSrvConfig->pstAnfMng) {
		LOG("AnfInit failed");
		return -3;
	}

	if ((iRet = InitListenSocket()) < 0) {
		LOG("InitListenSocket failed iRet %d", iRet);
		return -5;
	}


	if ((iRet = InitClientSocket()) < 0) {
		LOG("InitClientSocket failed iRet %d", iRet);
		return -7;
	}

	pstSrvConfig->iCurAcceptSocketNum = 0;

	while(true) {
		//do asyc io
	}

	return 0;
}
