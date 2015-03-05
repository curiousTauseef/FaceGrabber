#include "PCLViewer.h"
#include <future>

PCLViewer::PCLViewer(int cloudCount, std::string viewerName) :
	m_cloudCount(cloudCount),
	m_viewerName(viewerName),
	m_coloredClouds(cloudCount),
	m_nonColoredClouds(cloudCount),
	m_useColoredCloud(true),
	m_isRunning(false),
	m_updateThread(&PCLViewer::updateLoop, this),
	m_viewPortConfigurationChanged(false),
	m_cloudUpdated()
{
	useColoredCloud(m_useColoredCloud);
}


PCLViewer::~PCLViewer()
{
	m_updateThread.join();	
}

void PCLViewer::stopViewer()
{
	std::unique_lock<std::mutex> lock(m_cloudMutex);
	m_isRunning = false;
	m_cloudUpdate.notify_all();
}

void PCLViewer::updateNonColoredCloudThreated(pcl::PointCloud<pcl::PointXYZ>::ConstPtr cloud, int index)
{
	std::async(std::launch::async, &PCLViewer::pushNewNonColoredCloudAtIndex, this, cloud, index);
}
void PCLViewer::updateColoredCloudThreated(pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr cloud, int index)
{
	std::async(std::launch::async, &PCLViewer::pushNewColoredCloudAtIndex, this, cloud, index);
}

void PCLViewer::pushNewColoredCloudAtIndex(pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr cloud, int index)
{
	if (index >= m_cloudCount){
		return;
	}
	std::unique_lock<std::mutex> lock(m_cloudMutex);
	m_coloredClouds[index] = cloud;
	m_cloudUpdated[index] = true;
	bool notify = true;
	for (auto& isUpdated : m_cloudUpdated){
		notify &= isUpdated;
	}
	if (notify){
		m_cloudUpdate.notify_all();
		m_isRunning = true;
	}
	//
	//if (!m_isRunning){
	//	int numOfSetClouds = 0;
	//
	//	for (auto& cloud : m_coloredClouds){
	//		if (cloud){
	//			numOfSetClouds++;
	//		}
	//	}
	//	std::lock_guard<std::mutex> lock(m_viewPortConfigurationChangedMutex);
	//	if (numOfSetClouds != m_cloudCount){
	//		return;
	//	}
	//	m_isRunning = true;
	//	m_cloudUpdate.notify_all();
	//}
	//else{
	//	m_cloudUpdate.notify_all();
	//}
}

void PCLViewer::pushNewNonColoredCloudAtIndex(pcl::PointCloud<pcl::PointXYZ>::ConstPtr cloud, int index)
{
	if (index >= m_cloudCount){
		return;
	}
	std::unique_lock<std::mutex> lock(m_cloudMutex);
	m_nonColoredClouds[index] = cloud;
	
	m_cloudUpdated[index] = true;
	bool notify = true;
	for (auto& isUpdated : m_cloudUpdated){
		notify &= isUpdated;
	}
	if (notify){
		m_cloudUpdate.notify_all();
		m_isRunning = true;
	}
	//if (!m_isRunning){
	//	int numOfSetClouds = 0;
	//	for (auto& cloud : m_nonColoredClouds){
	//		if (!cloud){
	//			numOfSetClouds++;
	//		}
	//	}
	//	std::lock_guard<std::mutex> lock(m_viewPortConfigurationChangedMutex);
	//	if (numOfSetClouds != m_cloudCount){
	//		return;
	//	}
	//	m_isRunning = true;
	//	m_cloudUpdate.notify_all();
	//}
	//else{
	//	m_cloudUpdate.notify_all();
	//}
}

void PCLViewer::createViewPortsForViewer(pcl::visualization::PCLVisualizer::Ptr viewer)
{
	m_cloudUpdated.clear();
	m_viewPorts.clear();
	m_cloudIDs.clear();
	m_cloudUpdated.resize(m_cloudCount);
	m_viewPorts.resize(m_cloudCount);
	m_cloudIDs.resize(m_cloudCount);
	float horizontalSplitPlaneWidth = 1.0f / m_cloudCount;
	for (int i = 0; i < m_cloudCount; i++){
		float xMin = horizontalSplitPlaneWidth * i;
		float xMax = horizontalSplitPlaneWidth * (i + 1);
		viewer->createViewPort(xMin, 0, xMax, 1.0, m_viewPorts[i]);
		m_cloudIDs[i] = std::to_string(i);
	}
}

void PCLViewer::matchPointCloudsToViewPorts(pcl::visualization::PCLVisualizer::Ptr viewer)
{

	for (int i = 0; i < m_cloudCount; i++){
		viewer->addPointCloud(m_coloredClouds[i], m_cloudIDs[i], m_viewPorts[i]);
		viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, m_cloudIDs[i]);
		viewer->createViewPortCamera(m_viewPorts[i]);
	}
	viewer->resetCamera();
}
void PCLViewer::updateLoop()
{
	
	//static std::mutex constructorLock;
	//constructorLock.lock();
	//pcl::visualization::PCLVisualizer viewer(m_viewerName);
	std::chrono::milliseconds dura(100);
	pcl::visualization::PCLVisualizer::Ptr viewer(new pcl::visualization::PCLVisualizer(m_viewerName));
	//int viewPortID1;
	//int viewPortID2;
	createViewPortsForViewer(viewer);

	{
		std::unique_lock<std::mutex> lock(m_cloudMutex);
		std::chrono::milliseconds dura(100);

		while (!m_cloudUpdate.wait_for(lock, dura)){
			viewer->spinOnce();
		} 
		if (!m_isRunning){
			return;
		}
		matchPointCloudsToViewPorts(viewer);

	}
	while (!viewer->wasStopped() && m_isRunning)
	{
		{
			std::unique_lock<std::mutex> lock(m_cloudMutex);
			
			while (!m_cloudUpdate.wait_for(lock, dura)){
				viewer->spinOnce();
			}
			if (m_viewPortConfigurationChanged){
				std::lock_guard<std::mutex> lock(m_viewPortConfigurationChangedMutex);
				viewer.reset(new pcl::visualization::PCLVisualizer(m_viewerName));
				createViewPortsForViewer(viewer);
				matchPointCloudsToViewPorts(viewer);
				m_viewPortConfigurationChanged = false;
			}
			for (int i = 0; i < m_cloudCount; i++){
				std::lock_guard<std::mutex> lock(m_useColoredCloudMutex);
				updateCurrentCloudWithIndexAndIdentifier(i, m_cloudIDs[i], viewer);
			}
		}
		viewer->spinOnce(100);
	}
}

void PCLViewer::setNumOfClouds(int numOfClouds)
{
	std::lock_guard<std::mutex> lock(m_viewPortConfigurationChangedMutex);
	if (m_cloudCount != numOfClouds){
		m_cloudCount = numOfClouds;
		m_viewPortConfigurationChanged = true;
	}
}

void PCLViewer::updateColoredCloud(int cloudIndex, std::string cloudID, pcl::visualization::PCLVisualizer::Ptr viewer)
{
	auto& cloud = m_coloredClouds[cloudIndex];
	if (cloud){
		viewer->updatePointCloud(cloud, cloudID);
	}
}

void PCLViewer::updateNonColoredCloud(int cloudIndex, std::string cloudID, pcl::visualization::PCLVisualizer::Ptr viewer)
{
	auto& cloud = m_nonColoredClouds[cloudIndex];
	if (cloud){
		viewer->updatePointCloud(cloud, cloudID);
	}
}

void PCLViewer::useColoredCloud(bool useColored)
{
	std::lock_guard<std::mutex> lock(m_useColoredCloudMutex);
	if (useColored){
		updateCurrentCloudWithIndexAndIdentifier.connect(boost::bind(&PCLViewer::updateColoredCloud, this, _1, _2, _3));
	}
	else{
		updateCurrentCloudWithIndexAndIdentifier.connect(boost::bind(&PCLViewer::updateNonColoredCloud, this, _1, _2, _3));
	}
}

