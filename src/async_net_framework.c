#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include "async_net_framework.h"

// 最多监听端口数
#define MAX_TCP_PORT 10
#define MAX_UDP_PORT 10

typedef struct {
	LogFile *pstLog;
	int iLogLevel;

	int iTimeoutMSec; //ms 
	int iPkgHeadLenAsSrv; // 服务器收包包头长度
	int iPkgHeadLenAsClt;

    /* 用户session */
	int iUserInfoLen; //TODO 暂未用到
	void *aUserInfo; //TODO

	
    /* tcp监听相关 */
	int iTcpNum;
	ListenEntry stTcpListenEntrys[MAX_TCP_PORT];
	//struct in_addr stTcpListenAddr;
	//unsigned short ushTcpPort[MAX_TCP_PORT];

    /* udp监听相关 */
	int iUdpNum;
	ListenEntry stUdpListenEntrys[MAX_UDP_PORT];
	//struct in_addr stUdpListenAddr;
	//unsigned short ushUdpPort[MAX_UDP_PORT];
	//int aiUdpName[MAX_UDP_PORT];
	int aiUdpSocket[MAX_UDP_PORT]; // index iUdpNum

    /* 以客户端身份发包相关*/
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
			Log(pstSrvConfig->pstLog, LOG_FORMAT_TYPE_TIME, "[%s][%d]%s() "fmt, \
					__FILE__, __LINE__, __FUNCTION__, ## args);\
		} \
	}while(0) \

#define LOGTRACE(fmt, args...) \
	do { \
		if (0) { \
			Log(pstSrvConfig->pstLog, LOG_FORMAT_TYPE_TIME, "[%s][%d]%s() "fmt, \
					__FILE__, __LINE__, __FUNCTION__, ## args);\
		} \
	}while(0) \

/* 默认回调函数实现 */
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

static int HandleCloseDefault(SocketClientDef *pstScd, void *pUserInfo)
{
	LOG("HandleCloseDefault");
	return 0;
}

static void SetCallBack(SrvCallBack *pstDst, SrvCallBack *pstSrc)
{
	//TODO warning
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

#undef SET_CALLBACK
}

static void SetLimit(SrvConfig *pstSrvConfig)
{
	struct rlimit rlim;
	getrlimit(RLIMIT_NOFILE, &rlim);

	//TODO
	LOGTRACE("SetLimit rlim_max %d rlim_cur %d iMaxFdNum%d ",
			rlim.rlim_max, rlim.rlim_cur, pstSrvConfig->iMaxFdNum);

	rlim.rlim_max = rlim.rlim_cur = pstSrvConfig->iMaxFdNum;
	if (setrlimit(RLIMIT_NOFILE, &rlim)) {
		perror("setlimit failed");
	}

	//TODO
	LOGTRACE("SetLimit success");

	return;
}

/*
 * 初始化框架
 *      1、参数检查
 *      2、初始化监听结构体
 *      3、初始化客户端结构体
 *      4、初始化回调函数
 *      5、初始化资源大小
 * 参数说明：
 *      pUserInfoBuf：用户session存储地址，如果为NULL则框架负责new
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
 *      pstClientDefs：框架作为客户端发包结构体
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
		SrvCallBack *pstCallBack)
{
	if (iTcpNum < 0 || iUdpNum < 0 || iClientNum < 0 || iMaxAcceptSocketNum < 0) {
		LOG("Error argument");
		return -1;
	}

	if (MAX_TCP_PORT < iTcpNum || MAX_UDP_PORT < iUdpNum) {
		LOG("iTcpNum or iUdpNum too big");
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
	LOGTRACE("init args iUdpNum %d iTcpNum %d iLogLevel %d iTimeoutMSec %d iMaxAcceptSocketNum %d \n"
			"iTotalTcpNum %d iMaxFdNum %d iPkgHeadLenAsSrv %d iPkgHeadLenAsClt %d iUserInfoLen %d",
			pstSrvConfig->iUdpNum, pstSrvConfig->iTcpNum, pstSrvConfig->iLogLevel, pstSrvConfig->iTimeoutMSec, pstSrvConfig->iMaxAcceptSocketNum,
			pstSrvConfig->iTotalTcpNum, pstSrvConfig->iMaxFdNum, pstSrvConfig->iPkgHeadLenAsSrv, pstSrvConfig->iPkgHeadLenAsClt, 
			pstSrvConfig->iUserInfoLen);

	//TODO should use a callback to get the min head length
	if ((iMaxAcceptSocketNum && iPkgHeadLenAsSrv <= 0)
			|| (iClientNum && iPkgHeadLenAsClt <= 0)) {
		LOG("iPkgHeadLenAsSrv iPkgHeadLenAsClt error");
		return -2;
	}

	if (!pUserInfoBuf) {
		pstSrvConfig->aUserInfo = malloc(iUserInfoLen * pstSrvConfig->iMaxFdNum);

		//TODO
		LOGTRACE("Userinfo aUserInfo %p iUserInfoLen %d  * iMaxFdNum %d  = %d", pstSrvConfig->aUserInfo,
				iUserInfoLen, pstSrvConfig->iMaxFdNum, pstSrvConfig->iMaxFdNum * iUserInfoLen);

		if (!pstSrvConfig->aUserInfo) {
			perror("malloc aUserInfo failed");
			return -5;
		}
	} else {
		if (iUserInfoBufLen < iUserInfoLen * pstSrvConfig->iMaxFdNum) {
			return -7;
		}

		pstSrvConfig->aUserInfo = pUserInfoBuf;

		//TODO
		LOGTRACE("UserInfo aUserInfo %p iUserInfoLen %d  * iMaxFdNum %d  = %d", pstSrvConfig->aUserInfo,
				iUserInfoLen, pstSrvConfig->iMaxFdNum, pstSrvConfig->iMaxFdNum * iUserInfoLen);
	}

	int i = 0;
	for (i = 0; i < iTcpNum; i++) {
		strncpy(pstSrvConfig->stTcpListenEntrys[i].sIp, pstUdpListenEntrys[i].sIp, sizeof(pstUdpListenEntrys[i].sIp));
		pstSrvConfig->stTcpListenEntrys[i].iPort = pstUdpListenEntrys[i].iPort;
		pstSrvConfig->stTcpListenEntrys[i].iName = pstUdpListenEntrys[i].iName;

		//TODO
		LOGTRACE("TCP: ip %s port %d id %d", pstSrvConfig->stTcpListenEntrys[i].sIp, 
				pstSrvConfig->stTcpListenEntrys[i].iPort, pstSrvConfig->stTcpListenEntrys[i].iName);
	}

	for (i = 0; i < iUdpNum; i++) {
		strncpy(pstSrvConfig->stUdpListenEntrys[i].sIp, pstUdpListenEntrys[i].sIp, sizeof(pstUdpListenEntrys[i].sIp));
		pstSrvConfig->stUdpListenEntrys[i].iPort = pstUdpListenEntrys[i].iPort;
		pstSrvConfig->stUdpListenEntrys[i].iName = pstUdpListenEntrys[i].iName;

		//pstSrvConfig->stUdpListenEntrys[i] = pstUdpListenEntrys[i];
		//TODO
		LOGTRACE("UDP ip %s port %d id %d", pstSrvConfig->stUdpListenEntrys[i].sIp, 
				pstSrvConfig->stUdpListenEntrys[i].iPort, pstSrvConfig->stUdpListenEntrys[i].iName);
	}

	memset(&(pstSrvConfig->stCltMng), 0, sizeof(ClientDefMng));
	pstSrvConfig->stCltMng.iClientNum = iClientNum;

	//TODO
	LOGTRACE("iClientNum %d", pstSrvConfig->stCltMng.iClientNum);

	for (i = 0; i < iClientNum; i++) {
		if (pstClientDefs[i].iTcpClientId > MAX_CLTDEF || pstClientDefs[i].iTcpClientId < 0) {
			LOG("ip %s port %d iTcpClientId(%d) is out of [0, %d] ", 
					pstClientDefs[i].sServerIp, pstClientDefs[i].iServerPort, pstClientDefs[i].iTcpClientId, MAX_CLTDEF);
			return -9;
		}
		memcpy(&((pstSrvConfig->stCltMng.astClientDef)[pstClientDefs[i].iTcpClientId]), 
					&(pstClientDefs[i]), sizeof(ClientDef));
		//TODO
		LOGTRACE("Client ip %s port %d iTcpClientId %d", 
				pstClientDefs[i].sServerIp, pstClientDefs[i].iServerPort, pstClientDefs[i].iTcpClientId);
	}

    /* 设置回调函数 */
	SetCallBack(&(pstSrvConfig->stCallBack), pstCallBack);

	SetLimit(pstSrvConfig);

	//TODO
	LOGTRACE("AsyncNetFrameworkInit success!!!");

	return 0;
}

static int InitListenSocket()
{
	//TODO
	LOGTRACE("try to InitListenSocket");

	int i = 0, iSocket, iRet = 0;
	SocketContext *pContext = NULL;

	//init tcp listen socket
	for (i = 0; i < pstSrvConfig->iTcpNum; i++) {

		if (strlen(pstSrvConfig->stTcpListenEntrys[i].sIp) < 7 || pstSrvConfig->stTcpListenEntrys[i].iPort <= 0)
			return -15;

		iRet = CreateTcpSocketEx(&iSocket, pstSrvConfig->stTcpListenEntrys[i].sIp,
				pstSrvConfig->stTcpListenEntrys[i].iPort, NO_BLOCK);

		if (iRet || iSocket <= 0) {
			LOG("CreateTcpSocketEx failed iRet(%d) ip %s port %d id %d", iRet, pstSrvConfig->stTcpListenEntrys[i].sIp, 
					pstSrvConfig->stTcpListenEntrys[i].iPort, pstSrvConfig->stTcpListenEntrys[i].iName);
			return -11;
		}

		//TODO
		LOGTRACE("CreateTcpSocketEx success iRet(%d) ip %s port %d id %d", 
				iRet, pstSrvConfig->stTcpListenEntrys[i].sIp, 
				pstSrvConfig->stTcpListenEntrys[i].iPort, pstSrvConfig->stTcpListenEntrys[i].iName);

		pContext = &((pstSrvConfig->astSocketContext)[iSocket]);
		pContext->stat = SOCKET_TCP_LISTEN;
		pContext->iSocket = iSocket;
		pContext->tCreateTime = pContext->tLastAccessTime = time(NULL);

		if ((iRet = AnfAddFd(pstSrvConfig->pstAnfMng, iSocket, ANF_FLAG_READ | ANF_FLAG_ERROR)) < 0) {
			LOG("AnfAddFd failed iRet(%d) ip %s port %d id %d iSock %d", iRet, pstSrvConfig->stTcpListenEntrys[i].sIp, 
					pstSrvConfig->stTcpListenEntrys[i].iPort, pstSrvConfig->stTcpListenEntrys[i].iName, iSocket);
			return -13;
		}

		//TODO
		LOGTRACE("AnfAddFd success iRet(%d) ip %s port %d id %d iSock %d", 
				iRet, pstSrvConfig->stTcpListenEntrys[i].sIp, pstSrvConfig->stTcpListenEntrys[i].iPort, 
				pstSrvConfig->stTcpListenEntrys[i].iName, iSocket);
	}

	//init udp listen socket
	for (i = 0; i < pstSrvConfig->iUdpNum; i++) {

		if (strlen(pstSrvConfig->stUdpListenEntrys[i].sIp) < 7 || pstSrvConfig->stUdpListenEntrys[i].iPort <= 0)
			return -17;

		iRet = CreateUdpSocketEx(&iSocket, pstSrvConfig->stUdpListenEntrys[i].sIp,
				pstSrvConfig->stUdpListenEntrys[i].iPort, NO_BLOCK);

		if (iRet || iSocket <= 0) {
			LOG("CreateUdpSocketEx failed iRet(%d) ip %s port %d id %d", iRet, pstSrvConfig->stUdpListenEntrys[i].sIp, 
					pstSrvConfig->stUdpListenEntrys[i].iPort, pstSrvConfig->stUdpListenEntrys[i].iName);
			return -18;
		}

		//TODO
		LOGTRACE("CreateUdpSocketEx success iRet(%d) ip %s port %d id %d", 
				iRet, pstSrvConfig->stUdpListenEntrys[i].sIp, 
				pstSrvConfig->stUdpListenEntrys[i].iPort, pstSrvConfig->stUdpListenEntrys[i].iName);

		pstSrvConfig->aiUdpSocket[i] = iSocket;
		pContext = &((pstSrvConfig->astSocketContext)[iSocket]);
		pContext->stat = SOCKET_UDP;
		pContext->iSocket = iSocket;
		pContext->tCreateTime = pContext->tLastAccessTime = time(NULL);

		if ((iRet = AnfAddFd(pstSrvConfig->pstAnfMng, iSocket, ANF_FLAG_READ | ANF_FLAG_ERROR)) < 0) {
			LOG("AnfAddFd failed iRet(%d) ip %s port %d id %d iSock %d", iRet, pstSrvConfig->stUdpListenEntrys[i].sIp, 
					pstSrvConfig->stUdpListenEntrys[i].iPort, pstSrvConfig->stUdpListenEntrys[i].iName, iSocket);
			return -19;
		}

		//TODO
		LOGTRACE("AnfAddFd success iRet(%d) ip %s port %d id %d iSock %d", 
				iRet, pstSrvConfig->stUdpListenEntrys[i].sIp, 
				pstSrvConfig->stUdpListenEntrys[i].iPort, pstSrvConfig->stUdpListenEntrys[i].iName, iSocket);
	}
	
	//TODO
	LOGTRACE("InitListenSocket success");

	return 0;
}

static int InitClientSocket()
{
	//TODO
	LOGTRACE("try to InitClientSocket");

	int i = 0, iSocket, iRet = 0;
	SocketContext *pContext = NULL;

	//init tcp listen socket
	for (i = 0; i < pstSrvConfig->stCltMng.iClientNum; i++) {
		ClientDef *pstClienDef = &(pstSrvConfig->stCltMng.astClientDef[i]);

		if (!pstClienDef || strlen(pstClienDef->sServerIp) < 7 || pstClienDef->iServerPort <= 0)
			return -21;

		iRet = CreateTcpClientSocketEx(&iSocket, pstClienDef->sServerIp, pstClienDef->iServerPort, NO_BLOCK);

		if (iRet || iSocket <= 0 ||iSocket > pstSrvConfig->iMaxFdNum) {
			LOG("CreateTcpClientSocketEx failed iRet(%d) ip %s port %d id %d socket %d", iRet, pstClienDef->sServerIp,
					pstClienDef->iServerPort, pstClienDef->iTcpClientId, iSocket);
			return -23;
		}

		//TODO
		LOGTRACE("CreateTcpClientSocketEx success iRet(%d) ip %s port %d id %d socket %d", 
				iRet, pstClienDef->sServerIp, pstClienDef->iServerPort, pstClienDef->iTcpClientId, iSocket);

		pContext = &((pstSrvConfig->astSocketContext)[iSocket]);
		pContext->stat = SOCKET_TCP_CONNECTING;
		pContext->iSocket = iSocket;
		pContext->tCreateTime = pContext->tLastAccessTime = time(NULL);
		pContext->iClientIndex = i;// index astClientDef

		pstSrvConfig->stCltMng.aiSocket[i] =iSocket;
		pstSrvConfig->stCltMng.aStat[i] = pContext->stat;
		pstSrvConfig->stCltMng.atLastConnectTime[i] = time(NULL);

		if ((iRet = AnfAddFd(pstSrvConfig->pstAnfMng, iSocket, ANF_FLAG_WRITE | ANF_FLAG_ERROR)) < 0) {
			LOG("AnfAddFd failed iRet(%d) ip %s port %d id %d iSock %d", iRet,
					pstClienDef->sServerIp, pstClienDef->iServerPort, pstClienDef->iTcpClientId, iSocket);
			return -25;
		}

		//TODO
		LOGTRACE("AnfAddFd success iRet(%d) ip %s port %d id %d iSock %d write %d", 
				iRet, pstClienDef->sServerIp, pstClienDef->iServerPort, pstClienDef->iTcpClientId, iSocket);

	}

	//TODO
	LOGTRACE("InitClientSocket success");

	return 0;
}

static int ProcessClose(SocketContext *pContext, void *pUserInfo)
{
	SrvCallBack *pstCallback = &(pstSrvConfig->stCallBack);
	int iRet = 0;

	if (pContext == NULL) {
		LOG("pContext == NULL");
		return -51;
	}

	if (pContext->stat == SOCKET_UNUSED
			|| (pContext->stat != SOCKET_TCP_ACCEPT &&
				pContext->stat != SOCKET_TCP_CONNECTING &&
				pContext->stat != SOCKET_TCP_CONNECTED)){
		LOG("BUG error stat %d", pContext->stat);
		return -55;
	}

	pstCallback->HandleClose((SocketClientDef *)pContext, pUserInfo);

	if (pContext->stat == SOCKET_TCP_CONNECTED || pContext->stat == SOCKET_TCP_CONNECTING) {
		//TODO
		if (pContext->iSocket != pstSrvConfig->stCltMng.aiSocket[pContext->iClientIndex]) {
			LOG("BUG SOCKET_TCP_CONNECTED pContext->iSocket(%d) != stCltMng.iSocke(%d)", 
					pContext->iSocket, pstSrvConfig->stCltMng.aiSocket[pContext->iClientIndex]);
		}

		LOGTRACE("change cltMng astat to SOCKET_TCP_RECONNECT_WAIT");
		pstSrvConfig->stCltMng.aStat[pContext->iClientIndex] = SOCKET_TCP_RECONNECT_WAIT;
	}

	if (pContext->stat == SOCKET_TCP_ACCEPT) {
		pstSrvConfig->iCurAcceptSocketNum--;
		//TODO
		LOGTRACE("CurAcceptSocketNum-- -> %d", pstSrvConfig->iCurAcceptSocketNum);
	}

	pContext->stat = SOCKET_UNUSED;

	if ((iRet = AnfDelFd(pstSrvConfig->pstAnfMng, pContext->iSocket)) < 0) {
		LOG("AnfDelFd failed iRet %d iSocket %d", iRet, pContext->iSocket);
		close(pContext->iSocket);
		return -57;
	}

	if (close(pContext->iSocket)) {
		perror("ProcessClose close failed");
		return -59;
	}

	return 0;
}

static int ProcessAccept(SocketContext *pContext, void *pUserInfo)
{
	SrvCallBack *pstCallback = &(pstSrvConfig->stCallBack);
	int iRet = 0, iSocket = 0;
	static struct sockaddr_in stAddr;
	int iLen = sizeof(stAddr);
	SocketContext *pNewContext = NULL;
	void *pNewUserInfo = NULL;

	if (pContext == NULL) {
		LOG("pContext == NULL");
		return -51;
	}

	if (pContext->stat != SOCKET_TCP_LISTEN) {
		LOG("ProcessAccept error stat %d", pContext->stat);
		return -61;
	}

	if (pstSrvConfig->iCurAcceptSocketNum >= pstSrvConfig->iMaxAcceptSocketNum) {
		LOG("iCurAcceptSocketNum >= iMaxAcceptSocketNum(%d)", pstSrvConfig->iMaxAcceptSocketNum);
		return -63;
	}
	
	iSocket = accept(pContext->iSocket, (struct sockaddr *)&stAddr, (socklen_t *)&iLen);
	if (iSocket == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return 0; //ok
		perror("accept error");
		return -64;
	}

	if (iSocket > pstSrvConfig->iMaxFdNum) {
		LOG("isocket(%d) > iMaxFdNum(%d)", iSocket, pstSrvConfig->iMaxFdNum);
		return -65;
	}

	SetNBLock(iSocket);

	pNewContext = &((pstSrvConfig->astSocketContext)[iSocket]);
	pNewUserInfo = (char *)pstSrvConfig->aUserInfo + iSocket * pstSrvConfig->iUserInfoLen;

	pNewContext->stat = SOCKET_TCP_ACCEPT;
	pNewContext->iSocket = iSocket;
	memcpy(&(pNewContext->stClientAddr), &stAddr, sizeof(stAddr));
	pNewContext->iBytesRecved = 0;
	pNewContext->iBytesSend = 0;
	pNewContext->iPkgLen = 0;
	pNewContext->tLastAccessTime = pNewContext->tCreateTime = time(NULL);

	//new fd allow read
	if ((iRet = AnfAddFd(pstSrvConfig->pstAnfMng, iSocket, ANF_FLAG_READ | ANF_FLAG_ERROR)) < 0) {
		LOG("AnfAddFd failed iRet %d iSocket %d", iRet, iSocket);
		ProcessClose(pNewContext, pNewUserInfo);
		return -67;
	}

	if ((iRet = pstCallback->HandleAccept((SocketClientDef *)pNewContext, pNewUserInfo)) < 0) {
		LOG("HandleAccept failed iRet %d iSocket %d", iRet, iSocket);
		ProcessClose(pNewContext, pNewUserInfo);
		return -59;
	}

	pstSrvConfig->iCurAcceptSocketNum++;

	return 0;
}

static int ProcessTcpRead(SocketContext *pContext, void *pUserInfo)
{
	SrvCallBack *pstCallback = &(pstSrvConfig->stCallBack);
	int iRet = 0;
	int iReqLen = 0;
	int iPkgLen = 0;

	if (pContext == NULL) {
		LOG("pContext == NULL");
		return -61;
	}

	if (pContext->stat != SOCKET_TCP_ACCEPT &&
			pContext->stat != SOCKET_TCP_CONNECTED) {
		LOG("ProcessTcpRead error stat(%d)", pContext->stat);
		return -62;
	}

	iRet = recv(pContext->iSocket, pContext->RecvBuf + pContext->iBytesRecved, 
			sizeof(pContext->RecvBuf) - pContext->iBytesRecved, 0);
	if (iRet < 0) {
		LOG("recv error ret %d errno %d", iRet, errno);
		ProcessClose(pContext, pUserInfo);
		return -63;
	}

	if (iRet == 0 && pContext->iBytesSend == 0) {
		LOG("iRet == 0 && pContext->iBytesSend == 0");
		ProcessClose(pContext, pUserInfo);
		return -63;
	}

	LOGTRACE("recv iSocket %d iBytesRecved %d newRecv %d total %d", pContext->iSocket,
			pContext->iBytesRecved, iRet, sizeof(pContext->RecvBuf));

	pContext->iBytesRecved += iRet;
	
	do {
		//try to call HandlePkgHead one time
		if (pContext->iPkgLen == 0) {
			if (pContext->stat == SOCKET_TCP_ACCEPT)
				iReqLen = pstSrvConfig->iPkgHeadLenAsSrv;
			else 
				iReqLen = pstSrvConfig->iPkgHeadLenAsClt;

			if (pContext->iBytesRecved < iReqLen) {
				LOGTRACE("pContext->iBytesRecved(%d) < iReqLen(%d) stat(%d)",
						pContext->iBytesRecved, iReqLen, pContext->stat);
				break;//the pkg not enougth
			}

			//try to get iPkgLen !!!
			//iPkgLen meaning the one hold pkg size
			iRet = pstCallback->HandlePkgHead((SocketClientDef *)pContext, pUserInfo, 
					pContext->RecvBuf, pContext->iBytesRecved, &pContext->iPkgLen);
			if (iRet < 0){
				LOG("HandlePkgHead failed iRet %d", iRet);
				ProcessClose(pContext, pUserInfo);
				return -65;
			}
		}

		iPkgLen = pContext->iPkgLen;

		//TODO
		if (iPkgLen == 0) {
			LOG("BUG !! pContext->iPkgLen == 0");
			return -64;
		}

		if (iPkgLen > sizeof(pContext->RecvBuf)) {
			LOG("NO Way!! iPkgLen(%d) > RecvBuf(%d)", iPkgLen, pContext->iPkgLen);
			ProcessClose(pContext, pUserInfo);
			return -66;
		}

		//not enough continnue recv
		if (pContext->iBytesRecved < iPkgLen) {
			LOG("iBytesRecved(%d) < iPkgLen(%d) continue", pContext->iBytesRecved, iPkgLen);
			break;
		}

		iRet = pstCallback->HandlePkg((SocketClientDef *)pContext, pUserInfo, pContext->RecvBuf, iPkgLen);
		if (iRet < 0) {
			LOG("HandlePkg failed iRet %d", iRet);
			ProcessClose(pContext, pUserInfo);
			return -67;
		}

		pContext->iPkgLen = 0;
		pContext->iBytesRecved -= iPkgLen;

		LOGTRACE("after processPkg(%d) iBytesRecved %d", iPkgLen,  pContext->iBytesRecved);

		if (pContext->iBytesRecved == 0)
			break;

		if (pContext->iBytesSend < 0) {
			LOG("BUG iBytesSend %d", pContext->iBytesSend);
			return -69;
		}

		memmove(pContext->RecvBuf, pContext->RecvBuf + iPkgLen, pContext->iBytesSend);

	} while (0);

	return 0;
}

//TODO
static int ProcessUdpRead(SocketContext *pContext, void *pUserInfo)
{
	SrvCallBack *pstCallback = &(pstSrvConfig->stCallBack);
	int iRet = 0;
	int iAddrLen = sizeof(pContext->stClientAddr);

	if (pContext == NULL) {
		LOG("pContext == NULL");
		return -71;
	}

	if (pContext->stat != SOCKET_UDP) {
		LOG("ProcessUdpRead error stat(%d)", pContext->stat);
		return -72;
	}

	iRet = recvfrom(pContext->iSocket, pContext->RecvBuf, sizeof(pContext->RecvBuf), 0, 
			(struct sockaddr *)&(pContext->stClientAddr), (socklen_t *)&iAddrLen);
	if (iRet <= 0) {
		LOG("recvfrom failed %d", iRet);
		return -75;
	}

	iRet = pstCallback->HandleUdpPkg((SocketClientDef *)pContext, pUserInfo, 0, pContext->RecvBuf, iRet);
	if (iRet <= 0) {
		LOG("HandleUdpPkg failed %d", iRet);
		return -77;
	}

	return 0;
}

static int ProcessTcpConnect(SocketContext *pContext, void *pUserInfo)
{
	SrvCallBack *pstCallback = &(pstSrvConfig->stCallBack);
	int iRet = 0;
	int iSockErr, iSockErrLen = sizeof(iSockErr);

	if (pContext == NULL) {
		LOG("pContext == NULL");
		return -81;
	}

	if (getsockopt(pContext->iSocket, SOL_SOCKET, SO_ERROR, &iSockErr, (socklen_t *)&iSockErrLen)) {
		perror("getsockopt failed");
		return -83;
	}

	if (iSockErr) {
		LOG("getsockopt iSockErr %d", iSockErr);
		return -84;
	}

	pContext->stat = SOCKET_TCP_CONNECTED;
	pContext->iBytesRecved = 0;
	pContext->iBytesSend = 0;
	pContext->iPkgLen = 0;
	pContext->tLastAccessTime = pContext->tCreateTime = time(NULL);
	pstSrvConfig->stCltMng.aStat[pContext->iClientIndex] = pContext->stat;

	if ((iRet = AnfModFd(pstSrvConfig->pstAnfMng, pContext->iSocket, ANF_FLAG_READ | ANF_FLAG_ERROR)) < 0) {
		LOG("AnfModFd failed %d", iRet);
	}

	iRet = pstCallback->HandleConnect((SocketClientDef *)pContext, pUserInfo);
	if (iRet < 0) {
		LOG("HandleConnect failed %d", iRet);
		ProcessClose(pContext, pUserInfo);
		return -85;
	}

	return 0;
}

static int ProcessTcpWrite(SocketContext*pContext, void *pUserInfo)
{
	int iRet = 0;

	if (pContext == NULL) {
		LOG("pContext == NULL");
		return -91;
	}

	if (pContext->stat == SOCKET_UNUSED 
			|| (pContext->stat != SOCKET_TCP_ACCEPT && 
				pContext->stat != SOCKET_TCP_CONNECTED)) {
		LOG("error stat %d", pContext->stat);
		return -92;
	}

	pContext->tLastAccessTime = time(NULL);
	iRet = send(pContext->iSocket, pContext->SendBuf, pContext->iBytesSend, 0);
	if (iRet == 0) {
		LOGTRACE("ret 0");
		return 0;
	}

	if (iRet < 0) {
		if (errno == EAGAIN || errno == EINTR) {
			LOG("errno == EAGAIN || errno == EINT %d", errno);
			return 0;
		}
		LOG("send failt errno %d", errno);
		ProcessClose(pContext, pUserInfo);
		return -93;
	}

	if (iRet >= pContext->iBytesSend) {
		pContext->iBytesSend = 0;
		LOG("send success iRet %d", iRet);
		if ((iRet = AnfModFd(pstSrvConfig->pstAnfMng, pContext->iSocket, ANF_FLAG_READ | ANF_FLAG_ERROR)) < 0) {
			LOG("AnfModFd failed %d", iRet);
		}
		return 0;
	}

	pContext->iBytesSend -= iRet;
	memmove(pContext->SendBuf, pContext->SendBuf + iRet, pContext->iBytesSend);

	LOGTRACE("send not finish continue iRet %d iBytesSend %d", iRet, pContext->iBytesSend);

	return 0;
}

static int ProcessUdpWrite(SocketContext*pContext, void *pUserInfo)
{
	/*
	SrvCallBack *pstCallback = &(pstSrvConfig->stCallBack);
	int iRet = 0, iSocket = 0;

	if (pContext == NULL) {
		LOG("pContext == NULL");
		return -51;
	}

	LOG("ProcessUdpWrite don't imp");
	*/
	return 0;
}

/*
 * 启动框架
 *      1、初始化AnfInit结构
 *      2、监听端口，并加入多路复用
 *      3、初始化客户端发包结构，并加入多路复用
 *      4、wait收发包多路复用fd
 *      5、调用相应回调函数
 */
int AsyncNetFrameworkLoop()
{
	int i = 0, iRet = 0;
	pstSrvConfig->astSocketContext = (SocketContext *)malloc(sizeof(*(pstSrvConfig->astSocketContext)) * pstSrvConfig->iMaxFdNum);
	if (!(pstSrvConfig->astSocketContext)) {
		perror("astSocketContext malloc failed");
		return -1;
	}

	//TODO
	LOGTRACE("malloc astSocketContext one %d * num %d = %d", 
			sizeof(*(pstSrvConfig->astSocketContext)), pstSrvConfig->iMaxFdNum, 
				sizeof(*(pstSrvConfig->astSocketContext)) * pstSrvConfig->iMaxFdNum);

	for (i = 0; i < pstSrvConfig->iMaxFdNum; i++) {
		pstSrvConfig->astSocketContext[i].stat = SOCKET_UNUSED;
	}

	pstSrvConfig->pstAnfMng = AnfInit(pstSrvConfig->pstLog, pstSrvConfig->iLogLevel, pstSrvConfig->iMaxFdNum);
	if (!pstSrvConfig->pstAnfMng) {
		LOG("AnfInit failed");
		return -3;
	}

	//TODO
	LOGTRACE("pstAnfMng Addr %p", pstSrvConfig->pstAnfMng);

	if ((iRet = InitListenSocket()) < 0) {
		LOG("InitListenSocket failed iRet %d", iRet);
		return -5;
	}


	if ((iRet = InitClientSocket()) < 0) {
		LOG("InitClientSocket failed iRet %d", iRet);
		return -7;
	}

	pstSrvConfig->iCurAcceptSocketNum = 0;

	int iPos = 0, iFd = 0, iFlag = 0;
	int iIsTriggered = 0;
	SocketContext *pContext;

	//TODO
	void *pUserInfo;

	SrvCallBack *pstCallback = &(pstSrvConfig->stCallBack);

	while(true) {
		//do asyc io
		pstCallback->HandleLoop();

		//TODO check tcp connect and reconnect for timeout

		if ((iIsTriggered = AnfWaitForFd(pstSrvConfig->pstAnfMng, pstSrvConfig->iTimeoutMSec)) <= 0) {
			LOG("AnfWaitForFd failed ret %d", iIsTriggered);
			continue;
		}

		//TODO
		LOGTRACE("AnfWaitForFd iIsTriggered %d iTimeoutMSec %d", iIsTriggered, pstSrvConfig->iTimeoutMSec);

		iPos = 0;
		while((iFd = AnfGetReadyFd(pstSrvConfig->pstAnfMng, &iPos, &iFlag)) >= 0) {
			pContext = &(pstSrvConfig->astSocketContext[iFd]);
			pUserInfo = (char *)pstSrvConfig->aUserInfo + (iFd * pstSrvConfig->iUserInfoLen);

			//TODO
			LOGTRACE("AnfGetReadyFd iFd %d iPos %d iFlag %d", iFd, iPos, iFlag);

			if (pContext == NULL || pUserInfo == NULL) {
				LOG("Bug pContext == NULL || pUserInfo == NULL");
				continue;
			}

			if (pContext->iSocket != iFd){
				LOG("Bug pContext->iSocket(%d) != iFd(%d)", pContext->iSocket, iFd);
				continue;
			}

			//error
			if (iFlag & ANF_FLAG_ERROR) {
				//TODO
				LOGTRACE("ANF_FLAG_ERROR");

				ProcessClose(pContext, pUserInfo);
				continue;
			}

			if (iFlag & ANF_FLAG_READ) {
				//TODO
				LOGTRACE("ANF_FLAG_READ");

				if (pContext->stat == SOCKET_TCP_LISTEN) {
					//TODO
					LOGTRACE("ANF_FLAG_READ SOCKET_TCP_LISTEN");

					ProcessAccept(pContext, pUserInfo);
					continue;
				}

				if (pContext->stat == SOCKET_TCP_ACCEPT || pContext->stat == SOCKET_TCP_CONNECTED) {
					//TODO
					LOGTRACE("ANF_FLAG_READ SOCKET_TCP_ACCEPT || SOCKET_TCP_CONNECTED %d", pContext->stat);

					pContext->tLastAccessTime = time(NULL);
					ProcessTcpRead(pContext, pUserInfo);
				}

				if (pContext->stat == SOCKET_UDP) {
					//TODO
					LOGTRACE("ANF_FLAG_READ SOCKET_UDP");

					ProcessUdpRead(pContext, pUserInfo);
				}
			}

			if (iFlag & ANF_FLAG_WRITE) {
				//TODO
				LOGTRACE("ANF_FLAG_WRITE");

				if (pContext->stat == SOCKET_TCP_CONNECTING) {
					//TODO
					LOGTRACE("ANF_FLAG_WRITE SOCKET_TCP_CONNECTING");

					ProcessTcpConnect(pContext, pUserInfo);
					break;
				}

				//TODO
				if (pContext->stat == SOCKET_UDP) {
					//TODO
					//impossibie
					LOGTRACE("ANF_FLAG_WRITE SOCKET_UDP");

					ProcessUdpWrite(pContext, pUserInfo);
				} else {
					//TODO
					LOGTRACE("ANF_FLAG_WRITE Tcp");

					ProcessTcpWrite(pContext, pUserInfo);
				}
			}
		}

		//TODO
		LOGTRACE("while break iFd %d iPos %d iFlag %d", iFd, iPos, iFlag);
	}

	return 0;
}

int SendTcpPkg(SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iPkgLen)
{
	int iRet = 0;
	SocketContext *pContext = (SocketContext *)pstScd;

	if (!pContext) {
		LOG("SendTcpPkg failed pContext == NULL");
		return -1;
	}

	if (pContext->stat == SOCKET_UNUSED 
			|| (pContext->stat != SOCKET_TCP_ACCEPT && 
				pContext->stat != SOCKET_TCP_CONNECTED)) {
		LOG("error stat %d", pContext->stat);
		return -2;
	}

	if (pPkg == NULL || iPkgLen <= 0 || iPkgLen > sizeof(pContext->SendBuf)) {
		LOG("Pkg error param error");
		return -4;
	}

	pContext->tLastAccessTime = time(NULL);

	//buf too much data so add to buf
	if (pContext->iBytesSend > 0) {
		if (pContext->iBytesSend + iPkgLen > sizeof(pContext->SendBuf)) {
			LOG("iPkgLen too big");
			ProcessClose(pContext, pUserInfo);
			return -5;
		}
		memcpy(pContext->SendBuf + pContext->iBytesSend, pPkg, iPkgLen);
		pContext->iBytesSend += iPkgLen;
		return 0;
	}

	iRet = send(pContext->iSocket, pPkg, iPkgLen, 0);

	//TODO
	LOGTRACE("iPkgLen %d send iRet %d iByteSend %d", iPkgLen, iRet, pContext->iBytesSend);

	//sent parttly
	if (iRet > 0) {
		if (iRet == iPkgLen) {
			LOG("send ok exactly");
			return 0;
		}

		memcpy(pContext->SendBuf, (char *)pPkg + iRet, iPkgLen - iRet);
		pContext->iBytesSend = iPkgLen - iRet;

		//TODO
		LOGTRACE("after iPkgLen %d send iRet %d iByteSend %d", iPkgLen, iRet, pContext->iBytesSend);

		if (AnfModFd(pstSrvConfig->pstAnfMng, pContext->iSocket, 
					ANF_FLAG_WRITE | ANF_FLAG_READ | ANF_FLAG_ERROR) < 0) {
			ProcessClose(pContext, pUserInfo);
			return -6;
		}
		
		return 0;
	}

	//send nothing
	if (iRet < 0 && (errno == EAGAIN || errno == EINTR)) {
		memcpy(pContext->SendBuf, pPkg, iPkgLen);
		pContext->iBytesSend = iPkgLen;
		if (AnfModFd(pstSrvConfig->pstAnfMng, pContext->iSocket, 
					ANF_FLAG_WRITE | ANF_FLAG_READ | ANF_FLAG_ERROR) < 0) {
			ProcessClose(pContext, pUserInfo);
			return -7;
		}
		
		return 0;
	}

	LOG("send failed iRet %d errno %d", iRet, errno);

	ProcessClose(pContext, pUserInfo);

	return -9;
}


int SendUdpPkg(SocketClientDef *pstScd, ListenEntry *pstListenEntry, 
		void *pUserInfo, void *pPkg, int iPkgLen)
{

	int iSocket = 0, iRet = 0;
	SocketContext *pContext = NULL;
	struct sockaddr_in *pstAddr = NULL;

	if (pPkg == NULL || iPkgLen <= 0) {
		LOG("Pkg params erro");
		return -1;
	}

	pstAddr = CreateAddrEx(pstListenEntry->sIp, pstListenEntry->iPort, "udp");

	if (pstAddr == NULL) {
		LOG("CreateAddrEx error");
		return -2;
	}

	iRet = CreateUdpClientSocketEx(&iSocket, pstListenEntry->sIp, pstListenEntry->iPort, NO_BLOCK);

	if (iRet || iSocket <= 0 ||iSocket > pstSrvConfig->iMaxFdNum) {
		LOG("CreateUdpClientSocketEx failed iRet(%d) ip %s port %d id %d socket %d", iRet, pstListenEntry->sIp,
				pstListenEntry->iPort, pstListenEntry->iName, iSocket);
		return -3;
	}

	//TODO
	LOGTRACE("CreateUdpClientSocketEx success iRet(%d) ip %s port %d id %d socket %d", 
			iRet, pstListenEntry->sIp, pstListenEntry->iPort, pstListenEntry->iName, iSocket);

	pContext = &((pstSrvConfig->astSocketContext)[iSocket]);
	pContext->stat = SOCKET_UDP;
	pContext->iSocket = iSocket;
	pContext->tCreateTime = pContext->tLastAccessTime = time(NULL);
	//pContext->iClientIndex = i;// index astClientDef
	//pstSrvConfig->stCltMng.aiSocket[i] =iSocket;
	///pstSrvConfig->stCltMng.aStat[i] = pContext->stat;
	//pstSrvConfig->stCltMng.atLastConnectTime[i] = time(NULL);

	iRet = sendto(pContext->iSocket, pPkg, iPkgLen, 0, (const struct sockaddr *)pstAddr, sizeof(struct sockaddr));

	if (iRet != iPkgLen) {
		perror("sendto error");
		LOG("sento failed %d iPkgLen %d ", iRet, iPkgLen);
		return -7;
	}

	LOG("sendto sucess ret %d", iRet);
	if ((iRet = AnfAddFd(pstSrvConfig->pstAnfMng, iSocket, ANF_FLAG_READ | ANF_FLAG_ERROR)) < 0) {
		LOG("AnfAddFd failed iRet(%d) ip %s port %d id %d iSock %d", iRet,
				pstListenEntry->sIp, pstListenEntry->iPort, pstListenEntry->iName, iSocket);
		return -9;
	}

	//TODO
	LOGTRACE("AnfAddFd success iRet(%d) ip %s port %d id %d iSock %d write %d", 
			iRet, pstListenEntry->sIp, pstListenEntry->iPort, pstListenEntry->iName, iSocket);

	return 0;
}

int GetContext(int iSocket, SocketClientDef **ppstScd, void **ppUserInfo)
{
	if (iSocket < 0 || iSocket >= pstSrvConfig->iMaxFdNum) {
		LOG("GetContext error iSocket %d", iSocket);
		return -1;
	}

	if (ppstScd)
		*ppstScd = (SocketClientDef *)&((pstSrvConfig->astSocketContext)[iSocket]);

	if (ppUserInfo)
		*ppUserInfo = (char *)pstSrvConfig->aUserInfo + iSocket * pstSrvConfig->iUserInfoLen;

	return 0;
}

void SetWaitTimeout(int iTimeoutMSec)
{
	pstSrvConfig->iTimeoutMSec = iTimeoutMSec;
}
