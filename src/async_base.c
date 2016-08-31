#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#include "async_base.h"


//#define ANF_SELECT
#define ANF_EPOLL
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

#define LOG(fmt, args...) \
	do { \
		if (pstAnfMng->pstLog) { \
			Log(pstAnfMng->pstLog, LOG_FORMAT_TYPE_TIME, "%s:%d:%s "fmt, \
					__FILE__, __LINE__, __FUNCTION__, ## args);\
		} \
	}while(0) \

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

AnfMng *AnfInit(LogFile *pstLog, int iLogLevel, int iMaxSocketNum)
{
	AnfMng *pstAnfMng;
	if (!(pstAnfMng = (AnfMng *)malloc(sizeof(AnfMng)))) {
		perror("malloc AnfMng failed");
		return NULL;
	}

	memset(pstAnfMng, 0 , sizeof(*pstAnfMng));

	pstAnfMng->pstLog = pstLog;
	pstAnfMng->iLogLevel = iLogLevel;

	if (iMaxSocketNum < 0) {
		printf("error iMaxSocketNum\n");
		return NULL;
	}

	LOG("iLogLevel %d iMaxSocketNum %d", iLogLevel, iMaxSocketNum);

#if defined(ANF_EPOLL)
	if ((pstAnfMng->iEpollFd = epoll_create(iMaxSocketNum)) < 0) {
		perror("epoll_create failed");
		return NULL;
	}

	if (!(pstAnfMng->evs = (struct epoll_event *)malloc(sizeof(struct epoll_event) * iMaxSocketNum))) {
		perror("malloc epoll_event failed");
		return NULL;
	}

	LOG("iEpollFd %d", pstAnfMng->iEpollFd);
#elif defined(ANF_SELECT)
	if (iMaxSocketNum > FD_SETSIZE) {
		perror("iMaxSocketNum > FD_SETSIZE");
		return NULL;
	}

	FD_ZERO(&(pstAnfMng->stReadFds));
	FD_ZERO(&(pstAnfMng->stWriteFds));
	FD_ZERO(&(pstAnfMng->stExceptFds));
	FD_ZERO(&(pstAnfMng->stReadTmpFds));
	FD_ZERO(&(pstAnfMng->stWriteTmpFds));
	FD_ZERO(&(pstAnfMng->stExceptFds));

#elif defined(ANF_KQUEUE)

#endif

	LOG("AnfInit pstAnfMng %p", pstAnfMng);
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

	LOG("fd %d iFlag %d", iFd, iFlag);
}

static inline void AnfGetSeletFds(AnfMng *pstAnfMng, int iFd, int iFlag)
{

	if (FD_ISSET(iFd, &(pstAnfMng->stReadTmpFds))) {
		iFlag |= ANF_FLAG_READ;
	}

	if (FD_ISSET(iFd, &(pstAnfMng->stWriteTmpFds))) {
		iFlag |= ANF_FLAG_WRITE;
	}
	if (FD_ISSET(iFd, &(pstAnfMng->stExceptTmpFds))) {
		iFlag |= ANF_FLAG_ERROR;
	}
	LOG("fd %d flag %d", iFd, iFlag);
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
			LOG("epoll_ctl failed");
			return -2;
		} else {
			LOG("warning: epoll_ctr add failed but mod success");
		}
	}

#elif define(ANF_SELECT)
	AnfSetSeletFds(pstAnfMng, iFd, iFlag);
#endif

	return 0;
}
