#include <stdio.h>
#include <time.h>
#include "highgui.h"
#include <strsafe.h>
#include <fstream>
#include <Windows.h>
#include "cxcore.h"
#include "cv.h"
#include<opencv2/highgui/highgui.hpp>
#include "highgui.h"
#include<iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include<cstdio>
#include<opencv2/videoio/videoio.hpp>
//#include<opencv2/videoio/videoio_c.h>
#include<opencv2/imgcodecs/imgcodecs.hpp>
#include"vfw.h"
#include"io.h"
#include<sstream>
#include<string>
#pragma comment(lib,"vfw32")
#pragma comment(lib,"comctl32")
#pragma comment(lib,"opencv_world300d")
#pragma comment(lib,"ws2_32.lib") 

using namespace cv;
using namespace std;

int main(void)
{ifstream ifs("C:\\Kinect Data\\time.txt");
//ifstream ifs1("C:\\Kinect Data\\end.txt");
	IplImage* img = NULL; // 存放图像
	//CvVideoWriter *writer = NULL;// 写视频结构
	int i = 1;// 存放的文件名开始
	int start;
	ifs >> start;
	int d = 0;
	char image_name[100]; // 存放图像名
	int num_frm;
	int isColor = 1; // 如果非零，编码器将希望得到彩色帧并进行编码；否则，是灰度帧（只有在Windows下支持这个标志）。
	int fps = 30; // 图像帧率，格式工厂会让你很轻松的了解视频相关信息
	int frameW = 1920; // 帧宽，根据你需要的大小修改
	int frameH = 1080; // 帧高，同上
	//int frameW = 640; // 帧宽，根据你需要的大小修改
	//int frameH = 360; // 帧高，同上
	char key;
	int test;
	stringstream ss;string name;
	CvVideoWriter *writer = NULL;
	string filename_1;
	char* filename;
	//writer = cvCreateVideoWriter("out.avi", CV_FOURCC('D', 'I', 'V', 'X'), fps, cvSize(frameW, frameH), isColor); // 创建视频文件写入器
	printf("------------- image to video ... ----------------\n");   
	
	printf("\tvideo height : %d\n\tvideo width : %d\n\tfps : %d\n", frameH, frameW, fps);
	cvNamedWindow("mainWin", CV_WINDOW_AUTOSIZE); //创建窗口
	//num_frm = 3867;//最后一帧
	//while (i<num_frm+1)
	//i = 1;
	//while (i<800)
		// 选择你需要停止的最后一帧图像，我这里num_frm=21000，可自定义
	//{
		//if (i==start)
		//{
			//ifs >> start;
	        time_t rawtime;
			struct tm* timeinfo;
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			ss << timeinfo->tm_year + 1900 << "-" << 1 + timeinfo->tm_mon << "-" << timeinfo->tm_mday << "-" << timeinfo->tm_hour << "-" << timeinfo->tm_min;
			ss >> name;
			filename_1 = "C:\\Kinect Data\\Video\\"+name + ".avi";
			filename = (char *)filename_1.c_str();
			writer = cvCreateVideoWriter(filename, CV_FOURCC('D', 'I', 'V', 'X'), fps, cvSize(frameW, frameH), isColor); // 创建视频文件写入器
			
          //  writer = cvCreateVideoWriter("out.avi", CV_FOURCC('D', 'I', 'V', 'X'), fps, cvSize(frameW, frameH), isColor);
			sprintf_s(image_name, "C:\\Users\\Kinect Project\\Pictures\\%d.bmp", i++);
			//while (img = cvLoadImage(image_name, -1))
			while (_access(image_name,0)+1)
				// 选择你需要停止的最后一帧图像，我这里num_frm=21000，可自定义
			{
            // // 创建视频文件写入器
		//}
		
		//sprintf_s(image_name, "C:\\Users\\Kinect Project\\Desktop\\Pictures\\%d.bmp", i++);
		 // 读取图像
				/*
		if (!img)
		{
			printf("Could not load image file...\n");
			exit(0);
		}
		*/
		img = cvLoadImage(image_name, -1);
		cvShowImage("mainWin", img); // 显示图像
		key = cvWaitKey(20);
		cvWriteFrame(writer, img); //将该帧图像写入视频
		sprintf_s(image_name, "C:\\Users\\Kinect Project\\Pictures\\%d.bmp", i++);
		//cvReleaseImage(&img); // 释放图像内存
	}
	cvReleaseVideoWriter(&writer); // 释放结构
	cvDestroyWindow("mainWin"); // 销毁窗口
}