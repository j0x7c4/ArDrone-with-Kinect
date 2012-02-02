#include "kinect.h"
#define PI acos(-1.0)
HRESULT Kinect_Init()
{
  HRESULT   hr;


  if (!m_pNuiInstance)
  {
    HRESULT hr = MSR_NuiCreateInstanceByIndex(0, &m_pNuiInstance);

    if (FAILED(hr))
    {
      return hr;
    }

    if (m_instanceId)
    {
      ::SysFreeString(m_instanceId);
    }

    m_instanceId = m_pNuiInstance->NuiInstanceName();
  }

  m_hNextDepthFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
  m_hNextVideoFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
  m_hNextSkeletonEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
  m_hSocketBufferReadyEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

  DWORD nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON |  NUI_INITIALIZE_FLAG_USES_COLOR;
  hr = m_pNuiInstance->NuiInitialize(nuiFlags);
  if (E_NUI_SKELETAL_ENGINE_BUSY == hr)
  {
    nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH |  NUI_INITIALIZE_FLAG_USES_COLOR;
    hr = m_pNuiInstance->NuiInitialize(nuiFlags);
  }

  if( FAILED( hr ) )
  {
    fprintf(stderr,"Failed on initialization!\n");
    return hr;
  }

  if (HasSkeletalEngine(m_pNuiInstance))
  {
    hr = m_pNuiInstance->NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, 0 );
    if( FAILED( hr ) )
    {
      fprintf(stderr,"Skeleton Tracking is disable!\n");
      return hr;
    }
  }

  hr = m_pNuiInstance->NuiImageStreamOpen(
    NUI_IMAGE_TYPE_COLOR,
    NUI_IMAGE_RESOLUTION_640x480,
    0,
    2,
    m_hNextVideoFrameEvent,
    &m_pVideoStreamHandle );
  if( FAILED( hr ) )
  {
    fprintf(stderr,"Failed on opening color stream!\n");
    return hr;
  }

  hr = m_pNuiInstance->NuiImageStreamOpen(
    HasSkeletalEngine(m_pNuiInstance) ? NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX : NUI_IMAGE_TYPE_DEPTH,
    NUI_IMAGE_RESOLUTION_320x240,
    0,
    2,
    m_hNextDepthFrameEvent,
    &m_pDepthStreamHandle );
  if( FAILED( hr ) )
  {
    fprintf(stderr,"Failed on opening depth stream!\n");
    return hr;
  }
  // Create Image
  kinect_color_image = cvCreateImage(cvSize(640,480),IPL_DEPTH_8U,3);
  kinect_depth_image = cvCreateImage(cvSize(320,240),IPL_DEPTH_8U,3);
  kinect_skeleton_image = cvCreateImage(cvSize(320,240),IPL_DEPTH_8U,3);
  //set mutex
  //MutexInit(&socket_mutex);
  // Start the Nui processing thread
  m_hEvNuiProcessStop=CreateEvent(NULL,FALSE,FALSE,NULL);
  m_hThNuiProcess=CreateThread(NULL,0,Nui_ProcessThread,NULL,0,0);
  m_hThSocketProcess = CreateThread(NULL,0,Socket_ProcessThread,NULL,0,0);

  return hr;
}

void Kinect_UnInit( )
{
  // Stop the Nui processing thread
  if(m_hEvNuiProcessStop!=NULL)
  {
    // Signal the thread
    SetEvent(m_hEvNuiProcessStop);

    // Wait for thread to stop
    if(m_hThNuiProcess!=NULL)
    {
      WaitForSingleObject(m_hThNuiProcess,INFINITE);
      CloseHandle(m_hThNuiProcess);
    }
    if ( m_hThSocketProcess!=NULL ) {
      WaitForSingleObject(m_hThSocketProcess,INFINITE);
      CloseHandle(m_hThSocketProcess);
    }
    // Release Mutex
    //MUtexUnInit(&socket_mutex);

    CloseHandle(m_hEvNuiProcessStop);
  }

  if (m_pNuiInstance)
  {
    m_pNuiInstance->NuiShutdown( );
  }
  if( m_hNextSkeletonEvent && ( m_hNextSkeletonEvent != INVALID_HANDLE_VALUE ) )
  {
    CloseHandle( m_hNextSkeletonEvent );
    m_hNextSkeletonEvent = NULL;
  }
  if( m_hNextDepthFrameEvent && ( m_hNextDepthFrameEvent != INVALID_HANDLE_VALUE ) )
  {
    CloseHandle( m_hNextDepthFrameEvent );
    m_hNextDepthFrameEvent = NULL;
  }
  if( m_hNextVideoFrameEvent && ( m_hNextVideoFrameEvent != INVALID_HANDLE_VALUE ) )
  {
    CloseHandle( m_hNextVideoFrameEvent );
    m_hNextVideoFrameEvent = NULL;
  }
}

static DWORD WINAPI Nui_ProcessThread(LPVOID pParam)
{
  HRESULT res;
  printf("Kinect Thread Starts...\n");
  res = _Nui_ProcessThread();
  printf("Kinect Thread Ends...\n");
  return res;
}

DWORD WINAPI _Nui_ProcessThread()
{
  HANDLE                hEvents[4];
  int                    nEventIdx,t,dt;

  // Configure events to be listened on
  hEvents[0]= m_hEvNuiProcessStop;
  hEvents[1]= m_hNextDepthFrameEvent;
  hEvents[2]= m_hNextVideoFrameEvent;
  hEvents[3]= m_hNextSkeletonEvent;

#pragma warning(push)
#pragma warning(disable: 4127) // conditional expression is constant

  // Main thread loop
  while(1)
  {
    // Wait for an event to be signalled
    nEventIdx=WaitForMultipleObjects(sizeof(hEvents)/sizeof(hEvents[0]),hEvents,FALSE,100);

    // If the stop event, stop looping and exit
    if(nEventIdx==0)
      break;            

    // Perform FPS processing
    t = timeGetTime( );
    if( m_LastFPStime == -1 )
    {
      m_LastFPStime = t;
      m_LastFramesTotal = m_FramesTotal;
    }
    dt = t - m_LastFPStime;
    if( dt > 1000 )
    {
      m_LastFPStime = t;
      int FrameDelta = m_FramesTotal - m_LastFramesTotal;
      m_LastFramesTotal = m_FramesTotal;
      //::PostMessageW(m_hWnd, WM_USER_UPDATE_FPS, IDC_FPS, FrameDelta);
    }

    // Perform skeletal panel blanking
    if( m_LastSkeletonFoundTime == -1 )
      m_LastSkeletonFoundTime = t;
    dt = t - m_LastSkeletonFoundTime;
    if( dt > 250 )
    {
      if( !m_bScreenBlanked )
      {
        Nui_BlankSkeletonScreen( );
        m_bScreenBlanked = true;
      }
    }

    // Process signal events
    switch(nEventIdx)
    {
    case 1:
      Nui_GotDepthAlert();
      m_FramesTotal++;
      break;

    case 2:
      Nui_GotVideoAlert();
      break;

    case 3:
      Nui_GotSkeletonAlert( );
      break;
    }
  }
#pragma warning(pop)

  return (0);
}

void Nui_GotVideoAlert( )
{
  const NUI_IMAGE_FRAME * pImageFrame = NULL;

  HRESULT hr = m_pNuiInstance->NuiImageStreamGetNextFrame(
    m_pVideoStreamHandle,
    0,
    &pImageFrame );
  if( FAILED( hr ) )
  {
    return;
  }

  INuiFrameTexture * pTexture = pImageFrame->pFrameTexture;
  NUI_LOCKED_RECT LockedRect;
  pTexture->LockRect( 0, &LockedRect, NULL, 0 );
  /*
  if( LockedRect.Pitch != 0 )
  {
    BYTE * pBuffer = (BYTE*) LockedRect.pBits;

    cvZero(kinect_color_image);  
    for (int i=0; i<480; i++)  
    {  
      uchar* ptr = (uchar*)(kinect_color_image->imageData+i*kinect_color_image->widthStep);  
      BYTE * pBuffer = (BYTE*)(LockedRect.pBits)+i*LockedRect.Pitch;//每个字节代表一个颜色信息，直接使用BYTE  
      for (int j=0; j<640; j++)  
      {  
        ptr[3*j] = pBuffer[4*j];//内部数据是4个字节，0-1-2是BGR，第4个现在未使用  
        ptr[3*j+1] = pBuffer[4*j+1];  
        ptr[3*j+2] = pBuffer[4*j+2];  
      }  
    }  

    cvShowImage("Kinect color image", kinect_color_image);//显示图像
    cvWaitKey(10);
  }
  else
  {
    OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
  }
  */
  m_pNuiInstance->NuiImageStreamReleaseFrame( m_pVideoStreamHandle, pImageFrame );
}


void Nui_GotDepthAlert( )
{
  const NUI_IMAGE_FRAME * pImageFrame = NULL;

  HRESULT hr = m_pNuiInstance->NuiImageStreamGetNextFrame(
    m_pDepthStreamHandle,
    0,
    &pImageFrame );

  if( FAILED( hr ) )
  {
    return;
  }

  INuiFrameTexture * pTexture = pImageFrame->pFrameTexture;
  NUI_LOCKED_RECT LockedRect;
  pTexture->LockRect( 0, &LockedRect, NULL, 0 );
  if( LockedRect.Pitch != 0 )
  {
    BYTE * pBuffer = (BYTE*) LockedRect.pBits;
    BYTE * rgbrun = (BYTE*)kinect_depth_image->imageData;
    // draw the bits to the bitmap
    USHORT * pBufferRun = (USHORT*) pBuffer;

    for( int y = 0 ; y < 240 ; y++ )
    {
      for( int x = 0 ; x < 320 ; x++ )
      {
        RGBQUAD quad = Nui_ShortToQuad_Depth( *pBufferRun );
        pBufferRun++;
        *rgbrun = quad.rgbBlue;
        rgbrun++;
        *rgbrun = quad.rgbGreen;
        rgbrun++;
        *rgbrun = quad.rgbRed;
        rgbrun++;
      }
    }

  }
  else
  {
    OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
  }

  m_pNuiInstance->NuiImageStreamReleaseFrame( m_pDepthStreamHandle, pImageFrame );

  cvShowImage("Kinect depth image",kinect_depth_image);

  cvWaitKey(10);
}



void Nui_BlankSkeletonScreen()
{
  cvZero(kinect_skeleton_image); 
}

void Nui_DrawSkeleton( bool bBlank, NUI_SKELETON_DATA * pSkel, int WhichSkeletonColor )
{
  if( bBlank )
  {
    Nui_BlankSkeletonScreen();
  }

  int scaleX = 320; //scaling up to image coordinates
  int scaleY = 240;
  float fx=0,fy=0;
  int i;
  for (i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
  {
    NuiTransformSkeletonToDepthImageF( pSkel->SkeletonPositions[i], &fx, &fy );
    m_Points[i].x = (int) ( fx * scaleX + 0.5f );
    m_Points[i].y = (int) ( fy * scaleY + 0.5f );
  }

  // Draw the joints in a different color
  for (i = 0; i < NUI_SKELETON_POSITION_COUNT ; i++)
  {
    if (pSkel->eSkeletonPositionTrackingState[i] != NUI_SKELETON_POSITION_NOT_TRACKED)
    {
      CvPoint p;
      p.x = m_Points[i].x;
      p.y = m_Points[i].y;
      cvLine(kinect_skeleton_image,p,p,g_JointColorTable[i],7);
    }
  }

  return;

}

void Nui_GotSkeletonAlert( )
{
  NUI_SKELETON_FRAME SkeletonFrame;

  bool bFoundSkeleton = false;

  if( SUCCEEDED(m_pNuiInstance->NuiSkeletonGetNextFrame( 0, &SkeletonFrame )) )
  {
    for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
    {
      if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
      {
        bFoundSkeleton = true;
      }
    }
  }

  // no skeletons!
  //
  if( !bFoundSkeleton )
  {
    return;
  }

  // smooth out the skeleton data
  m_pNuiInstance->NuiTransformSmooth(&SkeletonFrame,NULL);

  // we found a skeleton, re-start the timer
  m_bScreenBlanked = false;
  m_LastSkeletonFoundTime = -1;

  // draw each skeleton color according to the slot within they are found.
  //
  bool bBlank = true;
  for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
  {
    // Show skeleton only if it is tracked, and the center-shoulder joint is at least inferred.

    if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED &&
      SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER] != NUI_SKELETON_POSITION_NOT_TRACKED)
    {
      Kinect_PoseRecognition(&SkeletonFrame.SkeletonData[i]);
      //Nui_DrawSkeleton( bBlank, &SkeletonFrame.SkeletonData[i], i );
      //bBlank = false;
    }
  }
  //cvShowImage("Kinect skeleton image",kinect_skeleton_image);
  //cvWaitKey(10);
}

void CreateJointVector ( JointPosition* b, JointPosition* e , JointVector* v ) {
  float x, y, z,len;
  x = e->x - b->x;
  y = e->y - b->y;
  z = e->z - b->z;
  v->x = x;
  v->y = y;
  v->z = z;
}

void GetJointPosition ( float x, float y, float z, JointPosition* p ){
  p->x = x;
  p->y = y;
  p->z = z;
}

float GetRad ( Vector v1 , Vector v2 ) {
  double len1 = sqrt(v1.x*v1.x+v1.y*v1.y+v1.z*v1.z);
  double len2 = sqrt(v2.x*v2.x+v2.y*v2.y+v2.z*v2.z);
  return acos((v1.x*v2.x+v1.y*v2.y+v1.z*v2.z)/(len1*len2));
}
void Kinect_PoseRecognition ( NUI_SKELETON_DATA * pSkel ) {
  int roll,pitch,yaw,gaz;
  float rad;
  int hovering = 1;
  JointPosition hip, shoulder_center, left_shoulder, right_shoulder, right_hand, left_hand, head;
  JointVector v_shoulder_center , v_shoulder_left , v_shoulder_right , v_hand_left, v_hand_right,v_head;
  /*
  Vector v_x, v_y, v_z;
  v_x.x=1,v_x.y=0, v_x.z=0;
  v_y.x=0,v_y.y=1, v_y.z=0;
  v_x.z=0,v_x.z=0, v_z.z=1;
  */
  //Get Skeleton position
  GetJointPosition(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HEAD].x,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HEAD].y,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HEAD].z,&head);
  GetJointPosition(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].x,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].y,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].z,&hip);
  GetJointPosition(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z,&shoulder_center);
  GetJointPosition(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z,&left_shoulder);
  GetJointPosition(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z,&right_shoulder);
  GetJointPosition(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT].x,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT].y,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT].z,&left_hand);
  GetJointPosition(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].x,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].y,
                  pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].z,&right_hand);
  //Create Joint Vector
  CreateJointVector(&hip,&head,&v_head);
  CreateJointVector(&hip,&shoulder_center,&v_shoulder_center);
  CreateJointVector(&hip,&left_shoulder,&v_shoulder_left);
  CreateJointVector(&hip,&right_shoulder,&v_shoulder_right);
  CreateJointVector(&hip,&left_hand,&v_hand_left);
  CreateJointVector(&hip,&right_hand,&v_hand_right);
  
  gaz = pitch = roll = yaw = 0;
  //set roll,pitch,yaw
  
  
  //set pitch
  rad = GetRad(Vector(0,v_head.y,v_head.z),Vector(0,0,1.0)); //和z轴夹角 in YZ Plane
  //printf("%lf\n",rad/PI);
  //return ;
  if (rad>PI/2+PI/12) { //forward
    pitch = 1;
    hovering = 0;
  }
  else if ( rad<PI/2-PI/12 ) { //backward
    pitch = -1;
    hovering = 0;
  }
  
  //set roll
  rad = GetRad(Vector(v_head.x,v_head.y,0),Vector(1.0,0,0)); //和x轴夹角 in XY plane
  if ( rad>2*PI/3 ) { //left
    roll = -1;
    hovering = 0;
  }
  else if ( rad<PI/3 ) { //right
    roll = 1;
    hovering = 0;
  }

  //set yaw
  rad = GetRad(Vector(v_hand_left.x,0,v_hand_left.z),Vector(v_hand_right.x,0,v_hand_right.z)); //left_hand 与 right_hand的夹角 in XZ Plane
  if ( rad<3*PI/4+PI/12 && left_hand.z<head.z && right_hand.z < head.z) {
    if ( left_hand.z < right_hand.z ) {
      yaw = 1;
    }
    else if ( right_hand.z < left_hand.z ) {
      yaw = -1;
    }
    hovering = 0;
  }
  
  //set gaz
  if ( left_hand.y< hip.y && right_hand.y< hip.y) { //down
    gaz = -1;
    hovering = 0;
  }
  else if ( left_hand.y> head.y && right_hand.y> head.y ) { //up
    gaz = 1;
    hovering = 0;
  }
  
  sprintf(socket_buf,"flying %d %d %d %d",pitch,roll,yaw,gaz);
  //printf("%s\n",socket_buf);
  SetEvent(m_hSocketBufferReadyEvent);
}


RGBQUAD Nui_ShortToQuad_Depth( USHORT s )
{
  bool hasPlayerData = HasSkeletalEngine(m_pNuiInstance);
  USHORT RealDepth = hasPlayerData ? (s & 0xfff8) >> 3 : s & 0xffff;
  USHORT Player = hasPlayerData ? s & 7 : 0;

  // transform 13-bit depth information into an 8-bit intensity appropriate
  // for display (we disregard information in most significant bit)
  BYTE l = 255 - (BYTE)(256*RealDepth/0x0fff);

  RGBQUAD q;
  q.rgbRed = q.rgbBlue = q.rgbGreen = 0;

  switch( Player )
  {
  case 0:
    q.rgbRed = l / 2;
    q.rgbBlue = l / 2;
    q.rgbGreen = l / 2;
    break;
  case 1:
    q.rgbRed = l;
    break;
  case 2:
    q.rgbGreen = l;
    break;
  case 3:
    q.rgbRed = l / 4;
    q.rgbGreen = l;
    q.rgbBlue = l;
    break;
  case 4:
    q.rgbRed = l;
    q.rgbGreen = l;
    q.rgbBlue = l / 4;
    break;
  case 5:
    q.rgbRed = l;
    q.rgbGreen = l / 4;
    q.rgbBlue = l;
    break;
  case 6:
    q.rgbRed = l / 2;
    q.rgbGreen = l / 2;
    q.rgbBlue = l;
    break;
  case 7:
    q.rgbRed = 255 - ( l / 2 );
    q.rgbGreen = 255 - ( l / 2 );
    q.rgbBlue = 255 - ( l / 2 );
  }

  return q;
}

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
  addrSvr.sin_port = htons(2001);  
  addrSvr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");  

  len = sizeof(sockaddr);  
  return 0;
}

void SocketUnInit ( ){
  closesocket(svr);  
  WSACleanup();  
}

static DWORD WINAPI Socket_ProcessThread( LPVOID pParam ) {
  HRESULT res;
  printf("Socket Thread Starts...\n");
  res = _Socket_ProcessThread( );
  printf("Socket Thread Ends...\n");
  return res;
}

int _Socket_ProcessThread( ) {
  int res = SocketInit ( );
  if ( res ) return res;

  while ( 1 ) {
    //Wait for kinect write socket buffer
    //printf("Waiting for message...\n");
    HRESULT res = WaitForSingleObject(m_hSocketBufferReadyEvent,INFINITE);
    if ( res == WAIT_FAILED ) break;
    if ( res == WAIT_OBJECT_0 ) {
      if ( socket_buf[0]=='f') {
        //printf("%s\n",socket_buf);
        sendto(svr,socket_buf,strlen(socket_buf)+1,0,(sockaddr*)&addrSvr,len);
      }
    }
    ResetEvent(m_hSocketBufferReadyEvent);

  }

  SocketUnInit();
}




int main () {

  Kinect_Init();
  while (1);
  Kinect_UnInit();
  return 0;
}
