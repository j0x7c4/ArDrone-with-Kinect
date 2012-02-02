//#include "socket.h"
//#include <stdio.h>
//#pragma comment (lib,"ws2_32.lib")
//
//
//int SocketInit( )  
//{  
//
//  wVersionRequested = MAKEWORD( 1, 1 ); 
//  err = WSAStartup( wVersionRequested, &wsaData );  
//  if ( err != 0 ) {  
//    /* Tell the user that we could not find a usable */ 
//    /* WinSock DLL.                                  */ 
//    return 1;  
//  }  
//  /* Confirm that the WinSock DLL supports 2.2.*/ 
//  /* Note that if the DLL supports versions greater    */ 
//  /* than 2.2 in addition to 2.2, it will still return */ 
//  /* 2.2 in wVersion since that is the version we      */ 
//  /* requested.                                        */ 
//  if ( LOBYTE( wsaData.wVersion ) != 1 ||  
//    HIBYTE( wsaData.wVersion ) != 1 ) {  
//      /* Tell the user that we could not find a usable */ 
//      /* WinSock DLL.                                  */ 
//      WSACleanup( );  
//      return 1;   
//  }  
//
//  /* The WinSock DLL is acceptable. Proceed. */ 
//  //创建服务器套接字  
//  svr = socket(AF_INET,SOCK_DGRAM,0);  
//  //创建地址   
//  addr.sin_family = AF_INET;  
//  addr.sin_port = htons(6000);
//  addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
//  len = sizeof(struct sockaddr);
//  bind(svr,(struct sockaddr*)&addr,len);
//
//    
//  return 0;
//}
//
//void SocketUnInit ( ){
//  closesocket(svr);  
//  WSACleanup();  
//}
//
//static DWORD WINAPI Socket_ProcessThread( LPVOID pParam ) {
//  HRESULT res;
//  printf("Socket Thread Starts...\n");
//  res = _Socket_ProcessThread( );
//  printf("Socket Thread Ends...\n");
//  return res;
//}
//
//int _Socket_ProcessThread( ) {
//  char* ipClient;
//  int res = SocketInit ( );
//  if ( res ) return res;
//
//  while(1)  
//  {  
//  
//      //接收数据  
//  
//      recvfrom(svr,recvBuf,128,0,(struct sockaddr*)&addrClient,&len);  
//  
//      ipClient = inet_ntoa(addrClient.sin_addr);  
//  
//      sprintf(tempBuf,"%s said: %s/n",ipClient,recvBuf);  
//  
//      printf("%s",tempBuf);  
//  
//      gets(sendBuf);  
//  
//      //发送数据  
//  
//      sendto(svr,sendBuf,strlen(sendBuf)+1,0,(struct sockaddr*)&addrClient,len);  
//  
//  }  
//  SocketUnInit();
//}
