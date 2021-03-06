#include "stdafx.h"
#include "WindowsApplication.h"
#include <windowsx.h>

WindowsApplication::WindowsApplication() :
	m_nNextStatusTime(0),
	m_FPSLimit(0),
	m_isKinectRunning(true),
	m_bufferSynchronizer(true),
	m_pDrawDataStreams(nullptr),
	m_pD2DFactory(nullptr)
{
	
}

SharedRecordingConfiguration WindowsApplication::initRecordDataModel()
{
	SharedRecordingConfiguration recordingConfigurations(RECORD_CLOUD_TYPE_COUNT);
	for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
		recordingConfigurations[i] = std::shared_ptr<RecordingConfiguration>(new RecordingConfiguration(static_cast<RecordCloudType>(i), PCD_BINARY));
		recordingConfigurations[i]->recordConfigurationStatusChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(RecordCloudType, bool)>(&RecordTabHandler::recordConfigurationStatusChanged), &m_recordTabHandler, _1, _2));
		recordingConfigurations[i]->recordPathOrFileNameChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(RecordCloudType)>(&RecordTabHandler::recordPathChanged), &m_recordTabHandler, _1));
		recordingConfigurations[i]->setThreadCountToStart(2);
		recordingConfigurations[i]->setKinectVersion(KinectV2);
	}
	return recordingConfigurations;
}

SharedCommonConfiguration WindowsApplication::initCommonDataModel()
{
	SharedCommonConfiguration commonConfigurations(COMMON_CONFIGURATION_TYPE_COUNT);
	for (int i = 0; i < COMMON_CONFIGURATION_TYPE_COUNT; i++){
		commonConfigurations[i] = std::shared_ptr<CommonConfiguration>(new CommonConfiguration);
		//recordingConfigurations[i]->recordConfigurationStatusChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(RecordCloudType, bool)>(&RecordTabHandler::recordConfigurationStatusChanged), &m_recordTabHandler, _1, _2));
		//recordingConfigurations[i]->recordPathOrFileNameChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(RecordCloudType)>(&RecordTabHandler::recordPathChanged), &m_recordTabHandler, _1));
		commonConfigurations[i]->setThreadCountToStart(2);
	}
	return commonConfigurations;
}


SharedImageRecordingConfiguration WindowsApplication::initImageRecordDataModel()
{
	SharedImageRecordingConfiguration recordingConfigurations(IMAGE_RECORD_TYPE_COUNT);
	for (int i = 0; i < IMAGE_RECORD_TYPE_COUNT; i++){
		recordingConfigurations[i] = std::shared_ptr<ImageRecordingConfiguration>(new ImageRecordingConfiguration(static_cast<ImageRecordType>(i), PNG));
		recordingConfigurations[i]->recordConfigurationStatusChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(ImageRecordType, bool)>(&RecordTabHandler::recordConfigurationStatusChanged), &m_recordTabHandler, _1, _2));
		recordingConfigurations[i]->recordPathOrFileNameChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(ImageRecordType)>(&RecordTabHandler::recordPathChanged), &m_recordTabHandler, _1));
		recordingConfigurations[i]->setThreadCountToStart(2);
		recordingConfigurations[i]->setKinectVersion(KinectV2);
	}
	return recordingConfigurations;
}

SharedImageRecordingConfiguration WindowsApplication::initImageRecordDataModelForKinectV1()
{
	SharedImageRecordingConfiguration recordingConfigurations(V1_IMAGE_RECORD_TYPE_COUNT);
	for (int i = 0; i < V1_IMAGE_RECORD_TYPE_COUNT; i++){
		recordingConfigurations[i] = std::shared_ptr<ImageRecordingConfiguration>(new ImageRecordingConfiguration(static_cast<ImageRecordType>(i), PNG));
		recordingConfigurations[i]->recordConfigurationStatusChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(ImageRecordType, bool)>(&RecordTabHandler::recordConfigurationStatusChanged), &m_recordTabHandler, _1, _2));
		recordingConfigurations[i]->recordPathOrFileNameChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(ImageRecordType)>(&RecordTabHandler::recordPathChanged), &m_recordTabHandler, _1));
		recordingConfigurations[i]->setThreadCountToStart(2);
		recordingConfigurations[i]->setKinectVersion(KinectV1);
	}
	return recordingConfigurations;
}


SharedStringFileRecordingConfiguration WindowsApplication::initStringFileRecordDataModel()
{
	SharedStringFileRecordingConfiguration recordingConfigurations(STRING_FILE_RECORD_TYPE_COUNT);
	for (int i = 0; i < STRING_FILE_RECORD_TYPE_COUNT; i++){
		recordingConfigurations[i] = std::shared_ptr<StringFileRecordingConfiguration>(new StringFileRecordingConfiguration(static_cast<StringFileRecordType>(i), StringFileRecordingFileFormat::TXT));

		recordingConfigurations[i]->recordConfigurationStatusChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(StringFileRecordType, bool)>(&RecordTabHandler::recordConfigurationStatusChanged), &m_recordTabHandler, _1, _2));
		recordingConfigurations[i]->recordPathOrFileNameChanged.connect(boost::bind(static_cast<void (RecordTabHandler::*)(StringFileRecordType)>(&RecordTabHandler::recordPathChanged), &m_recordTabHandler, _1));
		recordingConfigurations[i]->setThreadCountToStart(2);
		recordingConfigurations[i]->setKinectVersion(KinectV2);
	}
	return recordingConfigurations;
}


WindowsApplication::~WindowsApplication()
{
	SafeRelease(m_pD2DFactory);
	m_bufferSynchronizer.onApplicationQuit();
	m_bufferSynchronizerThread.join();
}

void WindowsApplication::UpdateStreams(int i)
{
	//_CrtMemState s1, s2, s3;
	//_CrtMemCheckpoint(&s1);
	
	if(m_kinectV1Enable)
	{
		m_kinectV1Controller.Update();
		/*
		switch(i)
		{
			case 1:
				m_kinectV1Controller.ProcessColor();
				break;
			case 2:
				m_kinectV1Controller.ProcessDepth();
				break;
			default:break;
		}
		*/

		const int depthFps =  30;//30;
		const int halfADepthFrameMs = (1000 / depthFps) / 2;
		const LARGE_INTEGER lastColorFrameStampp = m_kinectV1Controller.getLastColorFrameStamp();
		const LARGE_INTEGER lastDepthFrameStampp = m_kinectV1Controller.getLastDepthFrameStamp();
		bool needUpdateWriter = true;
		// If we have not yet received any data for either color or depth since we started up, we shouldn't draw
		if (lastColorFrameStampp.QuadPart == 0 || lastDepthFrameStampp.QuadPart == 0)
		{
			needUpdateWriter = false;
		}

		// If the color frame is more than half a depth frame ahead of the depth frame we have,
		// then we should wait for another depth frame.  Otherwise, just go with what we have.
		if (lastDepthFrameStampp.QuadPart - lastDepthFrameStampp.QuadPart > halfADepthFrameMs)
		{
			needUpdateWriter = false;
		}
	
		if(needUpdateWriter)
		{
			m_kinectV1DataUpdateMutex.lock();
			m_kinectV1Controller.updateWriter();
			m_kinectV1DataUpdateMutex.unlock();
		}
	//Sleep(halfADepthFrameMs);
	}
	return ;
}
/// <summary>
/// Returns the window handle
/// </summary>
/// <returns>Handle to the window</returns>
HWND WindowsApplication::GetWindow() const
{
    return m_hWnd;
}

DWORD WindowsApplication::runKinectV2Update(WindowsApplication * pThis)
{
	clock_t start;
	double timedelta;
	while(pThis->m_kinectV2Enable)
	{
		if (pThis->m_isKinectRunning)
		{
			pThis->m_kinectFrameGrabber.update();
		
			if (pThis->m_FPSLimit != 0)
			{
				// timedelta in seconds
				timedelta = (double(clock() - start)) / CLOCKS_PER_SEC;
				// if faster than specified target fps: sleep
				if (timedelta < (1.0 / pThis->m_FPSLimit)) Sleep(((1.0 / pThis->m_FPSLimit) - timedelta) * 1000);
			}
				
			start = clock();
		}
	}
	
	return 0;
}


DWORD WindowsApplication::runKinectV1StreamEvent(WindowsApplication * pThis)
{
	
	clock_t start;
	double timedelta;

	 HANDLE events[] = {
						pThis->m_hStopStreamEventThread,
						pThis->m_hPauseStreamEventThread,
						pThis->m_kinectV1Controller.getCorlorFrameEvent(),
						pThis->m_kinectV1Controller.getDepthFrameEvent(),
						pThis->m_kinectV1Controller.getBodyFrameEvent(),
						pThis->m_kinectV1Controller.getRemoveBGFrameEvent()
						} ;


    while (true)
    {
		
		
        DWORD ret = WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE, INFINITE);
		
        if (WAIT_OBJECT_0 == ret)
            break;
				
		else if( WAIT_OBJECT_0 + 1 == ret)
		{
			
			//Sleep(pThis->);
		}
		else {
			pThis->UpdateStreams(0);
			/*
			if( WAIT_OBJECT_0 == WaitForSingleObject(pThis->m_kinectV1Controller.getDepthFrameEvent(), 0))
			{
				pThis->UpdateStreams(2);

			}
			if (WAIT_OBJECT_0 == WaitForSingleObject(pThis->m_kinectV1Controller.getCorlorFrameEvent(), 0) )  //+ 1 
			{				
				pThis->UpdateStreams(1);					
			}
			*/
		}

	}

    return 0;
}

int WindowsApplication::run(HINSTANCE hInstance, int nCmdShow)
{

	MSG       msg = { 0 };
	WNDCLASS  wc;

	// Dialog custom window class
	ZeroMemory(&wc, sizeof(wc));
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
	wc.lpfnWndProc = DefDlgProcW;
	wc.lpszClassName = L"KinectHDFaceGrabberAppDlgWndClass";
	m_hInstance = hInstance;
	if (!RegisterClassW(&wc))
	{
		return 0;
	}

	// Create main application window
	HWND hWndApp = CreateDialogParamW(
		NULL,
		MAKEINTRESOURCE(IDD_APP_TAB),
		NULL,
		(DLGPROC)MessageRouterHelper::MessageRouter,
		reinterpret_cast<LPARAM>(this));

	// Show window
	ShowWindow(hWndApp, nCmdShow);

	clock_t start;
	double timedelta;
	double actualFPS;
	std::stringstream FPSinfo;
	CString msgCstring;

	HWND tabControlHandle = GetDlgItem(m_hWnd, IDC_TAB2);
	HANDLE hv1EventThread;
	
	//Sleep(30);

	HANDLE hEventThread;
	HANDLE hEventThread1;

	// creat a thread to capture  frames of V1
	if(m_kinectV1Enable)
	{
		m_hStopStreamEventThread = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		m_hPauseStreamEventThread = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		hEventThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)runKinectV1StreamEvent, this, 0, nullptr); //
		
	}

	// creat a thread to capture  frames of V2
	if(m_kinectV2Enable)
	{
		hEventThread1 = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)runKinectV2Update, this, 0, nullptr); //
	}

	start = clock();
	while (WM_QUIT != msg.message)
	{
		//start = clock();
		
		//while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		while (GetMessageW(&msg, NULL, 0, 0))
		{
			// If a dialog message will be taken care of by the dialog proc
			if (hWndApp && IsDialogMessageW(hWndApp, &msg))
			{
				continue;
			}

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

#if 0
		int sel = TabCtrl_GetCurSel(tabControlHandle);

		// Record Tab active
		if (sel == 0){
			/*
			FPSinfo.str("");
			FPSinfo.clear();
			FPSinfo << "Tab Sel: " << sel;
			msgCstring = CString(FPSinfo.str().c_str());
			msgCstring += L"\n";
			OutputDebugString(msgCstring);
			*/

			if (m_FPSLimit != 0)
			{
				// timedelta in seconds
				timedelta = (double(clock() - start)) / CLOCKS_PER_SEC;
				// if faster than specified target fps: sleep
				if (timedelta < (1.0 / m_FPSLimit)) Sleep(((1.0 / m_FPSLimit) - timedelta) * 1000);
			}

			actualFPS = CLOCKS_PER_SEC / (float(clock() - start));
			start = clock();
			/*
			FPSinfo.str("");
			FPSinfo.clear();
			FPSinfo << "UPDATE Loop actual fps: " << actualFPS << " target fps: " << m_FPSLimit;
			msgCstring = CString(FPSinfo.str().c_str());
			msgCstring += L"\n";
			OutputDebugString(msgCstring);
			*/
		}
		// Playback/Convert Tab active
		else{
			// Limit CPU usage from while(WM_QUIT != msg.message)
			Sleep(10);
		}
		//Sleep(5);
#endif
	}
	if(m_kinectV1Enable)
	{
		SetEvent(m_hStopStreamEventThread);

		WaitForSingleObject(hEventThread, INFINITE);
		CloseHandle(hEventThread);
	}
	if(m_kinectV2Enable)
	{
		m_kinectV2Enable = false;
		WaitForSingleObject(hEventThread1, INFINITE);
		CloseHandle(hEventThread1);
	}
	
	
	return static_cast<int>(msg.wParam);
}

/**
 * \fn	void WindowsApplication::onCreate()
 *
 * \brief	Executes the create action.
 *
 * \author	Martin
 * \date	17.03.2015
 */

void WindowsApplication::onCreate()
{


	HRESULT hr;
	//Create the pcl viewer
	


	initKinectFrameGrabber();

	initKinectV1FrameGrabber();
				
	m_recordTabHandler.setKinectEnableOpt(m_kinectV1Enable,m_kinectV2Enable);

	initTabs();
	
	m_pclFaceViewer = std::shared_ptr<PCLViewer>(new PCLViewer(2, "Face-Viewer"));

	if(m_kinectV2Enable)
	{
		//m_pclFaceViewer = std::shared_ptr<PCLViewer>(new PCLViewer(2, "Face-Viewer"));
		initCloudWriter();
		
		initStringFileWriter();
		connectStreamUpdaterToViewer();

		
	}

	initImageWriter();
	initInputReaderBufferAndSynchronizer();
	




}

int WindowsApplication::insertTabItem(HWND tab, LPTSTR text, int tabid)
{
	TCITEM ti = { 0 };
	ti.mask = TCIF_TEXT;
	ti.pszText = text;
	ti.cchTextMax = wcslen(text);
	return TabCtrl_InsertItem(tab, tabid, &ti);
}

void WindowsApplication::disconnectStreamUpdaterFromViewer()
{
	//disconnect all slots, so e.g. the PCLViewer does not get updated any more
	for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
		m_uncoloredOutputStreamUpdater->cloudUpdated[i].disconnect_all_slots();
		m_coloredOutputStreamUpdater->cloudUpdated[i].disconnect_all_slots();
	}
	m_uncoloredOutputStreamUpdater->cloudsUpdated.disconnect_all_slots();
	m_coloredOutputStreamUpdater->cloudsUpdated.disconnect_all_slots();
}


void WindowsApplication::initCloudWriter()
{
	for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
		//init
		auto uncoloredCloudWriter = std::shared_ptr<KinectCloudFileWriter<pcl::PointXYZ>>(new KinectCloudFileWriter<pcl::PointXYZ>);
		//register for events
		uncoloredCloudWriter->updateStatus.connect(boost::bind(static_cast<void (RecordTabHandler::*)(RecordCloudType, std::wstring)>(&RecordTabHandler::updateWriterStatus), &m_recordTabHandler, static_cast<RecordCloudType>(i), _1));
		uncoloredCloudWriter->writingWasStopped.connect(boost::bind(&RecordTabHandler::recordingStopped, &m_recordTabHandler));

		m_uncoloredCloudOutputWriter.push_back(uncoloredCloudWriter);

		//init
		auto coloredCloudWriter = std::shared_ptr<KinectCloudFileWriter<pcl::PointXYZRGB>>(new KinectCloudFileWriter<pcl::PointXYZRGB>);
		//register for events
		coloredCloudWriter->updateStatus.connect(boost::bind(static_cast<void (RecordTabHandler::*)(RecordCloudType, std::wstring)>(&RecordTabHandler::updateWriterStatus), &m_recordTabHandler, static_cast<RecordCloudType>(i), _1));
		coloredCloudWriter->writingWasStopped.connect(boost::bind(&RecordTabHandler::recordingStopped, &m_recordTabHandler));

		m_colorCloudOutputWriter.push_back(coloredCloudWriter);
	}
}

void WindowsApplication::initImageWriter()
{
	if(m_kinectV2Enable)
	{
		for (int i = 0; i < IMAGE_RECORD_TYPE_COUNT; i++){
			//init
			auto imageWriter = std::shared_ptr<KinectRawFileWriter>(new KinectRawFileWriter);
			//register for events
			imageWriter->updateStatus.connect(boost::bind(static_cast<void (RecordTabHandler::*)(ImageRecordType, std::wstring)>(&RecordTabHandler::updateWriterStatus), &m_recordTabHandler, static_cast<ImageRecordType>(i), _1));
			imageWriter->writingWasStopped.connect(boost::bind(&RecordTabHandler::recordingStopped, &m_recordTabHandler));

			m_imageOutputWriter.push_back(imageWriter);
		}
	}
	if(m_kinectV1Enable)
	{
		for (int i = 0; i < V1_IMAGE_RECORD_TYPE_COUNT; i++)
		{
			//init
			auto imageWriter = std::shared_ptr<KinectRawFileWriter>(new KinectRawFileWriter);
			//register for events
			//imageWriter->updateStatus.connect(boost::bind(static_cast<void (RecordTabHandler::*)(ImageRecordType, std::wstring)>(&RecordTabHandler::updateWriterStatus), &m_recordTabHandler, static_cast<ImageRecordType>(i), _1));
			imageWriter->writingWasStopped.connect(boost::bind(&RecordTabHandler::recordingStopped, &m_recordTabHandler));

			m_kinectV1ImageOutputWriter.push_back(imageWriter);
		}	
	}
}
void WindowsApplication::initStringFileWriter()
{
	for (int i = 0; i < STRING_FILE_RECORD_TYPE_COUNT; i++){
		//init
		auto imageWriter = std::shared_ptr<StringFileWriter>(new StringFileWriter);
		//register for events
		imageWriter->updateStatus.connect(boost::bind(static_cast<void (RecordTabHandler::*)(StringFileRecordType, std::wstring)>(&RecordTabHandler::updateWriterStatus), &m_recordTabHandler, static_cast<StringFileRecordType>(i), _1));
		imageWriter->writingWasStopped.connect(boost::bind(&RecordTabHandler::recordingStopped, &m_recordTabHandler));

		m_stringFileOutputWriter.push_back(imageWriter);
	}
}



void WindowsApplication::connectStreamUpdaterToViewer()
{
	for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
		//we skip enabling the fulldepth raw and kinect raw; this signal is connected 
		//only if recording is enabled; this way, full depth buffer conversion is skipped 
		//if not recorded
		if (i == FullDepthRaw || i == HDFace2D){
			continue;
		}
		//bind to the writer
		m_uncoloredOutputStreamUpdater->cloudUpdated[i].connect(boost::bind(&KinectCloudFileWriter<pcl::PointXYZ>::pushCloudAsync, m_uncoloredCloudOutputWriter[i], _1));
		m_coloredOutputStreamUpdater->cloudUpdated[i].connect(boost::bind(&KinectCloudFileWriter<pcl::PointXYZRGB>::pushCloudAsync, m_colorCloudOutputWriter[i], _1));
	}
	//bind to the viewer
	m_uncoloredOutputStreamUpdater->cloudsUpdated.connect(boost::bind(&PCLViewer::updateUncoloredClouds, m_pclFaceViewer, _1));
	m_coloredOutputStreamUpdater->cloudsUpdated.connect(boost::bind(&PCLViewer::updateColoredClouds, m_pclFaceViewer, _1));
}

HRESULT WindowsApplication::initKinectFrameGrabber()
{
	HRESULT hr;

	m_kinectV2Enable = false;
	hr = m_kinectFrameGrabber.initializeDefaultSensor();
	if(FAILED(hr))
	{
		return hr;
	}
	
			// Init Direct2D
	// Create and initialize a new Direct2D image renderer 
	//set the settings of the views and frames 
	

	// Get and initialize the default Kinect sensor
	
	int depthWidth, depthHeight, colorWidth, colorHeight;

	m_kinectFrameGrabber.getColourAndDepthSize(depthWidth, depthHeight, colorWidth, colorHeight);

	//init stragedy
	m_coloredOutputStreamUpdater = std::shared_ptr<ColoredOutputStreamUpdater>(new ColoredOutputStreamUpdater);
	m_uncoloredOutputStreamUpdater = std::shared_ptr<UncoloredOutputStreamsUpdater>(new UncoloredOutputStreamsUpdater);

	//set the colored as default
	m_kinectFrameGrabber.setOutputStreamUpdater(m_coloredOutputStreamUpdater);

	//register for status messages (HDFace grabbing state)
	m_kinectFrameGrabber.statusChanged.connect(boost::bind(&WindowsApplication::setStatusMessage, this, _1, _2));

	//init the stragedy
	m_coloredOutputStreamUpdater->initialize(m_kinectFrameGrabber.getCoordinateMapper(), depthWidth, depthHeight, colorWidth, colorHeight);
	m_uncoloredOutputStreamUpdater->initialize(m_kinectFrameGrabber.getCoordinateMapper(), depthWidth, depthHeight, colorWidth, colorHeight);

	m_kinectV2Enable = true;

	return hr;
}

HRESULT WindowsApplication::initKinectV1FrameGrabber()
{
	HRESULT hr ;
	m_kinectV1Enable = false;
	

	if(SUCCEEDED(hr = m_kinectV1Controller.init()))
	{
		m_kinectV1Enable = true;
		m_kinectV1OutputStreamUpdater = std::shared_ptr<KinectV1OutPutStreamUpdater>(new KinectV1OutPutStreamUpdater);
		m_kinectV1Controller.setOutPutStreamUpdater(m_kinectV1OutputStreamUpdater);
				
	}

	return hr;
}

void WindowsApplication::initTabs()
{
	auto recordingConfiguration = initRecordDataModel();
	auto commonConfiguration = initCommonDataModel();
	m_recordTabHandler.setSharedCommonConfiguration(commonConfiguration);

	m_recordTabHandler.setSharedRecordingConfiguration(recordingConfiguration);
	if(m_kinectV2Enable)
	{
		
		auto imageRecordingConfiguration = initImageRecordDataModel();
		auto stringFileRecordingConfiguration = initStringFileRecordDataModel();
				
		m_recordTabHandler.setSharedImageRecordingConfiguration(imageRecordingConfiguration);
		m_recordTabHandler.setSharedStringStringRecordingConfiguration(stringFileRecordingConfiguration);
		
	}
	if(m_kinectV1Enable)
	{
		auto recordingConfiguration =initImageRecordDataModelForKinectV1();
		m_recordTabHandler.setSharedImageRecordingConfigurationForKinectV1(recordingConfiguration);
	}
	//record, playback and convert are "subdialogs" with own message routers. They process their UI messages on their own
	//we will show only one of those subdialogs once
	//init record tab
	m_recordTabHandle = CreateDialogParamW(
		NULL,
		MAKEINTRESOURCE(IDC_TAB_RECORD),
		m_hWnd,
		(DLGPROC)MessageRouterHelper::MessageRouter,
		reinterpret_cast<LPARAM>(&m_recordTabHandler));
	m_recordTabHandler.colorConfigurationChanged.connect(boost::bind(&WindowsApplication::colorStreamingChangedTo, this, _1));
	m_recordTabHandler.centerConfigurationChanged.connect(boost::bind(&WindowsApplication::centerRecordingChangedTo, this, _1));
	m_recordTabHandler.fpsLimitUpdated.connect(boost::bind(&WindowsApplication::setFPSLimit, this, _1,_2));

	m_recordTabHandler.startWriting.connect(boost::bind(&WindowsApplication::startRecording, this, _1, _2, _3,_4,_5));

	m_recordTabHandler.stopWriting.connect(boost::bind(&WindowsApplication::stopRecording, this, _1, _2, _3,_4,_5));

	m_plackBackTabHandler.setSharedRecordingConfiguration(recordingConfiguration);

	if(m_kinectV2Enable)
	{
		m_kinectFrameGrabber.SetConfiguration(commonConfiguration[KinectV2_COMMON]);
		commonConfiguration[KinectV2_COMMON]->setShowOpt(Color_Raw);
	}
	if(m_kinectV1Enable)
	{
		m_kinectV1Controller.SetConfiguration(commonConfiguration[KinectV1_COMMON]);
		m_recordTabHandler.v1ShowOptChanged.connect(boost::bind(&KinectV1Controller::showOptUpdated, &m_kinectV1Controller, _1));
		m_recordTabHandler.v1ShowResolutionChanged.connect(boost::bind(&KinectV1Controller::showResolutionUpdated, &m_kinectV1Controller, _1));
		m_recordTabHandler.v1RecordingResolutionChanged.connect(boost::bind(&KinectV1Controller::recordingResolutionUpdated, &m_kinectV1Controller, _1,_2)); 
		m_recordTabHandler.kinectV1AlignmentEnable.connect(boost::bind(&WindowsApplication::setKinectV1AlignmentEnable, this, _1));
		m_recordTabHandler.kinectV1RemoveBGEnable.connect(boost::bind(&WindowsApplication::removeKinectV1BG, this, _1));
	}

	//init playbackt tab
	m_playbackTabHandle = CreateDialogParamW(
		NULL,
		MAKEINTRESOURCE(IDC_TAB_PLAYBACK),
		m_hWnd,
		(DLGPROC)MessageRouterHelper::MessageRouter,
		reinterpret_cast<LPARAM>(&m_plackBackTabHandler));
	ShowWindow(m_playbackTabHandle, SW_HIDE);
	m_plackBackTabHandler.startPlayback.connect(boost::bind(&WindowsApplication::startPlayback, this, _1, _2));
	m_plackBackTabHandler.stopPlayback.connect(boost::bind(&WindowsApplication::stopPlayback, this));

	//init conver tab
	m_convertTabHandle = CreateDialogParamW(
		NULL,
		MAKEINTRESOURCE(IDC_TAB_CONVERT),
		m_hWnd,
		(DLGPROC)MessageRouterHelper::MessageRouter,
		reinterpret_cast<LPARAM>(&m_convertTabHandler));
	ShowWindow(m_convertTabHandle, SW_HIDE);

	

	//create the tab items
	HWND tabControlHandle = GetDlgItem(m_hWnd, IDC_TAB2);	
	insertTabItem(tabControlHandle, L"Record", 0);
	insertTabItem(tabControlHandle, L"Playback", 1);
	insertTabItem(tabControlHandle, L"Convert", 2);
	//set the default
	TabCtrl_SetCurSel(tabControlHandle, 0);
	ShowWindow(tabControlHandle, SW_SHOW);

	//get the areas to adjust the size of the color live stream dialog/view
	RECT windowRect;
	GetClientRect(m_hWnd, &windowRect);

	RECT tabControlRect;
	GetWindowRect(tabControlHandle, &tabControlRect);

	RECT showOptGroupRect;
	HWND tabShowOPTHandle = GetDlgItem(m_recordTabHandle, IDC_GROUP_SHOW_OPT);
	//HWND tabShowOPTHandle = GetDlgItem(tabrecordHandle, IDC_GROUP_SHOW_OPT);	
	GetWindowRect(tabShowOPTHandle, &showOptGroupRect);

	HWND hWndSlider = GetDlgItem(m_recordTabHandle, IDC_TILTANGLE_SLIDER);
	SendMessageW(hWndSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, NUI_CAMERA_ELEVATION_MAXIMUM - NUI_CAMERA_ELEVATION_MINIMUM));
	
	
	
	

	if(m_kinectV2Enable && ! m_kinectV1Enable)
	{
		const int height = showOptGroupRect.top - tabControlRect.top-40; //(width / 16) * 9;
		const int width = (height/9)*16; 
		const int xPos = (windowRect.right - windowRect.left - width) / 2;
		//const int yPos = (showOptGroupRect.top - tabControlRect.top - height) / 2;
		const int yPos = tabControlRect.top;
		m_liveViewWindow = CreateWindow(WC_STATIC, L"", WS_CHILD | WS_VISIBLE, xPos, yPos, width, height, m_hWnd, NULL, m_hInstance, NULL);

		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
		
		if (FAILED(hr))
		{
			setStatusMessage(L"Failed to initialize the Direct2D draw device.", true);
			return;
		}
		m_pDrawDataStreams = new ImageRenderer();
		if(m_pD2DFactory)
			m_pDrawDataStreams->initialize(m_liveViewWindow, m_pD2DFactory, cColorWidth, cColorHeight, cColorWidth * sizeof(RGBQUAD));

		m_kinectFrameGrabber.setImageRenderer(m_pDrawDataStreams);
		//m_kinectFrameGrabber.setImageRenderer(m_pDrawDataStreams);

	}

	else if(!m_kinectV2Enable &&  m_kinectV1Enable)
	{
		
		const int height = showOptGroupRect.top - tabControlRect.top-40; //(width / 16) * 9;
		const int width = (height/3)*4; 
		const int xPos = (windowRect.right - windowRect.left - width) / 2;
		//const int yPos = (showOptGroupRect.top - tabControlRect.top - height) / 2;
		const int yPos = tabControlRect.top;

		m_liveViewWindow_for_v1 = CreateWindow(WC_STATIC, L"", WS_CHILD | WS_VISIBLE, xPos, yPos, width, height, m_hWnd, NULL, m_hInstance, NULL);
		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
		if (FAILED(hr))
		{
			setStatusMessage(L"Failed to initialize the Direct2D draw device.", true);
			return;
		}
		m_pDrawDataStreamsForV1 = new ImageRenderer();
		if(m_pD2DFactory)
			m_pDrawDataStreamsForV1->initialize(m_liveViewWindow_for_v1, m_pD2DFactory, cColorWidthForV1, cColorHeightForV1, cColorWidthForV1 * sizeof(RGBQUAD));

		m_kinectV1Controller.setImageRenderer(m_pDrawDataStreamsForV1,m_pD2DFactory);	

		m_kinectV1Controller.SetIcon(m_liveViewWindow_for_v1);

		LONG degree = m_kinectV1Controller.getTitleDegree();
		SendMessageW(hWndSlider, TBM_SETPOS, TRUE, (LPARAM)(NUI_CAMERA_ELEVATION_MAXIMUM - degree));
		m_recordTabHandler.v1TitleAngleChanged.connect(boost::bind(&WindowsApplication::setTitleAngle, this, _1));
		WCHAR buffer[128];
		swprintf_s(buffer, 128, L"%d\x00B0", degree);
		SetDlgItemTextW(m_recordTabHandle, IDC_ANGLE_STATUS, buffer);


	}
	else if(m_kinectV2Enable &&  m_kinectV1Enable)
	{
		const int height = showOptGroupRect.top - tabControlRect.top-60; //(width / 16) * 9;
		const int width = (height/9)*16; 
		const int xPos = 7;
		m_liveViewWindow = CreateWindow(WC_STATIC, L"", WS_CHILD | WS_VISIBLE, xPos, tabControlRect.top, width, height, m_hWnd, NULL, m_hInstance, NULL);

				
		const int width_v1 = (tabControlRect.right-width -10);
		const int height_v1 = (width_v1 / 4) * 3;
		const int xPos_v1 = xPos+width+10;
		m_liveViewWindow_for_v1 = CreateWindow(WC_STATIC, L"", WS_CHILD | WS_VISIBLE, xPos_v1, tabControlRect.top, width_v1, height_v1, m_hWnd, NULL, m_hInstance, NULL);
		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
		if (FAILED(hr))
		{
			setStatusMessage(L"Failed to initialize the Direct2D draw device.", true);
			return;
		}
		m_pDrawDataStreams = new ImageRenderer();
		if(m_pDrawDataStreams && m_pD2DFactory)
		{
			m_pDrawDataStreams->initialize(m_liveViewWindow, m_pD2DFactory, cColorWidth, cColorHeight, cColorWidth * sizeof(RGBQUAD));
		}
		else
		{
			return ;
		}
				
		
		m_kinectFrameGrabber.setImageRenderer(m_pDrawDataStreams);
		HRESULT hr1 = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactoryForKinectV1);
		if (FAILED(hr1))
		{
			setStatusMessage(L"Failed to initialize the Direct2D draw device.", true);
			return;
		}
		m_pDrawDataStreamsForV1 = new ImageRenderer();
		if(m_pDrawDataStreamsForV1 && m_pD2DFactoryForKinectV1)
		{
			m_pDrawDataStreamsForV1->initialize(m_liveViewWindow_for_v1, m_pD2DFactoryForKinectV1, cColorWidthForV1, cColorHeightForV1, cColorWidthForV1 * sizeof(RGBQUAD));
		}
		else 
		{
			return;
		}
		
		m_kinectV1Controller.setImageRenderer(m_pDrawDataStreamsForV1,m_pD2DFactoryForKinectV1);	

		m_kinectV1Controller.SetIcon(m_liveViewWindow_for_v1);

		LONG degree = m_kinectV1Controller.getTitleDegree();
		WCHAR buffer[128];
		swprintf_s(buffer, 128, L"%d\x00B0", degree);
		SetDlgItemTextW(m_recordTabHandle, IDC_ANGLE_STATUS, buffer);
		SendMessageW(hWndSlider, TBM_SETPOS, TRUE, (LPARAM)(NUI_CAMERA_ELEVATION_MAXIMUM - degree));
		m_recordTabHandler.v1TitleAngleChanged.connect(boost::bind(&WindowsApplication::setTitleAngle, this, _1));
	}

}


void WindowsApplication::initInputReaderBufferAndSynchronizer()
{
	//create the buffers and store them for the synchronizer
	std::vector<std::shared_ptr<Buffer<pcl::PointCloud<pcl::PointXYZRGB>::Ptr>>> buffers;
	for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
		//create buffer & input reader
		auto buffer = std::shared_ptr<Buffer<pcl::PointCloud<pcl::PointXYZRGB>::Ptr>>(new Buffer<pcl::PointCloud<pcl::PointXYZRGB>::Ptr>);
		auto inputReader = std::shared_ptr<PCLInputReader<pcl::PointXYZRGB>>(new PCLInputReader<pcl::PointXYZRGB>());

		//forward the updates of the reader to the playbacktabhandler
		inputReader->updateStatus.connect(boost::bind(&PlaybackTabHandler::updateReaderStatus, &m_plackBackTabHandler, static_cast<RecordCloudType>(i), _1));
		inputReader->setBuffer(buffer);

		//forward the status event of the buffersynchronizer to the playbacktabhandler
		m_bufferSynchronizer.updateStatus.connect(boost::bind(&PlaybackTabHandler::updateReaderStatus, &m_plackBackTabHandler, static_cast<RecordCloudType>(i), _1));

		//store reader & writer
		m_inputFileReader.push_back(inputReader);
		buffers.push_back(buffer);
	}

	//register for the playback finished event
	m_bufferSynchronizer.playbackFinished.connect(boost::bind(&WindowsApplication::onPlaybackFinished, this));

	//start the buffer synchronizer thread => will sleep until buffer notifes
	m_bufferSynchronizerThread = std::thread(&BufferSynchronizer<pcl::PointCloud<pcl::PointXYZRGB>::Ptr>::updateThreadFunc, &m_bufferSynchronizer);
}

void WindowsApplication::onTabSelected(int page)
{
	//only display one of the tabs at once
	//avoid clicks by the user while playback/recording is running
	int iPage = TabCtrl_GetCurSel(GetDlgItem(m_hWnd, IDC_TAB2));
	if (iPage == 0){
		if (m_plackBackTabHandler.isPlaybackRunning()){
			TabCtrl_SetCurSel(GetDlgItem(m_hWnd, IDC_TAB2), 1);
		}
		else{
			onRecordTabSelected();
		}

	}
	else if (iPage == 1){
		if (m_recordTabHandler.isRecording()){
			TabCtrl_SetCurSel(GetDlgItem(m_hWnd, IDC_TAB2), 0);
		}
		else{
			onPlaybackSelected();
		}

	}
	else if (iPage == 2){
		if (m_recordTabHandler.isRecording()){
			TabCtrl_SetCurSel(GetDlgItem(m_hWnd, IDC_TAB2), 0);
		}
		else if (m_plackBackTabHandler.isPlaybackRunning()){
			TabCtrl_SetCurSel(GetDlgItem(m_hWnd, IDC_TAB2), 1);
		}
		else{
			onConvertTabSelected();
		}
	}
}
void WindowsApplication::connectInputReaderToViewer()
{
	m_bufferSynchronizer.publishSynchronizedData.connect(boost::bind(&PCLViewer::updateColoredClouds, m_pclFaceViewer, _1));
}

void WindowsApplication::disconnectInputReaderFromViewer()
{
	m_bufferSynchronizer.publishSynchronizedData.disconnect_all_slots();
}

void WindowsApplication::onRecordTabSelected()
{
	m_isKinectRunning = true;
	m_plackBackTabHandler.playbackStopped();
	
	if(m_kinectV2Enable)
	{
		
		connectStreamUpdaterToViewer();

		m_pclFaceViewer->useColoredCloud(m_recordTabHandler.isColorEnabled());
		m_pclFaceViewer->setNumOfClouds(2);

		ShowWindow(m_liveViewWindow, SW_SHOW);
	}

	if(m_kinectV1Enable)
		ShowWindow(m_liveViewWindow_for_v1, SW_SHOW);

	disconnectInputReaderFromViewer();
	ShowWindow(m_recordTabHandle, SW_SHOW);

	//hide the other views
	ShowWindow(m_playbackTabHandle, SW_HIDE);
	ShowWindow(m_convertTabHandle, SW_HIDE);
}

void WindowsApplication::onConvertTabSelected()
{
	//hide the other views
	
	if(m_kinectV2Enable)
		ShowWindow(m_liveViewWindow, SW_HIDE);
	if(m_kinectV1Enable)
		ShowWindow(m_liveViewWindow_for_v1, SW_HIDE);

	ShowWindow(m_recordTabHandle, SW_HIDE);
	ShowWindow(m_playbackTabHandle, SW_HIDE);

	ShowWindow(m_convertTabHandle, SW_SHOW);
}

void WindowsApplication::onPlaybackSelected()
{
	setStatusMessage(L"", true);
	//stop the updating of the kinect frame grabber
	m_isKinectRunning = false;


	if(m_kinectV2Enable)
	{
		ShowWindow(m_liveViewWindow, SW_HIDE);
		disconnectStreamUpdaterFromViewer();
		

		//preset to default
		m_pclFaceViewer->useColoredCloud(true);
	}

	if(m_kinectV1Enable)
		ShowWindow(m_liveViewWindow_for_v1, SW_HIDE);

	connectInputReaderToViewer();

	m_plackBackTabHandler.resetUIElements();
	m_plackBackTabHandler.setSharedRecordingConfiguration(m_recordTabHandler.getRecordConfiguration());

	ShowWindow(m_recordTabHandle, SW_HIDE);
	ShowWindow(m_playbackTabHandle, SW_SHOW);

	ShowWindow(m_convertTabHandle, SW_HIDE);
}


void WindowsApplication::startRecording(bool isColoredStream, SharedRecordingConfiguration recordingConfigurations, SharedImageRecordingConfiguration imageRecordingConfigurations, SharedStringFileRecordingConfiguration KeyPointsRecordingConfiguration, SharedImageRecordingConfiguration imageRecordingConfigurationsForV1)
{
	if(m_kinectV2Enable)
	{
		for (int i = 0; i < IMAGE_RECORD_TYPE_COUNT; i++){
			auto recordingConfig = imageRecordingConfigurations[i];
			auto imageWriter = m_imageOutputWriter[i];
			imageWriter->setRecordingConfiguration(recordingConfig);

			//disconnect all connected signals
			m_coloredOutputStreamUpdater->imageUpdated[i].disconnect_all_slots();
			m_uncoloredOutputStreamUpdater->imageUpdated[i].disconnect_all_slots();

			if (recordingConfig->isEnabled()){
				//we agreed on manually enabling the Raw image writer, so we do not that image creation if not required
				if (isColoredStream) m_coloredOutputStreamUpdater->imageUpdated[i].connect(boost::bind(&KinectRawFileWriter::pushImageAsync, imageWriter, _1));
				else m_uncoloredOutputStreamUpdater->imageUpdated[i].connect(boost::bind(&KinectRawFileWriter::pushImageAsync, imageWriter, _1));
				imageWriter->startWriting();
			}
		}

		for (int i = 0; i < STRING_FILE_RECORD_TYPE_COUNT; i++){
			auto recordingConfig = KeyPointsRecordingConfiguration[i]; //
			auto stringFileWriter = m_stringFileOutputWriter[i];
			stringFileWriter->setRecordingConfiguration(recordingConfig);

			//disconnect all connected signals
			m_coloredOutputStreamUpdater->keyPointsUpdated[i].disconnect_all_slots();
			m_uncoloredOutputStreamUpdater->keyPointsUpdated[i].disconnect_all_slots();

			if (recordingConfig->isEnabled()){
				//we agreed on manually enabling the Raw image writer, so we do not that image creation if not required
				if (isColoredStream) m_coloredOutputStreamUpdater->keyPointsUpdated[i].connect(boost::bind(&StringFileWriter::pushStringFileAsync, stringFileWriter, _1));
				else m_uncoloredOutputStreamUpdater->keyPointsUpdated[i].connect(boost::bind(&StringFileWriter::pushStringFileAsync, stringFileWriter, _1));
				stringFileWriter->startWriting();
			}
		}

		//use the correct recoder & updater
		if (isColoredStream){
			for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
				auto recordingConfig = recordingConfigurations[i];
				auto cloudWriter = m_colorCloudOutputWriter[i];
				if (i == FullDepthRaw || i == HDFace2D){
					//make sure the FullDepthRaw and HDFace2D was disconnected from last recording session
					//we only want to do that point cloud creation if required
					m_coloredOutputStreamUpdater->cloudUpdated[i].disconnect_all_slots();
					m_uncoloredOutputStreamUpdater->cloudUpdated[i].disconnect_all_slots();
				}
				cloudWriter->setRecordingConfiguration(recordingConfig);
				if (recordingConfig->isEnabled()){
					//we agreed on manually enabling the FullDepthRaw and HDFace2D file writer, so we do not that cloud if not required
					if (i == FullDepthRaw || i == HDFace2D){
						m_coloredOutputStreamUpdater->cloudUpdated[i].connect(boost::bind(&KinectCloudFileWriter<pcl::PointXYZRGB>::pushCloudAsync, cloudWriter, _1));
					}
					cloudWriter->startWriting();
				}
			}
		}
		else{
			for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
				auto recordingConfig = recordingConfigurations[i];
				auto cloudWriter = m_uncoloredCloudOutputWriter[i];
				if (i == FullDepthRaw || i == HDFace2D){
					//make sure the FullDepthRaw and HDFace2D was disconnected from last recording session
					//we only want to do that point cloud creation if required
					m_coloredOutputStreamUpdater->cloudUpdated[i].disconnect_all_slots();
					m_uncoloredOutputStreamUpdater->cloudUpdated[i].disconnect_all_slots();
				}
				cloudWriter->setRecordingConfiguration(recordingConfig);
				if (recordingConfig->isEnabled()){
					//we agreed on manually enabling the FullDepthRaw and HDFace2D file writer, so we do not that cloud if not required
					if (i == FullDepthRaw || i == HDFace2D){
						m_uncoloredOutputStreamUpdater->cloudUpdated[i].connect(boost::bind(&KinectCloudFileWriter<pcl::PointXYZ>::pushCloudAsync, cloudWriter, _1));
					}
					cloudWriter->startWriting();
				}
			}
		}
	}
	if(m_kinectV1Enable)
	{
		m_kinectV1DataUpdateMutex.lock();
		for (int i = 0; i < V1_IMAGE_RECORD_TYPE_COUNT; i++){
			
			if(!m_kinectV1Controller.getAlignmentEnable() && i == KinectAlignedDepthRaw)
			{
				continue;
			}
			auto recordingConfig = imageRecordingConfigurationsForV1[i];
			auto imageWriter = m_kinectV1ImageOutputWriter[i];
			imageWriter->setRecordingConfiguration(recordingConfig);


			m_kinectV1OutputStreamUpdater->imageUpdated[i].disconnect_all_slots();

			if (recordingConfig->isEnabled()){
				//we agreed on manually enabling the Raw image writer, so we do not that image creation if not required
				m_kinectV1OutputStreamUpdater->imageUpdated[i].connect(boost::bind(&KinectRawFileWriter::pushImageAsync, imageWriter, _1));
				imageWriter->startWriting();
			}
		}
		m_kinectV1DataUpdateMutex.unlock();
		
	}
}


void WindowsApplication::triggerReaderStart(SharedPlaybackConfiguration playbackConfig, bool isSingleThreatedReading)
{
	if (playbackConfig.size() != RECORD_CLOUD_TYPE_COUNT){
		return;
	}
	//start all readers; reading for playbackConfig is disable the inputreader will finish immediately
	for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
		m_inputFileReader[i]->setPlaybackConfiguration(playbackConfig[i]);
		m_inputFileReader[i]->startReading(isSingleThreatedReading);
	}
}
void WindowsApplication::setupReaderAndBuffersForPlayback(SharedPlaybackConfiguration playbackConfig)
{
	int enabledClouds = 0;
	std::vector<std::shared_ptr<Buffer<pcl::PointCloud<pcl::PointXYZRGB>::Ptr>>> activeBuffers;

	//we have to specify the number of files which will be read, so the buffersynchronizer can notify the tab
	//when all clouds were rendered
	//we need to find the buffers are used for that playback, so the buffersynchronizer waits only for the enabled
	//ones
	int numOfFilesToRead = 0;
	for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
		auto& currentConfig = playbackConfig[i];
		if (currentConfig->isEnabled()){
			enabledClouds++;
			numOfFilesToRead = currentConfig->getCloudFilesToPlay().size();
			activeBuffers.push_back(m_inputFileReader[i]->getBuffer());
		}
	}

	//set the buffers and adjust the number of clouds to show in the viewer
	m_bufferSynchronizer.setBuffer(activeBuffers, numOfFilesToRead);
	m_pclFaceViewer->setNumOfClouds(enabledClouds);
}

void WindowsApplication::startPlayback(SharedPlaybackConfiguration playbackConfig, bool isSingleThreatedReading)
{
	setupReaderAndBuffersForPlayback(playbackConfig);
	if (isSingleThreatedReading){
		std::async(std::launch::async, &WindowsApplication::triggerReaderStart, this, playbackConfig, isSingleThreatedReading);
	}
	else{
		triggerReaderStart(playbackConfig, isSingleThreatedReading);
	}
}

void WindowsApplication::onPlaybackFinished()
{
	m_plackBackTabHandler.playbackStopped();
}

void WindowsApplication::stopPlayback()
{
	for (auto& inputReader : m_inputFileReader){
		inputReader->stopReading();
	}
}

void WindowsApplication::stopRecording(bool isColoredStream, SharedRecordingConfiguration recordingConfigurations,
									   SharedImageRecordingConfiguration imageRecordingConfigurations,
									   SharedStringFileRecordingConfiguration KeyPointsRecordingConfiguration,
									   SharedImageRecordingConfiguration imageRecordingConfigurationsForV1)
{
	if(m_kinectV2Enable)
	{
		for (int i = 0; i < IMAGE_RECORD_TYPE_COUNT; i++){
			auto recordingConfig = imageRecordingConfigurations[i];
			auto imageWriter = m_imageOutputWriter[i];
			if (recordingConfig->isEnabled() ){
				imageWriter->stopWriting();
			}
			//disconnect all connected signals
			m_coloredOutputStreamUpdater->imageUpdated[i].disconnect_all_slots();
			m_uncoloredOutputStreamUpdater->imageUpdated[i].disconnect_all_slots();
		}

		for (int i = 0; i < STRING_FILE_RECORD_TYPE_COUNT; i++){
			auto recordingConfig = KeyPointsRecordingConfiguration[i];
			auto stringFileWriter = m_stringFileOutputWriter[i];
			if (recordingConfig->isEnabled() ){
				stringFileWriter->stopWriting();
			}
			//disconnect all connected signals
			m_coloredOutputStreamUpdater->keyPointsUpdated[i].disconnect_all_slots();
			m_uncoloredOutputStreamUpdater->keyPointsUpdated[i].disconnect_all_slots();
		}

		//stop the correct writer
		if (isColoredStream){
			for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
				auto recordingConfig = recordingConfigurations[i];
				auto cloudWriter = m_colorCloudOutputWriter[i];

				//stop those which were enabled/started
				if (recordingConfig->isEnabled() ){
					cloudWriter->stopWriting();
				}

				//disconect again so FullDepthRaw and HDFace2D is not created anymore
				if (i == FullDepthRaw || i == HDFace2D){
					m_coloredOutputStreamUpdater->cloudUpdated[i].disconnect_all_slots();
				}
			}
		}
		else{
			for (int i = 0; i < RECORD_CLOUD_TYPE_COUNT; i++){
				auto recordingConfig = recordingConfigurations[i];
				auto cloudWriter = m_uncoloredCloudOutputWriter[i];

				//stop those which were enabled/started
				if (recordingConfig->isEnabled()){
					cloudWriter->stopWriting();
				}
				//disconect again so FullDepthRaw and HDFace2D is not created anymore
				if (i == FullDepthRaw || i == HDFace2D){
					m_uncoloredOutputStreamUpdater->cloudUpdated[i].disconnect_all_slots();
				}
			}
		}
	}

	if(m_kinectV1Enable)
	{
		m_kinectV1DataUpdateMutex.lock();
		for (int i = 0; i < V1_IMAGE_RECORD_TYPE_COUNT; i++){
			if(!m_kinectV1Controller.getAlignmentEnable() && i == KinectAlignedDepthRaw)
			{
				continue;
			}
			auto recordingConfig = imageRecordingConfigurationsForV1[i];
			auto imageWriter = m_kinectV1ImageOutputWriter[i];
			if (recordingConfig->isEnabled() ){
				imageWriter->stopWriting();
			}
			//disconnect all connected signals
			m_kinectV1OutputStreamUpdater->imageUpdated[i].disconnect_all_slots();
			
		}
		m_kinectV1DataUpdateMutex.unlock();
	}
}


void WindowsApplication::colorStreamingChangedTo(bool enable)
{
	//chose the correct implementation for the stragedy
	if (enable){
		m_kinectFrameGrabber.setOutputStreamUpdater(m_coloredOutputStreamUpdater);
	}
	else{
		m_kinectFrameGrabber.setOutputStreamUpdater(m_uncoloredOutputStreamUpdater);
	}
	//let the PCLViwer know which of the clouds will be updated
	m_pclFaceViewer->useColoredCloud(enable);
}

void WindowsApplication::centerRecordingChangedTo(bool enable)
{
	if(m_kinectV2Enable)
	{
		m_coloredOutputStreamUpdater->setCeterEnabled(enable);
		m_uncoloredOutputStreamUpdater->setCeterEnabled(enable);
	}
}

void WindowsApplication::setFPSLimit(int fps,KinectVersionType kinectVersion)
{
	if(kinectVersion==KinectV1)
	{
		m_kinectV1Controller.setLimitedFPS(fps);
	}
	else if(kinectVersion==KinectV2)
	{
		m_FPSLimit = fps;
	}
	
	
}

void WindowsApplication::setKinectV1AlignmentEnable(bool enable)
{

	m_kinectV1Controller.setAlignmentEnable(enable);
}

void WindowsApplication::removeKinectV1BG(bool enable)
{

	m_kinectV1Controller.removeBackGround(enable);
}


bool WindowsApplication::setStatusMessage(std::wstring statusString, bool bForce)
{
	//only update if neccessary
	_In_z_ WCHAR* szMessage = &statusString[0];
	ULONGLONG now = GetTickCount64();
	ULONGLONG nShowTimeMsec = 1000;
	if (m_hWnd && (bForce || (m_nNextStatusTime <= now)))
	{
		SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);
		m_nNextStatusTime = now + nShowTimeMsec;

		return true;
	}

	return false;
}


void  WindowsApplication::setTitleAngle(LONG degree)
{
	m_kinectV1Controller.setTitleDegree(degree);
}

