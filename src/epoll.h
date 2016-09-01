#ifndef _EPOLL_H_
#define _EPOLL_H_

#ifdef __cplusplus
extern "C" {
#endif 

int epoll_create(int);

typedef union epoll_data
{
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;

struct epoll_event
{
	uint32_t events;  /* Epoll events */
	epoll_data_t data;    /* User data variable */
} __attribute__ ((__packed__));

#define EPOLL_CTL_ADD 0x1
#define EPOLL_CTL_MOD 0x2
#define EPOLL_CTL_DEL 0x4

int epoll_ctl(int, int, int, struct epoll_event *);
int epoll_wait(int ,struct epoll_event *, int, int);

#ifdef __cplusplus
}
#endif 


#endif
