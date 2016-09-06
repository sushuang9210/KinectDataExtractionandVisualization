//------------------------------------------------------------------------------
// <copyright file="FaceBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#include "stdafx.h"
#include <strsafe.h>
#include "resource.h"
#include "FaceBasics.h"
#include <fstream>
#include <iostream>

std::ofstream ofs("C:\\Kinect Data\\skeleton.txt");
std::ifstream ifs_coords_in;
std::ofstream ofs_coords_out;
char infile[MAX_PATH], outfile[MAX_PATH];
bool convert_once_fl = FALSE;

INT64 nTime = 0;
// face property text layout offset in X axis
static const float c_FaceTextLayoutOffsetX = -0.1f;

// face property text layout offset in Y axis
static const float c_FaceTextLayoutOffsetY = -0.125f;

// define the face frame features required to be computed by this application
static const DWORD c_FaceFrameFeatures = 
    FaceFrameFeatures::FaceFrameFeatures_BoundingBoxInColorSpace
    | FaceFrameFeatures::FaceFrameFeatures_PointsInColorSpace
    | FaceFrameFeatures::FaceFrameFeatures_RotationOrientation
    | FaceFrameFeatures::FaceFrameFeatures_Happy
    | FaceFrameFeatures::FaceFrameFeatures_RightEyeClosed
    | FaceFrameFeatures::FaceFrameFeatures_LeftEyeClosed
    | FaceFrameFeatures::FaceFrameFeatures_MouthOpen
    | FaceFrameFeatures::FaceFrameFeatures_MouthMoved
    | FaceFrameFeatures::FaceFrameFeatures_LookingAway
    | FaceFrameFeatures::FaceFrameFeatures_Glasses
    | FaceFrameFeatures::FaceFrameFeatures_FaceEngagement;
int timestamp = 0;

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CFaceBasics application;
    application.Run(hInstance, nCmdShow);
}

/// <summary>
/// Constructor
/// </summary>
CFaceBasics::CFaceBasics() :
    m_hWnd(NULL),
    m_nStartTime(0),
    m_nLastCounter(0),
    m_nFramesSinceUpdate(0),
    m_fFreq(0),
    m_nNextStatusTime(0),
    m_pKinectSensor(nullptr),
    m_pCoordinateMapper(nullptr),
    m_pColorFrameReader(nullptr),
	m_pDepthFrameReader(nullptr), // DH
	m_pDepthData(nullptr), // DH
    m_pD2DFactory(nullptr),
    m_pDrawDataStreams(nullptr),
    m_pColorRGBX(nullptr),
    m_pBodyFrameReader(nullptr)	
{
    LARGE_INTEGER qpf = {0};
    if (QueryPerformanceFrequency(&qpf))
    {
        m_fFreq = double(qpf.QuadPart);
    }

    for (int i = 0; i < BODY_COUNT; i++)
    {
        m_pFaceFrameSources[i] = nullptr;
        m_pFaceFrameReaders[i] = nullptr;
    }

    // create heap storage for color pixel data in RGBX format
    m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];
	m_pDepthData = new UINT16[cDepthWidth * cDepthHeight];

	ofs << "skeleton: Joints (X,Y,Z,state); Color point (X,Y); Depth point (X,Y,D); " << '\n';
	ofs << "face points: color (X,Y); depth (X,Y,D); camera (X,Y,Z)" << '\n';
	ofs << "face bbox: bottom (X,Y); left (X,Y); right (X,Y); top (X,Y);" << '\n';
	ofs << "face orientation quaternion: (X,Y,Z,W);" << '\n';

	//////////////////////////////////////////////////
	// added by hararid 2/2014
	sprintf_s(infile, "C:\\Kinect Data\\xyd_coords.csv");
	ifs_coords_in.open(infile);
	sprintf_s(outfile, "C:\\Kinect Data\\xyz_coords.csv");
	ofs_coords_out.open(outfile);
	//////////////////////////////////////////////////
}


/// <summary>
/// Destructor
/// </summary>
CFaceBasics::~CFaceBasics()
{
    // clean up Direct2D renderer
    if (m_pDrawDataStreams)
    {
        delete m_pDrawDataStreams;
        m_pDrawDataStreams = nullptr;
    }

    if (m_pColorRGBX)
    {
        delete [] m_pColorRGBX;
        m_pColorRGBX = nullptr;
    }

	// DH
	if (m_pDepthData)
	{
		delete[] m_pDepthData;
		m_pDepthData = nullptr;
	}
	
	// clean up Direct2D
    SafeRelease(m_pD2DFactory);

    // done with face sources and readers
    for (int i = 0; i < BODY_COUNT; i++)
    {
        SafeRelease(m_pFaceFrameSources[i]);
        SafeRelease(m_pFaceFrameReaders[i]);		
    }

    // done with body frame reader
    SafeRelease(m_pBodyFrameReader);

    // done with color frame reader
    SafeRelease(m_pColorFrameReader);

	// done with depth frame reader - DH
	SafeRelease(m_pDepthFrameReader);

    // done with coordinate mapper
    SafeRelease(m_pCoordinateMapper);

    // close the Kinect Sensor
    if (m_pKinectSensor)
    {
        m_pKinectSensor->Close();
    }

    SafeRelease(m_pKinectSensor);
}

/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CFaceBasics::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = {0};
    WNDCLASS  wc;

    // Dialog custom window class
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra    = DLGWINDOWEXTRA;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc   = DefDlgProcW;
    wc.lpszClassName = L"FaceBasicsAppDlgWndClass";

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        NULL,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
        (DLGPROC)CFaceBasics::MessageRouter, 
        reinterpret_cast<LPARAM>(this));

    // Show window
    ShowWindow(hWndApp, nCmdShow);

    // Main message loop
    while (WM_QUIT != msg.message)
    {
        Update();

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if (hWndApp && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CFaceBasics::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CFaceBasics* pThis = nullptr;

    if (WM_INITDIALOG == uMsg)
    {
        pThis = reinterpret_cast<CFaceBasics*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CFaceBasics*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CFaceBasics::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
        {
            // Bind application window handle
            m_hWnd = hWnd;

            // Init Direct2D
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

            // Create and initialize a new Direct2D image renderer (take a look at ImageRenderer.h)
            // We'll use this to draw the data we receive from the Kinect to the screen
            m_pDrawDataStreams = new ImageRenderer();
            HRESULT hr = m_pDrawDataStreams->Initialize(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), m_pD2DFactory, cColorWidth, cColorHeight, cColorWidth * sizeof(RGBQUAD)); 
            if (FAILED(hr))
            {
                SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
            }

            // Get and initialize the default Kinect sensor
            InitializeDefaultSensor();
        }
        break;

        // If the titlebar X is clicked, destroy app
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        // Quit the main message pump
        PostQuitMessage(0);
        break;        
    }

    return FALSE;
}

/// <summary>
/// Initializes the default Kinect sensor
/// </summary>
/// <returns>S_OK on success else the failure code</returns>
HRESULT CFaceBasics::InitializeDefaultSensor()
{
    HRESULT hr;

    hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
        return hr;
    }

    if (m_pKinectSensor)
    {
        // Initialize Kinect and get color, body and face readers
        IColorFrameSource* pColorFrameSource = nullptr;
		IDepthFrameSource* pDepthFrameSource = nullptr; // DH
        IBodyFrameSource* pBodyFrameSource = nullptr;

        hr = m_pKinectSensor->Open();

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
        }

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_ColorFrameSource(&pColorFrameSource);
        }

        if (SUCCEEDED(hr))
        {
            hr = pColorFrameSource->OpenReader(&m_pColorFrameReader);
        }

		// DH
		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
		}
		

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_BodyFrameSource(&pBodyFrameSource);
        }
		else // DH
		{
			printf("failded to initialize depth frame source\n");
		}

        if (SUCCEEDED(hr))
        {
            hr = pBodyFrameSource->OpenReader(&m_pBodyFrameReader);
        }

        if (SUCCEEDED(hr))
        {
            // create a face frame source + reader to track each body in the fov
            for (int i = 0; i < BODY_COUNT; i++)
            {
                if (SUCCEEDED(hr))
                {
                    // create the face frame source by specifying the required face frame features
                    hr = CreateFaceFrameSource(m_pKinectSensor, 0, c_FaceFrameFeatures, &m_pFaceFrameSources[i]);
                }
                if (SUCCEEDED(hr))
                {
                    // open the corresponding reader
                    hr = m_pFaceFrameSources[i]->OpenReader(&m_pFaceFrameReaders[i]);
                }				
            }
        }        

        SafeRelease(pColorFrameSource);
        SafeRelease(pBodyFrameSource);
		SafeRelease(pDepthFrameSource);
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
        SetStatusMessage(L"No ready Kinect found!", 10000, true);
        return E_FAIL;
    }

    return hr;
}

/// <summary>
/// Main processing function
/// </summary>
void CFaceBasics::Update()
{
    if (!m_pColorFrameReader || !m_pBodyFrameReader)
    {
        return;
    }

    IColorFrame* pColorFrame = nullptr;
    HRESULT hr = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);

    if (SUCCEEDED(hr))
    {
        //INT64 nTime = 0;
        IFrameDescription* pFrameDescription = nullptr;
        int nWidth = 0;
        int nHeight = 0;
        ColorImageFormat imageFormat = ColorImageFormat_None;
        UINT nBufferSize = 0;
        RGBQUAD *pBuffer = nullptr;

        hr = pColorFrame->get_RelativeTime(&nTime);

        if (SUCCEEDED(hr))
        {
            hr = pColorFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
        }

        if (SUCCEEDED(hr))
        {
            hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
        }

        if (SUCCEEDED(hr))
        {
            if (imageFormat == ColorImageFormat_Bgra)
            {
                hr = pColorFrame->AccessRawUnderlyingBuffer(&nBufferSize, reinterpret_cast<BYTE**>(&pBuffer));
            }
            else if (m_pColorRGBX)
            {
                pBuffer = m_pColorRGBX;
                //nBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
				nBufferSize = nWidth * nHeight * sizeof(RGBQUAD);
                hr = pColorFrame->CopyConvertedFrameDataToArray(nBufferSize, reinterpret_cast<BYTE*>(pBuffer), ColorImageFormat_Bgra);            
            }
            else
            {
                hr = E_FAIL;
            }
        }			

        if (SUCCEEDED(hr))
        {   
			WCHAR szScreenshotPath[MAX_PATH];

			// Retrieve the path to My Photos
			GetScreenshotFileName(szScreenshotPath, _countof(szScreenshotPath), timestamp);
			//i = i + 1;
			// Write out the bitmap to disk
			// comment out to discard of frame dump to file
//			HRESULT hr = SaveBitmapToFile(reinterpret_cast<BYTE*>(pBuffer), nWidth, nHeight, sizeof(RGBQUAD) * 8, szScreenshotPath);
			timestamp = timestamp + 1;
			WCHAR szStatusMessage[64 + MAX_PATH];
			DrawStreams(nTime, pBuffer, nWidth, nHeight);

        }
		
		// DH begin
		IDepthFrame* pDepthFrame = nullptr;
		HRESULT hrD = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);

		if (SUCCEEDED(hrD))
		{
			// initialize depth frame data 
			IFrameDescription* pDepthFrameDescription = nullptr;
			hrD = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
			if (SUCCEEDED(hrD))
			{
				UINT depthSize = 0;
				hrD = pDepthFrameDescription->get_LengthInPixels(&depthSize);
				if (SUCCEEDED(hrD))
				{
					// load depth frame into ushort[] 
					hrD = pDepthFrame->CopyFrameDataToArray(depthSize, m_pDepthData);
					printf("%d\n", depthSize);
				}
			}
			SafeRelease(pDepthFrameDescription);
		}
		else
		{
			hrD = E_FAIL;
		}
		SafeRelease(pDepthFrame);
		// DH end

		SafeRelease(pFrameDescription);		
    }

    SafeRelease(pColorFrame);
	//timestamp = timestamp + 1;
}

/// <summary>
/// Renders the color and face streams
/// </summary>
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>

HRESULT CFaceBasics::GetScreenshotFileName(_Out_writes_z_(nFilePathSize) LPWSTR lpszFilePath, UINT nFilePathSize, int i)
{
	WCHAR* pszKnownPath = NULL;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &pszKnownPath);

	if (SUCCEEDED(hr))
	{
		// Get the time
		WCHAR szTimeString[MAX_PATH];
		GetTimeFormatEx(NULL, 0, NULL, L"hh'-'mm'-'ss", szTimeString, _countof(szTimeString));

		// File name will be KinectScreenshotColor-HH-MM-SS.bmp
		//StringCchPrintfW(lpszFilePath, nFilePathSize, L"%s\\KinectScreenshot-Color-%s.bmp", pszKnownPath, szTimeString);
		StringCchPrintfW(lpszFilePath, nFilePathSize, L"%s\\%d.bmp", pszKnownPath, i);
		//StringCchPrintfW(lpszFilePath, nFilePathSize, L"%s\\%d.bmp", pszKnownPath, (nTime - m_nStartTime));
	}

	if (pszKnownPath)
	{
		CoTaskMemFree(pszKnownPath);
	}

	return hr;
}

/// <summary>
/// Save passed in image data to disk as a bitmap
/// </summary>
/// <param name="pBitmapBits">image data to save</param>
/// <param name="lWidth">width (in pixels) of input image data</param>
/// <param name="lHeight">height (in pixels) of input image data</param>
/// <param name="wBitsPerPixel">bits per pixel of image data</param>
/// <param name="lpszFilePath">full file path to output bitmap to</param>
/// <returns>indicates success or failure</returns>
HRESULT CFaceBasics::SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath)
{
	DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

	BITMAPINFOHEADER bmpInfoHeader = { 0 };

	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);  // Size of the header
	bmpInfoHeader.biBitCount = wBitsPerPixel;             // Bit count
	bmpInfoHeader.biCompression = BI_RGB;                    // Standard RGB, no compression
	bmpInfoHeader.biWidth = lWidth;                    // Width in pixels
	bmpInfoHeader.biHeight = -lHeight;                  // Height in pixels, negative indicates it's stored right-side-up
	bmpInfoHeader.biPlanes = 1;                         // Default
	bmpInfoHeader.biSizeImage = dwByteCount;               // Image size in bytes

	BITMAPFILEHEADER bfh = { 0 };

	bfh.bfType = 0x4D42;                                           // 'M''B', indicates bitmap
	bfh.bfOffBits = bmpInfoHeader.biSize + sizeof(BITMAPFILEHEADER);  // Offset to the start of pixel data
	bfh.bfSize = bfh.bfOffBits + bmpInfoHeader.biSizeImage;        // Size of image + headers

	// Create the file on disk to write to
	HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	// Return if error opening file
	if (NULL == hFile)
	{
		return E_ACCESSDENIED;
	}

	DWORD dwBytesWritten = 0;

	// Write the bitmap file header
	if (!WriteFile(hFile, &bfh, sizeof(bfh), &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Write the bitmap info header
	if (!WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Write the RGB Data
	if (!WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Close the file
	CloseHandle(hFile);
	return S_OK;
}




void CFaceBasics::DrawStreams(INT64 nTime, RGBQUAD* pBuffer, int nWidth, int nHeight)
{
    if (m_hWnd)
    {
        HRESULT hr;
        hr = m_pDrawDataStreams->BeginDrawing();

        if (SUCCEEDED(hr))
        {
            // Make sure we've received valid color data
            if (pBuffer && (nWidth == cColorWidth) && (nHeight == cColorHeight))
            {
                // Draw the data with Direct2D
                hr = m_pDrawDataStreams->DrawBackground(reinterpret_cast<BYTE*>(pBuffer), cColorWidth * cColorHeight * sizeof(RGBQUAD));        
            }
            else
            {
                // Recieved invalid data, stop drawing
                hr = E_INVALIDARG;
            }

            if (SUCCEEDED(hr))
            {
                // begin processing the face frames
                ProcessFaces();				
            }

            m_pDrawDataStreams->EndDrawing();
        }

        if (!m_nStartTime)
        {
            m_nStartTime = nTime;
        }

        double fps = 0.0;

        LARGE_INTEGER qpcNow = {0};
        if (m_fFreq)
        {
            if (QueryPerformanceCounter(&qpcNow))
            {
                if (m_nLastCounter)
                {
                    m_nFramesSinceUpdate++;
                    fps = m_fFreq * m_nFramesSinceUpdate / double(qpcNow.QuadPart - m_nLastCounter);
                }
            }
        }

        WCHAR szStatusMessage[64];
        StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" FPS = %0.2f    Time = %I64d", fps, (nTime - m_nStartTime));

        if (SetStatusMessage(szStatusMessage, 1000, false))
        {
            m_nLastCounter = qpcNow.QuadPart;
            m_nFramesSinceUpdate = 0;
        }
    }    
}


/// <summary>
/// Processes new face frames
/// </summary>
void CFaceBasics::ProcessFaces()
{
    HRESULT hr;
    IBody* ppBodies[BODY_COUNT] = {0};
    bool bHaveBodyData = SUCCEEDED( UpdateBodyData(ppBodies) );
	RECT rct;
	GetClientRect(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), &rct);
	int width = rct.right;
	int height = rct.bottom;
	JointOrientation joint_orientations[JointType_Count];
	Joint joints[JointType_Count];

    // iterate through each face reader
    for (int iFace = 0; iFace < BODY_COUNT; ++iFace)
    {
		IBody* pBody = ppBodies[iFace];
		if (pBody)
		{
			BOOLEAN bTracked = false;
			hr = pBody->get_IsTracked(&bTracked);

			if (SUCCEEDED(hr) && bTracked)
			{
				D2D1_POINT_2F jointPoints[JointType_Count];
				HandState leftHandState = HandState_Unknown;
				HandState rightHandState = HandState_Unknown;

				pBody->get_HandLeftState(&leftHandState);
				pBody->get_HandRightState(&rightHandState);

				hr = pBody->GetJoints(_countof(joints), joints);
				hr = pBody->GetJointOrientations(_countof(joint_orientations), joint_orientations);
				if (SUCCEEDED(hr))
				{
					ofs << "--" << timestamp << "-" << nTime << '\n';
					//timestamp = timestamp + 1;
//					ofs << "sk: Joints (X,Y,Z,state); Color point (X,Y); Depth point (X,Y,D); " << iFace << ":\n";
					ofs << "face id:" << iFace << "\n";
					ofs << "Floor clip plane:" << m_floorClipFrame.x << ", " << m_floorClipFrame.y << ", " << m_floorClipFrame.z << ", " << m_floorClipFrame.w << "\n";
					CameraSpacePoint CameraPoint = { 0 };
					DepthSpacePoint depthPoint = { 0 };
					ColorSpacePoint colorPoint = { 0 };
					for (int j = 0; j < _countof(joints); ++j)
					{
						//float screenPointX = static_cast<float>(depthPoint.X * width) / 512;
						//float screenPointY = static_cast<float>(depthPoint.Y * height) / 424;
						//jointPoints[j] =D2D1::Point2F(screenPointX, screenPointY);
						ofs << "(" << joints[j].Position.X << " " << joints[j].Position.Y << " " << joints[j].Position.Z << ' ' << joints[j].TrackingState << ") | ";

						m_pCoordinateMapper->MapCameraPointToColorSpace(joints[j].Position, &colorPoint);
						if ((colorPoint.X > cColorWidth) || (colorPoint.Y > cColorHeight) || (colorPoint.X < 0) || (colorPoint.Y < 0))
						{
							ofs << "(0 0) | ";
						}
						else
						{
							ofs << "(" << colorPoint.X << " " << colorPoint.Y << ") | ";
						}

						m_pCoordinateMapper->MapCameraPointToDepthSpace(joints[j].Position, &depthPoint);
						if ((depthPoint.X > cDepthWidth) || (depthPoint.Y > cDepthHeight) || (depthPoint.X < 0) || (depthPoint.Y < 0))
						{
							ofs << "(0 0 0) | ";
						}
						else
						{
							ofs << "(" << depthPoint.X << " " << depthPoint.Y << " " << m_pDepthData[(int)(depthPoint.Y + 0.5)*cDepthWidth + (int)(depthPoint.X + 0.5)] << ") | ";
						}
					}
					ofs << '\n';
					
					char dataline[100];
					if (convert_once_fl)
					{
						int depthval = 0;

						while (!ifs_coords_in.eof())
						{
							ifs_coords_in.getline(dataline, 100);
							sscanf(dataline, "%f, %f, %d", &depthPoint.X, &depthPoint.Y, &depthval);
							m_pCoordinateMapper->MapDepthPointToCameraSpace(depthPoint, (USHORT)(depthval), &CameraPoint);
							ofs_coords_out << CameraPoint.X << ", " << CameraPoint.Y << ", " << CameraPoint.Z << '\n';
						}
						convert_once_fl = FALSE;
						ifs_coords_in.close();
						ofs_coords_out.close();
					}
				}
			}
		}
        // retrieve the latest face frame from this reader
        IFaceFrame* pFaceFrame = nullptr;
        hr = m_pFaceFrameReaders[iFace]->AcquireLatestFrame(&pFaceFrame);

        BOOLEAN bFaceTracked = false;
        if (SUCCEEDED(hr) && nullptr != pFaceFrame)
        {
            // check if a valid face is tracked in this face frame
            hr = pFaceFrame->get_IsTrackingIdValid(&bFaceTracked);
        }

        if (SUCCEEDED(hr))
        {
//            if (bFaceTracked)
//            {
//                IFaceFrameResult* pFaceFrameResult = nullptr;
//                RectI faceBox = {0};
//                PointF facePoints[FacePointType::FacePointType_Count];
//                Vector4 faceRotation;
//                DetectionResult faceProperties[FaceProperty::FaceProperty_Count];
//                D2D1_POINT_2F faceTextLayout;
//
//                hr = pFaceFrame->get_FaceFrameResult(&pFaceFrameResult);
//
//                // need to verify if pFaceFrameResult contains data before trying to access it
//                if (SUCCEEDED(hr) && pFaceFrameResult != nullptr)
//                {
//                    hr = pFaceFrameResult->get_FaceBoundingBoxInColorSpace(&faceBox);
//
//                    if (SUCCEEDED(hr))
//                    {										
//                        hr = pFaceFrameResult->GetFacePointsInColorSpace(FacePointType::FacePointType_Count, facePoints);
//			
//						CameraSpacePoint* cameraSpacePoints = new CameraSpacePoint[cColorWidth * cColorHeight];
//						hr = m_pCoordinateMapper->MapColorFrameToCameraSpace(cDepthWidth * cDepthHeight, m_pDepthData, cColorWidth * cColorHeight, cameraSpacePoints);
//
////						ofs << "face points: color (X,Y); depth (X,Y,D); camera (X,Y,Z)" << '\n';
//						for (int j = 0; j < _countof(facePoints); ++j)
//						{
//							DepthSpacePoint depthPoint = { 0 };
//							long colorIndex = (long)(facePoints[j].Y * cColorWidth + facePoints[j].X);
//
//							ofs << "(" << facePoints[j].X << " " << facePoints[j].Y << ") |";
//							m_pCoordinateMapper->MapCameraPointToDepthSpace(cameraSpacePoints[colorIndex], &depthPoint);
//							ofs << "(" << depthPoint.X << " " << depthPoint.Y << " " << m_pDepthData[(int)(depthPoint.Y + 0.5)*cDepthWidth + (int)(depthPoint.X + 0.5)] << ") |";
//							ofs << "(" << cameraSpacePoints[colorIndex].X << " " << cameraSpacePoints[colorIndex].Y << " " << cameraSpacePoints[colorIndex].Z << ") |";
//						}
//						ofs << '\n';
////						ofs << "(" << facePoints[0].X << "," << facePoints[0].Y << "),(" << facePoints[1].X << "," << facePoints[1].Y << "),(" << facePoints[2].X << "," << facePoints[2].Y << "),(" << facePoints[3].X << "," << facePoints[3].Y << "),(" << facePoints[4].X << "," << facePoints[4].Y << ")," << '\n';
////						ofs << "face bbox: bottom (X,Y); left (X,Y); right (X,Y); top (X,Y);" << '\n';
//						ofs << "(" << faceBox.Bottom << "," << faceBox.Left << "," << faceBox.Right << "," << faceBox.Top << ")" << '\n';
//
//						if (cameraSpacePoints)
//						{
//							delete[] cameraSpacePoints;
//							cameraSpacePoints = nullptr;
//						}
//                    }
//
//                    if (SUCCEEDED(hr))
//                    {
//                        hr = pFaceFrameResult->get_FaceRotationQuaternion(&faceRotation);
//                    }
////					ofs << "face orientation quaternion: (X,Y,Z,W);" << '\n';
//					ofs << "(" << faceRotation.x << "," << faceRotation.y << "," << faceRotation.z << "," << faceRotation.w << ")" << '\n';
//					
//                    if (SUCCEEDED(hr))
//                    {
//						hr = pFaceFrameResult->GetFaceProperties(FaceProperty::FaceProperty_Count, faceProperties); 
//						ofs << faceProperties[0] << "," << faceProperties[1] << "," << faceProperties[2] << "," << faceProperties[4] << "," << faceProperties[5] << "," << faceProperties[6] << "," << faceProperties[7] << '\n';
//                    }
//
//                    if (SUCCEEDED(hr))
//                    {
//                        hr = GetFaceTextPositionInColorSpace(ppBodies[iFace], &faceTextLayout);
//                    }
//
//                    if (SUCCEEDED(hr))
//                    {
//                        // draw face frame results
//                       // m_pDrawDataStreams->DrawFaceFrameResults(iFace, &faceBox, facePoints, &faceRotation, faceProperties, &faceTextLayout);
//                    }							
//                }
//
//                SafeRelease(pFaceFrameResult);	
//            }
			if (bFaceTracked)
			{
				IFaceFrameResult* pFaceFrameResult = nullptr;
				RectI faceBox = { 0 };
				PointF facePoints[FacePointType::FacePointType_Count];
				Vector4 faceRotation;
				DetectionResult faceProperties[FaceProperty::FaceProperty_Count];
				D2D1_POINT_2F faceTextLayout;

				hr = pFaceFrame->get_FaceFrameResult(&pFaceFrameResult);

				// need to verify if pFaceFrameResult contains data before trying to access it
				if (SUCCEEDED(hr) && pFaceFrameResult != nullptr)
				{
					hr = pFaceFrameResult->get_FaceBoundingBoxInColorSpace(&faceBox);

					hr = pFaceFrameResult->GetFacePointsInColorSpace(FacePointType::FacePointType_Count, facePoints);

					CameraSpacePoint* cameraSpacePoints = new CameraSpacePoint[cColorWidth * cColorHeight];
					DepthSpacePoint* depthSpacePoints = new DepthSpacePoint[cColorWidth * cColorHeight];
					hr = m_pCoordinateMapper->MapColorFrameToCameraSpace(cDepthWidth * cDepthHeight, m_pDepthData, cColorWidth * cColorHeight, cameraSpacePoints);
					hr = m_pCoordinateMapper->MapColorFrameToDepthSpace(cDepthWidth * cDepthHeight, m_pDepthData, cColorWidth * cColorHeight, depthSpacePoints);

					//						ofs << "face points: color (X,Y); depth (X,Y,D); camera (X,Y,Z)" << '\n';
					for (int j = 0; j < _countof(facePoints); ++j)
					{
						DepthSpacePoint depthPoint = { 0 };
						CameraSpacePoint cameraPoint = { 0 };
						int depthVal;

						long colorIndex = (long)((int)(facePoints[j].Y+0.5) * cColorWidth + (int)(facePoints[j].X+0.5));
						depthPoint = depthSpacePoints[colorIndex];
						cameraPoint = cameraSpacePoints[colorIndex];
						depthVal = m_pDepthData[(int)(depthPoint.Y + 0.5)*cDepthWidth + (int)(depthPoint.X + 0.5)];

						ofs << "(" << facePoints[j].X << " " << facePoints[j].Y << ") |";
						ofs << "(" << depthPoint.X << " " << depthPoint.Y << " " << depthVal << ") |";
						ofs << "(" << cameraPoint.X << " " << cameraPoint.Y << " " << cameraPoint.Z << ") |";
					}
					ofs << '\n';
					//						ofs << "(" << facePoints[0].X << "," << facePoints[0].Y << "),(" << facePoints[1].X << "," << facePoints[1].Y << "),(" << facePoints[2].X << "," << facePoints[2].Y << "),(" << facePoints[3].X << "," << facePoints[3].Y << "),(" << facePoints[4].X << "," << facePoints[4].Y << ")," << '\n';
					//						ofs << "face bbox: bottom (X,Y); left (X,Y); right (X,Y); top (X,Y);" << '\n';
					ofs << "(" << faceBox.Bottom << "," << faceBox.Left << "," << faceBox.Right << "," << faceBox.Top << ")" << '\n';

					if (cameraSpacePoints)
					{
						delete[] cameraSpacePoints;
						cameraSpacePoints = nullptr;
					}
					if (depthSpacePoints)
					{
						delete[] depthSpacePoints;
						depthSpacePoints = nullptr;
					}

					if (SUCCEEDED(hr))
					{
						hr = pFaceFrameResult->get_FaceRotationQuaternion(&faceRotation);
					}
					//					ofs << "face orientation quaternion: (X,Y,Z,W);" << '\n';
					ofs << "(" << faceRotation.x << "," << faceRotation.y << "," << faceRotation.z << "," << faceRotation.w << ")" << '\n';

					if (SUCCEEDED(hr))
					{
						hr = pFaceFrameResult->GetFaceProperties(FaceProperty::FaceProperty_Count, faceProperties);
						ofs << faceProperties[0] << "," << faceProperties[1] << "," << faceProperties[2] << "," << faceProperties[4] << "," << faceProperties[5] << "," << faceProperties[6] << "," << faceProperties[7] << '\n';
					}

					if (SUCCEEDED(hr))
					{
						hr = GetFaceTextPositionInColorSpace(ppBodies[iFace], &faceTextLayout);
					}

					if (SUCCEEDED(hr))
					{
						// draw face frame results
						// m_pDrawDataStreams->DrawFaceFrameResults(iFace, &faceBox, facePoints, &faceRotation, faceProperties, &faceTextLayout);
					}
				}
				else
				{
					ofs << "Null frame in face tracker" << '\n';
				}

				SafeRelease(pFaceFrameResult);
			}
            else 
            {	
                // face tracking is not valid - attempt to fix the issue
                // a valid body is required to perform this step
                if (bHaveBodyData)
                {
                    // check if the corresponding body is tracked 
                    // if this is true then update the face frame source to track this body
                    IBody* pBody = ppBodies[iFace];
                    if (pBody != nullptr)
                    {
                        BOOLEAN bTracked = false;
                        hr = pBody->get_IsTracked(&bTracked);

                        UINT64 bodyTId;
                        if (SUCCEEDED(hr) && bTracked)
                        {
                            // get the tracking ID of this body
                            hr = pBody->get_TrackingId(&bodyTId);
                            if (SUCCEEDED(hr))
                            {
                                // update the face frame source with the tracking ID
                                m_pFaceFrameSources[iFace]->put_TrackingId(bodyTId);
                            }
                        }
                    }
                }
            }
        }			

        SafeRelease(pFaceFrame);
    }

    if (bHaveBodyData)
    {
        for (int i = 0; i < _countof(ppBodies); ++i)
        {
            SafeRelease(ppBodies[i]);
        }
    }
}

/// <summary>
/// Computes the face result text position by adding an offset to the corresponding 
/// body's head joint in camera space and then by projecting it to screen space
/// </summary>
/// <param name="pBody">pointer to the body data</param>
/// <param name="pFaceTextLayout">pointer to the text layout position in screen space</param>
/// <returns>indicates success or failure</returns>
HRESULT CFaceBasics::GetFaceTextPositionInColorSpace(IBody* pBody, D2D1_POINT_2F* pFaceTextLayout)
{
    HRESULT hr = E_FAIL;

    if (pBody != nullptr)
    {
        BOOLEAN bTracked = false;
        hr = pBody->get_IsTracked(&bTracked);

        if (SUCCEEDED(hr) && bTracked)
        {
            Joint joints[JointType_Count]; 
            hr = pBody->GetJoints(_countof(joints), joints);
            if (SUCCEEDED(hr))
            {
                CameraSpacePoint headJoint = joints[JointType_Head].Position;
                CameraSpacePoint textPoint = 
                {
                    headJoint.X + c_FaceTextLayoutOffsetX,
                    headJoint.Y + c_FaceTextLayoutOffsetY,
                    headJoint.Z
                };

                ColorSpacePoint colorPoint = {0};
                hr = m_pCoordinateMapper->MapCameraPointToColorSpace(textPoint, &colorPoint);

                if (SUCCEEDED(hr))
                {
                    pFaceTextLayout->x = colorPoint.X;
                    pFaceTextLayout->y = colorPoint.Y;
                }
            }
        }
    }

    return hr;
}

/// <summary>
/// Updates body data
/// </summary>
/// <param name="ppBodies">pointer to the body data storage</param>
/// <returns>indicates success or failure</returns>
HRESULT CFaceBasics::UpdateBodyData(IBody** ppBodies)
{
    HRESULT hr = E_FAIL;

    if (m_pBodyFrameReader != nullptr)
    {
        IBodyFrame* pBodyFrame = nullptr;
        hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);
        if (SUCCEEDED(hr))
        {
            hr = pBodyFrame->GetAndRefreshBodyData(BODY_COUNT, ppBodies);
			hr = pBodyFrame->get_FloorClipPlane(&m_floorClipFrame);
        }
        SafeRelease(pBodyFrame);    
    }

    return hr;
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
/// <param name="showTimeMsec">time in milliseconds to ignore future status messages</param>
/// <param name="bForce">force status update</param>
/// <returns>success or failure</returns>
bool CFaceBasics::SetStatusMessage(_In_z_ WCHAR* szMessage, ULONGLONG nShowTimeMsec, bool bForce)
{
    ULONGLONG now = GetTickCount64();

    if (m_hWnd && (bForce || (m_nNextStatusTime <= now)))
    {
        SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);
        m_nNextStatusTime = now + nShowTimeMsec;

        return true;
    }

    return false;
}

