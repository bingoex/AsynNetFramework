#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#include "async_base.h"


#define ANF_SELECT
//#define ANF_EPOLL
//#define ANF_KQUEUE

#if defined(ANF_EPOLL)
#include "epoll.h"
#elif defined(ANF_SELECT)
#include <sys/select.h>
#elif defined(ANF_KQUEUE)
#include <sys/event.h>
#else
#error please define ANF_EPOLL or ANF_SELECT !!
#endif 

struct _AnfMng
{
	LogFile *pstLog;
	int iLogLevel;

	int iMaxSocketNum;
	int iIsTriggered;

#if defined(ANF_EPOLL)
	int iEpollFd;
	struct epoll_event *evs;
#elif defined(ANF_SELECT)
	fd_set stReadFds;
	fd_set stWriteFds;
	fd_set stExceptFds;

	fd_set stReadTmpFds;
	fd_set stWriteTmpFds;
	fd_set stExceptTmpFds;

#elif defined(ANF_KQUEUE)

#endif
};

#define LOG(fmt, args...) \
	do { \
		if (pstAnfMng->pstLog) { \
			Log(pstAnfMng->pstLog, LOG_FORMAT_TYPE_TIME, "[%s][%d]%s() "fmt, \
					__FILE__, __LINE__, __FUNCTION__, ## args);\
		} \
	}while(0) \


AnfMng *AnfInit(LogFile *pstLog, int iLogLevel, int iMaxSocketNum)
{

	AnfMng *pstAnfMng;
	if (!(pstAnfMng = (AnfMng *)malloc(sizeof(AnfMng)))) {
		perror("malloc AnfMng failed");
		return NULL;
	}

	//TODO
	LOG("try to AnfInit");

	memset(pstAnfMng, 0 , sizeof(*pstAnfMng));

	pstAnfMng->pstLog = pstLog;
	pstAnfMng->iLogLevel = iLogLevel;

	if (iMaxSocketNum < 0) {
		printf("error iMaxSocketNum\n");
		return NULL;
	}

	pstAnfMng->iMaxSocketNum = iMaxSocketNum;

	//TODO
	LOG("AnfInit iLogLevel %d iMaxSocketNum %d", iLogLevel, iMaxSocketNum);

#if defined(ANF_EPOLL)
	if ((pstAnfMng->iEpollFd = epoll_create(iMaxSocketNum)) < 0) {
		perror("epoll_create failed");
		return NULL;
	}

	if (!(pstAnfMng->evs = (struct epoll_event *)malloc(sizeof(struct epoll_event) * iMaxSocketNum))) {
		perror("malloc epoll_event failed");
		return NULL;
	}

	LOG("iEpollFd %d evs %p", pstAnfMng->iEpollFd, pstAnfMng->evs);
#elif defined(ANF_SELECT)
	if (iMaxSocketNum > FD_SETSIZE) {
		LOG("iMaxSocketNum(%d) > FD_SETSIZE(%d)", iMaxSocketNum, FD_SETSIZE);
		return NULL;
	}

	FD_ZERO(&(pstAnfMng->stReadFds));
	FD_ZERO(&(pstAnfMng->stWriteFds));
	FD_ZERO(&(pstAnfMng->stExceptFds));
	FD_ZERO(&(pstAnfMng->stReadTmpFds));
	FD_ZERO(&(pstAnfMng->stWriteTmpFds));
	FD_ZERO(&(pstAnfMng->stExceptTmpFds));

#elif defined(ANF_KQUEUE)

#endif

	LOG("AnfInit pstAnfMng %p success!!!", pstAnfMng);
	return pstAnfMng;
}

#if defined(ANF_EPOLL)
static inline void AnfSetEpollEv(AnfMng *pstAnfMng, struct epoll_event *pev, int iFd, int iFlag)
{
	uint32_t epoll_events = 0;

	if (iFlag & ANF_FLAG_READ) {
		epoll_events |= EPOLLIN;
	}

	if (iFlag & ANF_FLAG_WRITE) {
		epoll_events |= EPOLLOUT;
	}

	if (iFlag & ANF_FLAG_ERROR) {
		epoll_events |= EPOLLHUP | EPOLLERR;
	}

	pev->data.fd = iFd;
	pev->events = epoll_events;
	LOG("fd %d event %d", iFd, epoll_events);
}

static inline void AnfGetEpollEv(AnfMng *pstAnfMng, struct epoll_event *pev, int *piFd, int *piFlag)
{
	int iFlag = 0;

	if (pev->events & EPOLLIN) {
		iFlag |= ANF_FLAG_READ;
	}

	if (pev->events & EPOLLOUT) {
		iFlag |= ANF_FLAG_WRITE;
	}

	if (pev->events & (EPOLLHUP | EPOLLERR)) {
		iFlag |= ANF_FLAG_ERROR;
	}

	*piFd = pev->data.fd;
	*piFlag = iFlag;

	LOG("fd %d event %d", *piFd, *piFlag);
}

#elif defined(ANF_SELECT)

static inline void AnfSetSeletFds(AnfMng *pstAnfMng, int iFd, int iFlag)
{
	if (iFlag & ANF_FLAG_READ) {
		FD_SET(iFd, &(pstAnfMng->stReadFds));
	} else {
		FD_CLR(iFd, &(pstAnfMng->stReadFds));
	}

	if (iFlag & ANF_FLAG_WRITE) {
		FD_SET(iFd, &(pstAnfMng->stWriteFds));
	} else {
		FD_CLR(iFd, &(pstAnfMng->stWriteFds));
	}

	if (iFlag & ANF_FLAG_ERROR) {
		FD_SET(iFd, &(pstAnfMng->stExceptFds));
	} else {
		FD_CLR(iFd, &(pstAnfMng->stExceptFds));
	}

	LOG("fd %d iFlag %d addr %p %p iFlag & ANF_FLAG_WRITE (%d) stWriteFds %llu", 
			iFd, iFlag, pstAnfMng, &(pstAnfMng->stWriteFds), iFlag & ANF_FLAG_WRITE, pstAnfMng->stWriteFds);
}

static inline void AnfGetSelectFds(AnfMng *pstAnfMng, int iFd, int *piFlag)
{

	if (FD_ISSET(iFd, &(pstAnfMng->stReadTmpFds))) {
		*piFlag |= ANF_FLAG_READ;
	}

	if (FD_ISSET(iFd, &(pstAnfMng->stWriteTmpFds))) {
		*piFlag |= ANF_FLAG_WRITE;
	}

	if (FD_ISSET(iFd, &(pstAnfMng->stExceptTmpFds))) {
		*piFlag |= ANF_FLAG_ERROR;
	}

	if (*piFlag) 
		LOG("fd %d flag %d", iFd, *piFlag);
}

#elif defined(ANF_KQUEUE)

static inline void AnfSetKqueueEv(AnfMng *pstAnfMng, int iFd, int iFlag)
{
	LOG("fd %d Flag %d", iFd, iFlag);
}

static inline void AnfGetKqueueEv(AnfMng *pstAnfMng, int *piFd, int *piFlag)
{
	LOG("fd %d Flag %d", *piFd, *piFlag);
}
#endif


int AnfAddFd(AnfMng *pstAnfMng, int iFd, int iFlag)
{
	if (iFd <= 0)
		return -1;

#if defined(ANF_EPOLL)
	struct epoll_event ev;

	AnfSetEpollEv(pstAnfMng, &ev, iFd, iFlag);

	if (epoll_ctl(pstAnfMng->iEpollFd, EPOLL_CTL_ADD, iFd, &ev) < 0) {
		if (errno != EEXIST || epoll_ctl(pstAnfMng->iEpollFd, EPOLL_CTL_MOD, iFd, &ev) < 0) {
			LOG("epoll_ctl EPOLL_CTL_MOD failed");
			return -2;
		} else {
			LOG("warning: epoll_ctr add failed but mod success");
		}
	}

#elif defined(ANF_SELECT)
	AnfSetSeletFds(pstAnfMng, iFd, iFlag);
	LOG("after addr %p %p", pstAnfMng, &(pstAnfMng->stWriteFds));
#elif defined(ANF_KQUEUE)

#endif

	return 0;
}

int AnfModFd(AnfMng *pstAnfMng, int iFd, int iFlag)
{
	if (iFd <= 0)
		return -1;

#if defined(ANF_EPOLL)
	struct epoll_event ev;

	AnfSetEpollEv(pstAnfMng, &ev, iFd, iFlag);

	if (epoll_ctl(pstAnfMng->iEpollFd, EPOLL_CTL_MOD, iFd, &ev) < 0) {
		if (errno != ENOENT || epoll_ctl(pstAnfMng->iEpollFd, EPOLL_CTL_ADD, iFd, &ev) < 0) {
			LOG("epoll_ctl EPOLL_CTL_MOD failed");
			return -2;
		} else {
			LOG("warning: epoll_ctr mod failed but mod success");
		}
	}

#elif defined(ANF_SELECT)
	AnfSetSeletFds(pstAnfMng, iFd, iFlag);
#elif defined(ANF_KQUEUE)

#endif

	return 0;
}


int AnfDelFd(AnfMng *pstAnfMng, int iFd)
{
	if (iFd <= 0)
		return -1;

#if defined(ANF_EPOLL)
	struct epoll_event ev;

	if (epoll_ctl(pstAnfMng->iEpollFd, EPOLL_CTL_DEL, iFd, &ev) < 0) {
		if (errno != ENOENT) {
			LOG("epoll_ctl EPOLL_CTL_DEL failed");
			return -2;
		} else {
			LOG("warning: epoll_ctr del failed");
		}
	}

#elif defined(ANF_SELECT)
	AnfSetSeletFds(pstAnfMng, iFd, 0);
#elif defined(ANF_KQUEUE)

#endif

	return 0;
}

int AnfWaitForFd(AnfMng *pstAnfMng, int iTimeoutMSec)
{
	int iIsTriggered = 0;
#if defined(ANF_EPOLL)
	if (iTimeoutMSec < 0) {
		iTimeoutMSec = -1;
	}
	iIsTriggered = epoll_wait(pstAnfMng->iEpollFd, pstAnfMng->evs, pstAnfMng->iMaxSocketNum, iTimeoutMSec);
#elif defined(ANF_SELECT)
	struct timeval tv, *ptv = &tv;
	tv.tv_sec = iTimeoutMSec / 1000;
	tv.tv_usec = iTimeoutMSec % 1000 * 1000;

	//return immediately
	if (iTimeoutMSec < 0)
		ptv = NULL;

	pstAnfMng->stReadTmpFds = pstAnfMng->stReadFds;
	pstAnfMng->stWriteTmpFds = pstAnfMng->stWriteFds;
	pstAnfMng->stExceptTmpFds = pstAnfMng->stExceptFds;

	//LOG("iMaxSocketNum %d", pstAnfMng->iMaxSocketNum);

	iIsTriggered = select(pstAnfMng->iMaxSocketNum, 
			&(pstAnfMng->stReadTmpFds), 
			&(pstAnfMng->stWriteTmpFds), 
			&(pstAnfMng->stExceptTmpFds), ptv);
#elif defined(ANF_KQUEUE)

#endif
	if (iIsTriggered < 0) {
		if (errno != EAGAIN && errno != EINTR) {
			LOG("epoll_wait erro errno != EAGAIN && errno != EINT");
		}
		return -2;
	}

	pstAnfMng->iIsTriggered = iIsTriggered;

	return iIsTriggered;
}


//the first call iPos should be 0
//return fd
int AnfGetReadyFd(AnfMng *pstAnfMng, int *piPos, int *piFlag)
{
	*piFlag = 0;

	if (*piPos < 0)
		return -1;

#if defined(ANF_EPOLL)
	int iFd;

	if (*piPos >= pstAnfMng->iIsTriggered) {
		LOG("GetReady over");
		return -2;
	}

	AnfGetEpollEv(pstAnfMng, pstAnfMng->evs + *piPos, &iFd, piFlag);
	if (!(*piFlag)) {
		LOG("BUG! even occur but no Flag");
		return -4;
	}

	(*piPos)++;
	return iFd;
#elif defined(ANF_SELECT)
	while (*piPos < pstAnfMng->iMaxSocketNum) {
		AnfGetSelectFds(pstAnfMng, *piPos, piFlag);

		if (*piFlag)
			return ((*piPos)++);

		(*piPos)++;
	}

	return -8;
#elif defined(ANF_KQUEUE)

	return -1;
#endif
}

