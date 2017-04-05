# AsynNetFramework
## Introduction

一个简单的异步框架。
- 适合简单的echo类型服务器。
- 服务器监听端口，框架负责收发包，开发人员只需编写响应的回调函数（收请求、处理请求等）即可。
- 支持tcp、udp
- 支持select、epoll

a Asynchronous Network framework

this framework be fit to a echo server

exmaple:

request :client send request to server
respond :server listen and recv client's req, then process it, finally respond to client 

support tcp udp
support select epoll

## example

详情请看：
./example/test_client.c and server.c
