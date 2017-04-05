# AsynNetFramework
## Introduction

一个简单的异步框架。
- 适合简单的echo类型服务器(一问一答式)。
- 服务器监听端口，框架负责收发包，开发人员只需编写相应的回调函数（收请求、处理请求等）即可。
- 支持tcp、udp
- 支持select、epoll

客户端回调函数：
```C
/* 从包头获取预期长度并回填piPkgLen */
int (*HandlePkgHead) (SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iBytesRecved, int *piPkgLen);

/* 收包成功后处理函数(tcp) */
int (*HandlePkg) (SocketClientDef *pstScd, void *pUserInfo, void *pPkg, int iPkgLen);

/* 作为服务器，有新客户端连接时回调 */
int (*HandleAccept) (SocketClientDef *pstScd, void *pUserInfo);

/* 作为客户端，连接成功时回调(主要做组包操作和SendTcpPkg)*/
int (*HandleConnect) (SocketClientDef *pstScd, void *pUserInfo);

/* 循环操作(千万不要做耗时的事情，会阻塞进程)*/
int (*HandleLoop) ();

/* 收包成功后的处理函数(udp) */
int (*HandleUdpPkg) (SocketClientDef *pstScd, void *pUserInfo, int iUdpName, void *pPkg, int iPkgLen);

/* 关闭连接时回调 */
int (*HandleClose) (SocketClientDef *pstScd, void *pUserInfo);
```
a Esay Asynchronous Network framework
It is suitable for as a echo server
support tcp/udp
support select/epoll

exmaple:
request :client send request to server
respond :server listen and recv client's req, then process it, finally respond to client 


## example

详情请看：
./example/test_client.c and server.c
