#ifndef _KINECT_COMMAND_H
#define _KINECT_COMMAND_H
#pragma comment (lib,"ws2_32.lib") 
#include <Windows.h> 
#include <stdio.h> 
#include <string>
#include "vp.h"

using namespace std;
//Client
WORD wVersionRequested;  
WSADATA wsaData;  
int err;
//创建数据报套接字   
SOCKET svr;
//创建地址  
SOCKADDR_IN addrSvr;
char recv_buf[128];  
char send_buf[128];  
static string socket_buf; 
int len;
static CRITICAL_SECTION socket_mutex;

//socket init
static DWORD WINAPI  Socket_ProcessThread( LPVOID pParam );
int SocketInit( );
void SocketUnInit ( );
void SendMessage ( );
int _Socket_ProcessThread( );
#endif //_KINECT_COMMAND_H