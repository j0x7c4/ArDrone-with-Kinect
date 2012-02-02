#ifndef _KINECT_H
#define _KINECT_H
//#define WIN32_LEAN_AND_MEAN
#include<Windows.h>
#include<MSR_NuiApi.h>
#include<cv.h>
#include<highgui.h>
#include <string>
#include "vp.h"
using namespace std;
#pragma comment (lib,"ws2_32.lib") 
#pragma comment(lib, "Winmm.lib")

class Vector {
public:
  Vector(){}
  Vector(float ix,float iy, float iz){
    x=ix,y=iy,z=iz;
  }
  float x,y,z;
};

typedef Vector JointVector;

typedef struct joint_position {
  float x,y, z;
}JointPosition;

static const CvScalar g_JointColorTable[20*4] = 
{
    155, 176, 169,0, // NUI_SKELETON_POSITION_HIP_CENTER
    155, 176, 169,0, // NUI_SKELETON_POSITION_SPINE
    29, 230, 168,0, // NUI_SKELETON_POSITION_SHOULDER_CENTER
    0, 0,   200,0, // NUI_SKELETON_POSITION_HEAD
    33,  84,  79,0, // NUI_SKELETON_POSITION_SHOULDER_LEFT
    42,  33,  84,0, // NUI_SKELETON_POSITION_ELBOW_LEFT
    0, 126, 255,0, // NUI_SKELETON_POSITION_WRIST_LEFT
    0,  86, 215,0, // NUI_SKELETON_POSITION_HAND_LEFT
    84,  79,  33,0, // NUI_SKELETON_POSITION_SHOULDER_RIGHT
    84,  33,  33,0, // NUI_SKELETON_POSITION_ELBOW_RIGHT
    243,  109, 77,0, // NUI_SKELETON_POSITION_WRIST_RIGHT
    243,   69, 37, 0,// NUI_SKELETON_POSITION_HAND_RIGHT
    243,  109, 77,0, // NUI_SKELETON_POSITION_HIP_LEFT
    84,  33,  69, 0,// NUI_SKELETON_POSITION_KNEE_LEFT
    122, 170, 229,0, // NUI_SKELETON_POSITION_ANKLE_LEFT
    0, 126, 255,0, // NUI_SKELETON_POSITION_FOOT_LEFT
    213, 165, 181,0, // NUI_SKELETON_POSITION_HIP_RIGHT
    76, 222,  71,0, // NUI_SKELETON_POSITION_KNEE_RIGHT
    156, 228, 245,0, // NUI_SKELETON_POSITION_ANKLE_RIGHT
    243,  109, 77 ,0// NUI_SKELETON_POSITION_FOOT_RIGHT
};







//Socket
WORD                    wVersionRequested;  
WSADATA                 wsaData;  
int                     err;
//创建数据报套接字   
SOCKET                  svr;
//创建地址  
SOCKADDR_IN             addrSvr;
 
static char             socket_buf[256]; 
int                     len;
static CRITICAL_SECTION socket_mutex;



//socket init
static DWORD WINAPI     Socket_ProcessThread( LPVOID pParam );
int                     SocketInit( );
void                    SocketUnInit ( );
void                    SendMessage ( );
int                     _Socket_ProcessThread( );

static DWORD WINAPI     Nui_ProcessThread(LPVOID pParam);
DWORD WINAPI            _Nui_ProcessThread();
INuiInstance*           m_pNuiInstance;
BSTR                    m_instanceId;

//opencv
IplImage* kinect_color_image;
IplImage* kinect_depth_image;
IplImage* kinect_skeleton_image;
// thread handling
HANDLE        m_hThNuiProcess;
HANDLE        m_hEvNuiProcessStop;
HANDLE        m_hThSocketProcess;

HANDLE        m_hSocketBufferReadyEvent;
HANDLE        m_hNextDepthFrameEvent;
HANDLE        m_hNextVideoFrameEvent;
HANDLE        m_hNextSkeletonEvent;
HANDLE        m_pDepthStreamHandle;
HANDLE        m_pVideoStreamHandle;
HFONT         m_hFontFPS;

int           m_PensTotal;
POINT         m_Points[NUI_SKELETON_POSITION_COUNT];
int           m_LastSkeletonFoundTime;
bool          m_bScreenBlanked;
int           m_FramesTotal;
int           m_LastFPStime;
int           m_LastFramesTotal;





HRESULT                 Kinect_Init();
void                    Kinect_UnInit( );
void                    Kinect_PoseRecognition ( NUI_SKELETON_DATA * pSkel );
void                    Nui_GotDepthAlert( );
void                    Nui_GotVideoAlert( );
void                    Nui_GotSkeletonAlert( );
void                    Nui_BlankSkeletonScreen( );
void                    Nui_DrawSkeleton( bool bBlank, NUI_SKELETON_DATA * pSkel, int WhichSkeletonColor );
void                    Nui_DrawSkeletonSegment( NUI_SKELETON_DATA * pSkel, int numJoints, ... );

RGBQUAD                 Nui_ShortToQuad_Depth( USHORT s );




#endif //_KINECT_H