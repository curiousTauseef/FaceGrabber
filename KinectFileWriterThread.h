#pragma once

#include <memory>
#include "RecordingConfiguration.h"
#include "CloudMeasurementSource.h"


template < class PointCloudType >
class KinectFileWriterThread
{
public:
	KinectFileWriterThread();
	~KinectFileWriterThread();

	void writeCloudToFile(int index, IRecordingConfigurationPtr recordingConfiguration);

	void setKinectCloudOutputWriter(CloudMeasurementSource<PointCloudType>* source);

	CloudMeasurementSource<PointCloudType>* m_source;
};

