#include "PerformanceProFiler.h"

PPSection* PerformanceProfiler::CreateSection(const char* filename, const char* function, int line, const char* desc)
{
	//如果是第一次，那么必须需要查找
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
	//计算不准确，平均值受变化影响不明显
	//需要优化，可参考网络滑动窗口反馈调节计算
	//

	_total += value;
	_avg = _total / (++_count);

}

ResourceStatistics::ResourceStatistics()
	:_refCount(0)
	, _statisticsThread(&ResourceStatistics::_Statistics, this)
{
	//初始化统计资源信息
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
	// 多个线程并行剖析一个代码段的场景下使用引用计数进行统计。
	// 第一个线程进入剖析段时开始统计，最后一个线程出剖析段时
	// 停止统计。
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
		//未开始统计时，使用条件变量阻塞
		//
		if (_refCount == 0)
		{
			unique_lock<mutex> lock(_lockMutex);
			_condVariable.wait(lock);
		}

		//更新统计信息
		_UpdateStatistics();

		//每个CPU时间片单元统计一次
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

// 获取CPU个数
int ResourceStatistics::_GetCpuCount()
{
	SYSTEM_INFO info;
	::GetSystemInfo(&info);

	return info.dwNumberOfProcessors;
}

// 获取内核时间
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

// 获取CPU占用率
LongType ResourceStatistics::_GetCpuUsageRate()
{
	LongType cpuRate = -1;

	// 1.如果是剖析段重新开始的第一次统计，则更新最近的内核时间和系统时间
	if (_lastSystemTime == -1 && _lastKernelTime == -1)
	{
		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
		return cpuRate;
	}

	LongType systemTimeInterval = GetTickCount() - _lastSystemTime;
	LongType kernelTimeInterval = _GetKernelTime() - _lastKernelTime;

	// 2.若耗费的系统时间值小于设定的时间片（CPU_TIME_SLICE_UNIT），则不计入统计。
	if (systemTimeInterval > CPU_TIME_SLICE_UNIT)
	{
		cpuRate = kernelTimeInterval * 100 / systemTimeInterval;
		cpuRate /= _cpuCount;

		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
	}

	return cpuRate;
}

// 获取内存使用信息
LongType ResourceStatistics::_GetMemoryUsage()
{
	PROCESS_MEMORY_COUNTERS PMC;
	if (false == GetProcessMemoryInfo(_processHandle, &PMC, sizeof(PMC)))
	{
		RECORD_ERROR_LOG("GetProcessMemoryInfo Error");
	}

	return PMC.PagefileUsage;
}

// 获取IO使用信息
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

// 更新统计信息
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
	// 将 "ps" 命令的输出 通过管道读取 ("pid" 参数) 到 FILE* stream
	// 将刚刚 stream 的数据流读取到buf中
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
