#ifndef _ARDRONE_SOCKET_H
#define _ARDRONE_SOCKET_H
#include <Windows.h>
HANDLE                  m_hThSocketProcess;

//Socket
WORD                    wVersionRequested;  
WSADATA                 wsaData;  
int                     err;
//创建数据报套接字   
SOCKET                  cmd_receiver;
SOCKET                  img_sender;
//创建地址  
SOCKADDR_IN             cmd_receiver_addr;
SOCKADDR_IN             img_sender_addr;
SOCKADDR_IN             addrSvr;
SOCKADDR_IN             addrClient; 
char                    recvBuf[128];  
  
char                    sendBuf[128];  
  
char                    tempBuf[256];  
int                     len;
//socket init
static DWORD WINAPI     Socket_ProcessThread( LPVOID pParam );
int                     SocketInit( );
void                    SocketUnInit ( );
int                     _Socket_ProcessThread( );

#endif