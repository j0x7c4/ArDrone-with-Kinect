#include "kinect.h"
#pragma warning( disable : 4996 ) // disable deprecation warning 

//extern "C" {
//#include <stdlib.h>
//
//#include <VP_Os/vp_os_malloc.h>
//#include <VP_Os/vp_os_print.h>
//#include <VP_Api/vp_api_thread_helper.h>
//
//#include <VP_Com/vp_com.h>
//
//#include <ardrone_tool/ardrone_tool.h>
//#include <ardrone_tool/ardrone_time.h>
//#include <ardrone_tool/Control/ardrone_control.h>
//#include <ardrone_tool/Control/ardrone_control_ack.h>
//#include <ardrone_tool/Navdata/ardrone_navdata_client.h>
//#include <ardrone_tool/UI/ardrone_input.h>
//#include <ardrone_tool/Com/config_com.h>
//}
#pragma comment(lib, "Winmm.lib")
HRESULT Nui_Init()
{
    HRESULT                hr;
    RECT                rc;

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
    //cvNamedWindow("Kinect depth image",CV_WINDOW_AUTOSIZE);
    //cvNamedWindow("Kinect color image",CV_WINDOW_AUTOSIZE);
    //cvNamedWindow("Kinect skeleton image",CV_WINDOW_AUTOSIZE);
    kinect_color_image = cvCreateImage(cvSize(640,480),IPL_DEPTH_8U,3);
    kinect_depth_image = cvCreateImage(cvSize(320,240),IPL_DEPTH_8U,3);
    kinect_skeleton_image = cvCreateImage(cvSize(320,240),IPL_DEPTH_8U,3);
    // Start the Nui processing thread
    m_hEvNuiProcessStop=CreateEvent(NULL,FALSE,FALSE,NULL);
    m_hThNuiProcess=CreateThread(NULL,0,Nui_ProcessThread,NULL,0,0);

    return hr;
}

void Nui_UnInit( )
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
    return _Nui_ProcessThread();
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
/*
void Nui_DrawSkeletonSegment( NUI_SKELETON_DATA * pSkel, int numJoints, ... )
{
    va_list vl;
    va_start(vl,numJoints);
    vector<CvPoint*> segmentPositions;
    vector<CvPoint> pt;
    //CvPoint segmentPositions[1][NUI_SKELETON_POSITION_COUNT];
    int segmentPositionsCount = 0;
    vector<int> polylinePointCounts;
    //int polylinePointCounts[NUI_SKELETON_POSITION_COUNT];
    int numPolylines = 0;
    int currentPointCount = 0;

    // Note the loop condition: We intentionally run one iteration beyond the
    // last element in the joint list, so we can properly end the final polyline.

    for (int iJoint = 0; iJoint <= numJoints; iJoint++)
    {
        if (iJoint < numJoints)
        {
            NUI_SKELETON_POSITION_INDEX jointIndex = va_arg(vl,NUI_SKELETON_POSITION_INDEX);

            if (pSkel->eSkeletonPositionTrackingState[jointIndex] != NUI_SKELETON_POSITION_NOT_TRACKED)
            {
                // This joint is tracked: add it to the array of segment positions.
              CvPoint t;
              t.x = m_Points[jointIndex].x;
              t.y = m_Points[jointIndex].y;
              pt.push_back(t);
              currentPointCount++;

                // Fully processed the current joint; move on to the next one

              continue;
            }
        }

        // If we fall through to here, we're either beyond the last joint, or
        // the current joint is not tracked: end the current polyline here.

        if (currentPointCount > 1)
        {
            // Current polyline already has at least two points: save the count.
          segmentPositions.push_back(&pt[0]);
          polylinePointCounts.push_back(currentPointCount);
        }
        else if (currentPointCount == 1)
        {
            // Current polyline has only one point: ignore it.

          pt.clear();
        }
        currentPointCount = 0;
    }

#ifdef _DEBUG
    // We should end up with no more points in segmentPositions than the
    // original number of joints.

    assert(segmentPositionsCount <= numJoints);

    int totalPointCount = 0;
    for (int i = 0; i < numPolylines; i++)
    {
        // Each polyline should contain at least two points.
    
        assert(polylinePointCounts[i] > 1);

        totalPointCount += polylinePointCounts[i];
    }

    // Total number of points in all polylines should be the same as number
    // of points in segmentPositions.
    
    assert(totalPointCount == segmentPositionsCount);
#endif

    if (numPolylines > 0)
    {
      CvScalar color={0,255,0};
      
      cvPolyLine(kinect_skeleton_image,&segmentPositions[0],&polylinePointCounts[0],numPolylines,0,color,4);
     //   PolyPolyline(m_SkeletonDC, segmentPositions, polylinePointCounts, numPolylines);
    }

    va_end(vl);
}
*/
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
    /*
    Nui_DrawSkeletonSegment(pSkel,4,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD);
    Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);
    Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);
    Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);
    Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);
    */
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
            Nui_DrawSkeleton( bBlank, &SkeletonFrame.SkeletonData[i], i );
            bBlank = false;
        }
    }
    cvShowImage("Kinect skeleton image",kinect_skeleton_image);
    cvWaitKey(10);
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



int main () {
  
  test_drone_connection();
  return 0;
}
