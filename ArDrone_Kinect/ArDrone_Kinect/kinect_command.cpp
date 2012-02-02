#include "kinect_command.h"
#include <string.h>
#include <iostream>
int SocketInit( )  
{  
    
    wVersionRequested = MAKEWORD( 1, 1 ); 
    err = WSAStartup( wVersionRequested, &wsaData );  
    if ( err != 0 ) {  
        /* Tell the user that we could not find a usable */ 
        /* WinSock DLL.                                  */ 
        return 1;  
    }  
    /* Confirm that the WinSock DLL supports 2.2.*/ 
    /* Note that if the DLL supports versions greater    */ 
    /* than 2.2 in addition to 2.2, it will still return */ 
    /* 2.2 in wVersion since that is the version we      */ 
    /* requested.                                        */ 
    if ( LOBYTE( wsaData.wVersion ) != 1 ||  
        HIBYTE( wsaData.wVersion ) != 1 ) {  
        /* Tell the user that we could not find a usable */ 
        /* WinSock DLL.                                  */ 
        WSACleanup( );  
        return 1;   
    }  

    /* The WinSock DLL is acceptable. Proceed. */ 
    //创建服务器套接字  
    svr = socket(AF_INET,SOCK_DGRAM,0);  
    //创建地址   
    addrSvr.sin_family = AF_INET;  
    addrSvr.sin_port = htons(6000);  
    addrSvr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");  

    len = sizeof(sockaddr);  
    return 0;
}

void SocketUnInit ( ){
  closesocket(svr);  
  WSACleanup();  
}

static DWORD WINAPI Socket_ProcessThread( LPVOID pParam ) {
  return _Socket_ProcessThread( );
}

int _Socket_ProcessThread( ) {
  int res = SocketInit ( );
  if ( !res ) return res;

  while ( 1 ) {
    mutex_lock(&socket_mutex);
    if ( socket_buf=="takeoff" || socket_buf=="landing" || socket_buf=="hovering" || socket_buf.find("flying") ) {
      sendto(svr,socket_buf.c_str(),strlen(socket_buf.c_str())+1,0,(sockaddr*)&addrSvr,len);
    }
    mutex_unlock(&socket_mutex);
  }

  SocketUnInit();
}

