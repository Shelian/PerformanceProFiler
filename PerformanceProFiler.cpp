#include "PerformanceProFiler.h"

PPSection* PerformanceProfiler::CreateSection(const char* filename, const char* function, int line, const char* desc)
{
	//����ǵ�һ�Σ���ô������Ҫ����
	PPSection* pps = NULL;
	PPNode node(filename, function, line, desc);
	unique_lock<mutex> Lock(_mutex);
	
	map<PPNode, PPSection*>::iterator it = _ppMap.find(node);
	if (it != _ppMap.end())
	{
		pps = it->second;
	}
	else
	{
		pps = new PPSection;
		_ppMap[node] = pps;
	}

	return pps;
}

void PerformanceProfiler::OutPut()
{
	int flag = ConfigManager::GetInstance()->GetOptions();
	if (flag & PPCO_SAVE_TO_CONSOLE)
	{
		ConsoleAdapter csa;
		PerformanceProfiler::GetInstance()->_Output(csa);
	}
	if (flag & PPCO_SAVE_TO_FILE)
	{
		FileSaveAdapter fsa("PerformanceProfilerReport.txt");
		PerformanceProfiler::GetInstance()->_Output(fsa);
	}
}



bool PerformanceProfiler::CompareByCallCount(map<PPNode, PPSection*>::iterator lhs,
	map<PPNode, PPSection*>::iterator rhs)
{
	return lhs->second->_totalCallCount > rhs->second->_totalCallCount;
}

bool PerformanceProfiler::CompareByCostTime(map<PPNode, PPSection*>::iterator lhs,
	map<PPNode, PPSection*>::iterator rhs)
{
	return lhs->second->_totalCostTime > rhs->second->_totalCostTime;
}

void ResourceInfo::Update(LongType value)
{
	if (value < 0)
		return;
	if (value >_peak)
		_peak = value;

	//
	//���㲻׼ȷ��ƽ��ֵ�ܱ仯Ӱ�첻����
	//��Ҫ�Ż����ɲο����绬�����ڷ������ڼ���
	//

	_total += value;
	_avg = _total / (++_count);

}

ResourceStatistics::ResourceStatistics()
	:_refCount(0)
	, _statisticsThread(&ResourceStatistics::_Statistics, this)
{
	//��ʼ��ͳ����Դ��Ϣ
#ifdef _WIN32
	_lastKernelTime = -1;
	_lastSystemTime = -1;
	_processHandle = ::GetCurrentProcess();
	_cpuCount = _GetCpuCount();
#else
	_pid = getpid();
#endif
}

ResourceStatistics::~ResourceStatistics()
{
#ifdef _WIN32
	CloseHandle(_processHandle);
#endif
}

void ResourceStatistics::StartStatistics()
{
	//
	// ����̲߳�������һ������εĳ�����ʹ�����ü�������ͳ�ơ�
	// ��һ���߳̽���������ʱ��ʼͳ�ƣ����һ���̳߳�������ʱ
	// ֹͣͳ�ơ�
	//
	if (_refCount++ == 0)
	{
		unique_lock<mutex> lock(_lockMutex);
		_condVariable.notify_one();
	}
}

void ResourceStatistics::StopStatistics()
{
	if (_refCount > 0)
	{
		--_refCount;
		_lastKernelTime = -1;
		_lastSystemTime = -1;
	}
}

const ResourceInfo& ResourceStatistics::GetMemoryInfo()
{
	return _memoryInfo;
}

//////////////////////////////////
//ResourceStatistics

static const int CPU_TIME_SLICE_UNIT = 100;

void ResourceStatistics::_Statistics()
{
	while (1)
	{
		//
		//δ��ʼͳ��ʱ��ʹ��������������
		//
		if (_refCount == 0)
		{
			unique_lock<mutex> lock(_lockMutex);
			_condVariable.wait(lock);
		}

		//����ͳ����Ϣ
		_UpdateStatistics();

		//ÿ��CPUʱ��Ƭ��Ԫͳ��һ��
		this_thread::sleep_for(std::chrono::milliseconds(CPU_TIME_SLICE_UNIT));
	}
}

#ifdef _WIN32
// FILETIME->long long
static LongType FileTimeToLongType(const FILETIME& fTime)
{
	LongType time = 0;

	time = fTime.dwHighDateTime;
	time <<= 32;
	time += fTime.dwLowDateTime;

	return time;
}

// ��ȡCPU����
int ResourceStatistics::_GetCpuCount()
{
	SYSTEM_INFO info;
	::GetSystemInfo(&info);

	return info.dwNumberOfProcessors;
}

// ��ȡ�ں�ʱ��
LongType ResourceStatistics::_GetKernelTime()
{
	FILETIME createTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;

	if (false == GetProcessTimes(GetCurrentProcess(),
		&createTime, &exitTime, &kernelTime, &userTime))
	{
		RECORD_ERROR_LOG("GetProcessTimes Error");
	}

	return (FileTimeToLongType(kernelTime) + FileTimeToLongType(userTime)) / 10000;
}

// ��ȡCPUռ����
LongType ResourceStatistics::_GetCpuUsageRate()
{
	LongType cpuRate = -1;

	// 1.��������������¿�ʼ�ĵ�һ��ͳ�ƣ������������ں�ʱ���ϵͳʱ��
	if (_lastSystemTime == -1 && _lastKernelTime == -1)
	{
		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
		return cpuRate;
	}

	LongType systemTimeInterval = GetTickCount() - _lastSystemTime;
	LongType kernelTimeInterval = _GetKernelTime() - _lastKernelTime;

	// 2.���ķѵ�ϵͳʱ��ֵС���趨��ʱ��Ƭ��CPU_TIME_SLICE_UNIT�����򲻼���ͳ�ơ�
	if (systemTimeInterval > CPU_TIME_SLICE_UNIT)
	{
		cpuRate = kernelTimeInterval * 100 / systemTimeInterval;
		cpuRate /= _cpuCount;

		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
	}

	return cpuRate;
}

// ��ȡ�ڴ�ʹ����Ϣ
LongType ResourceStatistics::_GetMemoryUsage()
{
	PROCESS_MEMORY_COUNTERS PMC;
	if (false == GetProcessMemoryInfo(_processHandle, &PMC, sizeof(PMC)))
	{
		RECORD_ERROR_LOG("GetProcessMemoryInfo Error");
	}

	return PMC.PagefileUsage;
}

// ��ȡIOʹ����Ϣ
void ResourceStatistics::_GetIOUsage(LongType& readBytes, LongType& writeBytes)
{
	IO_COUNTERS IOCounter;
	if (false == GetProcessIoCounters(_processHandle, &IOCounter))
	{
		RECORD_ERROR_LOG("GetProcessIoCounters Error");
	}

	readBytes = IOCounter.ReadTransferCount;
	writeBytes = IOCounter.WriteTransferCount;
}

// ����ͳ����Ϣ
void ResourceStatistics::_UpdateStatistics()
{
	_cpuInfo.Update(_GetCpuUsageRate());
	_memoryInfo.Update(_GetMemoryUsage());

	/*
	LongType readBytes, writeBytes;
	GetIOUsage(readBytes, writeBytes);
	_readIOInfo.Update(readBytes);
	_writeIOInfo.Update(writeBytes);
	*/
}

#else // Linux
void ResourceStatistics::_UpdateStatistics()
{
	char buf[1024] = { 0 };
	char cmd[256] = { 0 };
	sprintf(cmd, "ps -o pcpu,rss -p %d | sed 1,1d", _pid);

	//
	// �� "ps" �������� ͨ���ܵ���ȡ ("pid" ����) �� FILE* stream
	// ���ո� stream ����������ȡ��buf��
	// http://www.cnblogs.com/caosiyang/archive/2012/06/25/2560976.html
	FILE *stream = ::popen(cmd, "r");
	::fread(buf, sizeof (char), 1024, stream);
	::pclose(stream);

	double cpu = 0.0;
	int rss = 0;
	sscanf(buf, "%lf %d", &cpu, &rss);

	_cpuInfo.Update(cpu);
	_memoryInfo.Update(rss);
}
#endif
