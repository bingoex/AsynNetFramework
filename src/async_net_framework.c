
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

	struct in_addr stTcpListenAddr;
	unsigned short ushTcpPort[MAX_TCP_PORT];

	struct in_addr stUdpListenAddr;
	unsigned short ushUdpPort[MAX_UDP_PORT];
	int aiUdpName[MAX_UDP_PORT];
	int aiUdpSocket[MAX_UDP_PORT];

	ClientDefMng stCltMng;

	AnfMng *pstAnfMng;
	
	int iMaxAcceptSocketNum; //just for accept
	int iCurAcceptSocketNum;

	int iTotalTcpNum; // iMaxAcceptSocketNum + stCltMng.iClientDefNum

	int iMaxFdNum; // iTotalTcpNum + 64

	SocketContext *astSocketContext;
	SrvCallBack stCallBack;
} SrvConfig;

static SrvConfig stSrvConfig;
SrvConfig *pstSrvConfig = &stSrvConfig;

	

