#include <iostream>
#include <algorithm>
#include "opencv2\highgui\highgui.hpp"
#include "opencv2\core\core.hpp"
#include "opencv2\imgproc\imgproc.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include <Windows.h>
#include <string>
#include "mmSystem.h"
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"ws2_32.lib")


#define motion_t 80
#define rad 57.29578

using namespace cv;
using namespace std;

HANDLE        m_hThSocketProcess;
HANDLE        hSoundPlayerThread;
HANDLE        hPlaySoundEvent;
int sound_flag;
Mat frame_show(240,320,CV_8UC3);
//Socket
WORD                    wVersionRequested;  
WSADATA                 wsaData;  
int                     err;
//创建数据报套接字   
SOCKET                  svr;
SOCKET                  cmd_sender;
//创建地址  
SOCKADDR_IN             addrSvr;
SOCKADDR_IN             addr_cmd_sender; 
static string           socket_buf; 
int                     len;
//socket init
static DWORD WINAPI     Socket_ProcessThread( LPVOID pParam );
int                     SocketInit( );
void                    SocketUnInit ( );
void                    SendMessage ( );
int                     _Socket_ProcessThread( );

int ints[240][320];
int intb[240][320];
double gradientM[240][320][9];
int mainGdirection[4][3]={6,7,8,
						  5,7,0,
						  4,3,0,
						  4,7,0					
						};
int secondGdirection[4][3]={5,6,7,
						  4,6,8,
						  0,2,1,
						  5,2,1							
						};

void mydetectAndDraw( Mat& frame){
  static int run_count = 0;
		Mat frameCopy,
			hsv_image,
			gray_image,
			edge_image,
			skin_image,
			black_image;
		int tmpsumx =0;
		int tmpsumy =0;
    int tmpaa=0;
		int nncount=0;
												

		 skin_image.create(frame.rows,frame.cols,CV_8UC1) ;
		 black_image.create(frame.rows,frame.cols,CV_8UC1) ;
		//pyrUp(src_frame,frame, Size( src_frame.cols*2, src_frame.rows*2 ) );
		
		frameCopy=frame.clone();
		cvtColor(frame, hsv_image, CV_BGR2HSV);		
		//imshow("Webcam0",frame);
		cvtColor(frame, gray_image, CV_BGR2GRAY);	
		//imshow("gray",gray_image);
		
		Canny(gray_image,edge_image,50,150,3);
		//threshold(gray_image,otsu_image,30,255,THRESH_OTSU);
		//imshow("gray",gray_image);
		//imshow("Webcam0",edge_image);	
		//imshow("Webcam0",hsv_image);

	for(int i=0; i < frame.rows;i++){
					for(int j=0; j < frame.cols;j++){
						int h_value;
						int s_value;
						int v_value;
						h_value = hsv_image.at<Vec3b>(i,j)[0];
						s_value = hsv_image.at<Vec3b>(i,j)[1];
						v_value = hsv_image.at<Vec3b>(i,j)[2];
						
						if( s_value>78 &&s_value <193 ){
							if(h_value>0 &&h_value<40 ||h_value>170 &&h_value<=180){							
								skin_image.at<uchar>(i,j) =255;	
							}
						}
						else{
							skin_image.at<uchar>(i,j) =0;
						}

						if(gray_image.at<uchar>(i,j) >90 ||s_value>100){
								black_image.at<uchar>(i,j) =0;								
							}
							else{
								/*
								if(abs(frame.at<Vec3b>(i,j)[0]-frame.at<Vec3b>(i,j)[1]) <10&&
								abs(frame.at<Vec3b>(i,j)[0]-frame.at<Vec3b>(i,j)[2]) <10&&
								abs(frame.at<Vec3b>(i,j)[1]-frame.at<Vec3b>(i,j)[2]) <10)
								*/
								black_image.at<uchar>(i,j) =255;
						}
					}
			}

			erode(skin_image, skin_image,1);
			dilate(skin_image, skin_image,1);
			dilate(skin_image, skin_image,1);
			erode(black_image, black_image,1);
			dilate(black_image, black_image,1);
			memset(gradientM,0,320*240*9*8);

			for(int i=1; i < frame.rows-1;i++){
				for(int j=1; j < frame.cols-1;j++){
					double gx = gray_image.at<uchar>(i,j+1) -gray_image.at<uchar>(i,j-1);
					double gy = gray_image.at<uchar>(i+1,j) -gray_image.at<uchar>(i-1,j);
					double g_mag = sqrt(gx*gx+gy*gy);
					double g_angle = atan2(gy,gx);
					if(g_mag<20)
						g_mag=0;
					g_angle *=rad;
					if(g_angle<0)
						g_angle+=360;
					g_angle /=40;
					double intersect= g_angle -(int)g_angle;
					gradientM[i][j][int(g_angle)] =g_mag*(1-intersect);
					gradientM[i][j][(int(g_angle)+1)%9] =g_mag*intersect;								
				}
			}

			for(int i=0; i < frame.rows;i++){
					for(int j=0; j < frame.cols;j++){
						if (skin_image.at<uchar>(i,j) ==255)
							ints[i][j]=1;
						else
							ints[i][j]=0;
						if (black_image.at<uchar>(i,j) ==255)
							intb[i][j]=1;
						else
							intb[i][j]=0;
					}
			}
			
			//imshow("skin",skin_image);
			//imshow("black",black_image);


		
			for(int i = 1; i < frame.rows; i++){
				ints[i][0] = ints[i][0] + ints[i-1][0];
				intb[i][0] = intb[i][0] + intb[i-1][0];
				for(int k=0;k<9;k++)
					gradientM[i][0][k] = gradientM[i][0][k] +gradientM[i-1][0][k];
			}

			for(int j = 1; j < frame.cols; j++){
				ints[0][j] = ints[0][j] + ints[0][j-1];
				intb[0][j] = intb[0][j] + intb[0][j-1];
				for(int k=0;k<9;k++)
					gradientM[0][j][k] = gradientM[0][j][k] +gradientM[0][j-1][k];
			
			}

			
			for(int i =1; i < frame.rows;i++){
				for(int j =1; j < frame.cols;j++){
					ints[i][j] += ints[i-1][j]+ints[i][j-1]-ints[i-1][j-1];
					intb[i][j] += intb[i-1][j]+intb[i][j-1]-intb[i-1][j-1];
					for(int k=0;k<9;k++)
						gradientM[i][j][k] += gradientM[i-1][j][k] +gradientM[i][j-1][k] -gradientM[i-1][j-1][k];			
				}
			}

			// sliding block 羪 3:4 15:20 ~ 21:28 ~ 27:36 ~57:76 ~87:116b
			for(int a=5;a<31; a+=2){
				for(int i=0; i < frame.rows-a*4;i+= (a/2)){
						for(int j=0; j < frame.cols-a*3;j+= (a/2)){
							int area= a*a*3*4;
							int snum;
							int bnum;
							int ccount=0;
							int mainG[4][3];
							int secondG[4][3];
							snum = ints[i+a*4-1][j+a*3-1]-ints[i+a*4-1][j]-ints[i][j+a*3-1]+ints[i][j];
							//  1/4
							bnum = intb[i+a*2-1][j+a*3-1]-intb[i+a*2-1][j]-intb[i][j+a*3-1]+intb[i][j];
							for(int m=0; m<4;m++){
								for(int n=0 ; n<3;n++){
									double mG =DBL_MIN;
									double sG =DBL_MIN;
									int Gindex;
									for(int k=0;k<9;k++){
										double nowG = gradientM[i+(m+1)*a-1][j+(n+1)*a-1][k] - gradientM[i][j+(n+1)*a-1][k] - gradientM[i+(m+1)*a-1][j][k] + gradientM[i+m*a][j+n*a][k];
										if(nowG >mG){
											Gindex =k;
											mG =nowG;
										}
									}
									if(mG >300)
										mainG[m][n] = Gindex; //程璶よ
									else
										mainG[m][n] =-1;
									for(int k=0;k<9;k++){
										double nowG = gradientM[i+(m+1)*a-1][j+(n+1)*a-1][k]-gradientM[i][j+(n+1)*a-1][k] - gradientM[i+(m+1)*a-1][j][k] + gradientM[i+m*a][j+n*a][k];
										if(nowG >sG && nowG<mG){
											Gindex =k;
											sG =nowG;
										}
									}
									if(sG >300)
										secondG[m][n] = Gindex; //Ω璶よ
									else
										secondG[m][n] =-1;
								}
							}
							int score=0;
							for(int m=0; m<4;m++){
								for(int n=0 ; n<3;n++){									
									if(abs(mainG[m][n]%8 -mainGdirection[m][n]%8)<2 || abs(mainG[m][n]%8 -secondGdirection[m][n]%8)<2)
										score++;
									if(abs(secondG[m][n]%8 - mainGdirection[m][n]%8)<2|| abs(secondG[m][n]%8 -secondGdirection[m][n]%8)<2)
										score++;
								}
							}

							/*
							for(int ii=0;ii<a*4;ii++)
								for(int jj=0;jj<a*3;jj++)
									if(skin_image.at<uchar>(i+ii,j+jj)==255)
										ccount++;
								*/		
							//cout<<"area"<<area<<endl;
							//cout<<"snum"<<snum<<endl;
							//cout<<"ccount"<<ccount<<endl;
							//cout<<ccount-snum<<endl;
						

							if( ((double)snum /(double)area) >0.28 ){
								//rectangle(frameCopy,Point(j,i),Point(j+a*3,i+a*4),CV_RGB(0,255,0),1,8,0);
								if(((double)bnum /(double)area) >0.005 && ((double)bnum /(double)area) < 0.5){
									//rectangle(frameCopy,Point(j,i),Point(j+a*3,i+a*4),CV_RGB(0,0,255),1,8,0);
									//秨﹍耞硂跋办
									if(score >10){
										//rectangle(frameCopy,Point(j,i),Point(j+a*3,i+a*4),CV_RGB(0,0,255),1,8,0);
										int regionsum=0;
										for(int ii=0;ii<a*4;ii++)
											for(int jj=0;jj<a*3;jj++)
												if(edge_image.at<uchar>(i+ii,j+jj)==255)
													regionsum++;
										// edgeness璶ㄌthreshold
										if( ((double)regionsum /(double)area) >0.18 &&((double)regionsum /(double)area) <0.6){
											//蔼28*a  糴 13*a											
											if(i+28*a <= 240){
												double leftG=0;
												double rightG=0;
												double lverticalG=0;
												double rverticalG=0;


												for(int k=0;k<9;k++){
													leftG += gradientM[min(i+28*a,240)][j][k]+gradientM[i+12*a][max(j-5*a,0)][k]-gradientM[min(i+28*a,240)][max(j-5*a,0)][k]-gradientM[i+12*a][j][k];
													rightG += gradientM[min(i+28*a,240)][min(j+8*a,320)][k]+gradientM[i+12*a][j+3*a][k]-gradientM[min(i+28*a,240)][j+3*a][k]-gradientM[i+12*a][min(j+8*a,320)][k];
												
													if(k==0||k==4||k==8){
														lverticalG += gradientM[min(i+28*a,240)][j][k]+gradientM[i+12*a][max(j-5*a,0)][k]-gradientM[min(i+28*a,240)][max(j-5*a,0)][k]-gradientM[i+12*a][j][k];
														rverticalG += gradientM[min(i+28*a,240)][min(j+8*a,320)][k]+gradientM[i+12*a][j+3*a][k]-gradientM[min(i+28*a,240)][j+3*a][k]-gradientM[i+12*a][min(j+8*a,320)][k];
													}
												}
												//rectangle(frameCopy,Point(j,i),Point(j+a*3,i+a*4),CV_RGB(0,255,0),1,8,0);
												if( (lverticalG/leftG)>0.25 &&(rverticalG/rightG)>0.25){
													tmpsumy +=i+a*2;
													tmpsumx +=j+a*1.5;
                          tmpaa +=a;
													nncount ++;
													//rectangle(frameCopy,Point(j,i),Point(j+a*3,i+a*4),CV_RGB(255,0,0),1,8,0);
												}
											}
										}
									}
								}
							}
						}
				}
			}
     
      line(frameCopy,Point(100,0),Point(100,239),CV_RGB(0,0,255),1,8,0);
      line(frameCopy,Point(220,0),Point(220,239),CV_RGB(0,0,255),1,8,0);

      imshow("bb",frameCopy);
      cvWaitKey(10);
      run_count = (run_count+1)%10;
      if ( nncount == 0 || run_count ) return;
			int xcentral = tmpsumx/nncount; 
      int ycentral = tmpsumy/nncount;
      int aaaaaaa = tmpaa/nncount;			
    
			rectangle(frameCopy,Point(xcentral-1.5*aaaaaaa,ycentral-2*aaaaaaa),Point(xcentral+1.5*aaaaaaa,ycentral+2*aaaaaaa),CV_RGB(255,0,0),1,8,0);
      

			if(xcentral <100){
				sound_flag = 0;
			
			}
			else if(xcentral<220){
				sound_flag = 1;
			
			}
			else{
				sound_flag = 2;
			
			}
      frame_show = frameCopy.clone();
      SetEvent(hPlaySoundEvent);
			
}


void _PlaySoundProcess ( ) {
  
  while ( 1 ) {
    WaitForSingleObject(hPlaySoundEvent,INFINITE);
    imshow("detector window",frame_show);
    cvWaitKey(10);
    if(sound_flag==0){
        char *cmd = "flying 0 0 -1 0";
				PlaySound(TEXT("D:\\ardrone_tmp\\left.wav"),NULL,SND_ASYNC||SND_FILENAME);
			  sendto(cmd_sender,cmd,strlen(cmd)+1,0,(sockaddr*)&addr_cmd_sender,sizeof(addr_cmd_sender));
        Sleep(100);
			}
			else if(sound_flag==1){
				PlaySound(TEXT("D:\\ardrone_tmp\\front.wav"),NULL,SND_ASYNC||SND_FILENAME);
			
			}
			else if (sound_flag==2) {
        char *cmd = "flying 0 0 1 0";
				PlaySound(TEXT("D:\\ardrone_tmp\\right.wav"),NULL,SND_ASYNC||SND_FILENAME);
			  sendto(cmd_sender,cmd,strlen(cmd)+1,0,(sockaddr*)&addr_cmd_sender,sizeof(addr_cmd_sender));
        Sleep(100);
      }
    
    ResetEvent(hPlaySoundEvent);
  }
}
static DWORD WINAPI PlaySoundProcess ( LPVOID pParam ) {
  hPlaySoundEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
  _PlaySoundProcess();
  //WaitForSingleObject(hPlaySoundEvent,INFINITE);
  CloseHandle(hPlaySoundEvent);
  return 0;
}
int main(int argc, char *argv[]){
  int run_count=0;
  char title_buf[100];
	int res = SocketInit();
	if (res)	
		return res;
	char recvbuf[10];
	char tmprecvbuf[230400];
	int recvtotal=0;
	int recvonce=0;
	int endflag=0;
	Mat frame(240,320,CV_8UC3);
	printf("start to receive image!!\n");
  hSoundPlayerThread = CreateThread(NULL,0,PlaySoundProcess,0,NULL,NULL);
	while(1){
    
		recvtotal=0;
		recvonce=0;
		if (endflag ==1){
			break;
		}
		
		recvonce = recv(svr,recvbuf,3,0);
    if (recvonce == SOCKET_ERROR ){
				endflag=1;
				break;
		}
    run_count = (run_count+1)%5;
    if  ( run_count ) continue;
		sprintf(title_buf,"D:\\ardrone_tmp\\%s.jpg",recvbuf);	
		printf("%s\n",title_buf);
    //printf("image got!\n");
    frame = imread(title_buf,1);
		imshow("got",frame);
    mydetectAndDraw(frame);
		waitKey(10);
	}
  WaitForSingleObject(hSoundPlayerThread,INFINITE);
  CloseHandle(hSoundPlayerThread);
  return 0;
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
  cmd_sender = socket(AF_INET,SOCK_DGRAM,0);
  //创建地址   
  addrSvr.sin_family = AF_INET;  
  addrSvr.sin_port = htons(6001);  
  addrSvr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");  
  addr_cmd_sender.sin_family = AF_INET;
  addr_cmd_sender.sin_port = htons(2001);
  addr_cmd_sender.sin_addr.S_un.S_addr= inet_addr("127.0.0.1");
  len = sizeof(sockaddr); 
  bind(svr,(struct sockaddr*)&addrSvr,sizeof(addrSvr));
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
	  //recv
    //Wait for kinect write socket buffer
    //printf("Waiting for message...\n");
    //HRESULT res = WaitForSingleObject(m_hSocketBufferReadyEvent,INFINITE);
    //if ( res == WAIT_FAILED ) break;
    //if ( res == WAIT_OBJECT_0 ) {
      if ( socket_buf=="takeoff" || socket_buf=="landing" || socket_buf=="hovering" || socket_buf.find("flying") ) {
        sendto(svr,socket_buf.c_str(),strlen(socket_buf.c_str())+1,0,(sockaddr*)&addrSvr,len);
      }
    //}
    //ResetEvent(m_hSocketBufferReadyEvent);

  }

  SocketUnInit();
}
