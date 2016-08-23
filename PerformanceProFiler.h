#define _CRT_SECURE_NO_WARNINGS 1
#pragma once
#include <iostream>
using namespace std;
#include <time.h>
#include <map>
#include <Windows.h>
#include <assert.h>
#include <stdarg.h>
#include <algorithm>

// C++11
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

#ifdef _WIN32
#include <Windows.h>
#include<Psapi.h>
#pragma comment(lib,"Psapi.lib")
#else
#include <pthread.h>
#endif // _WIN32

static int GetThreadId()
{
#ifdef _WIN32
	return ::GetCurrentThreadId();
#else
	return ::thread_self();
#endif
}

//������������Ŀ�ܽ᣺
//1.���������--������ӡ�洢��Ϣ�Լ�ʱ��������Ϣ
//		  ����������--������ѡ������ǿ����ǹأ���ǰ̨��ӡ�����ļ���ӡ
//		  ����ģʽ--����ֻ����һ������ʹ�����Ķ��ٿ������Լ�����
//2.��ɺ��ĵĳ�ԱPPNode--�����ļ�������������line�Լ�������Ϣ�����д�ӡ��Ϣ�ĺ���
//3.��ɺ��ĳ�Ա2---PPSection---�������еĺķ�ʱ���Լ��߳���δ�������⣬��Ƶĳ�Ա�������8��9������map<int,LongType>�洢�̵߳����ü���
//�߳��ܹ��ĵ��ô������ķ�ʱ��ȣ�Ȼ�����ܹ������ü������ܹ��ĺķ�ʱ�䣬�ܹ��ĵ��ô�����
//4.����ĵĲ��֣�������Ա����Ϊ1.���캯���������˳��������ӡ��Ϣ�ĳ�ʽ
//					2.compare�ȽϺ������������ݴ�ӡʱ����ʲô˳����
//					3.Output������������Ҫ��ӡ����Ϣȫ��������������㶨--��ǿ��Ϊ���̰߳�ȫ������Ҫ������
//5.begin��end��Ҳ�Ǻ���Ҫ�ģ��ⲿ���û����϶����õ�����begin��end�����������ü���������̵߳�˼���ǿ����Ҫ�������ü���������������ʱ��Ŀ�ʼ�������Ȼ���еݹ����⣬���ü�������������

////////////////////////////////////////////////////
//����������
enum PP_CONFIG_OPTION
{
	PPCO_NONE = 0,		//��������
	PPCO_PROFILER = 2,	//��������
	PPCO_SAVE_TO_CONSOLE = 4,	//���浽����̨
	PPCO_SAVE_TO_FILE = 8,		//���浽�ļ�
	PPCO_SAVE_BY_CALL_COUNT = 16,	//�����ô������򱣴�
	PPCO_SAVE_BY_COST_TIME = 32,	//�����û���ʱ�併�򱣴�

};

////��������
template<class T>
class Singleton
{
public:
	static T* GetInstance()
	{
		if (_sInstance == NULL)
		{
			unique_lock<mutex> lock(_mutex);
			if (_sInstance == NULL)
			{
				_sInstance = new T();
			}
		}

		return _sInstance;
	}
protected:
	Singleton()
	{}

	static T* _sInstance;
	static mutex _mutex;
};

template<class T>
T* Singleton<T>::_sInstance = NULL;

template<class T>
mutex Singleton<T>::_mutex;

////����ģʽ
//template<class T>
//class Singleton
//{
//public:
//	static T* GetInstance()
//	{
//		assert(_sInstance);
//		return _sInstance;
//	}
//protected:
//	static T* _sInstance;
//
//};
//
//template<class T>
//T* Singleton<T>::_sInstance = new T;

///////////////////////////////////////////////////
//���ù������ò�����
class ConfigManager : public Singleton<ConfigManager>
{
public:
	void SetOptions(int flag)
	{
		_flag = flag;
	}

	int GetOptions()
	{
		return _flag;
	}

	ConfigManager()
		:_flag(PPCO_PROFILER | PPCO_SAVE_TO_CONSOLE | PPCO_SAVE_TO_FILE)
	{}

private:
	int _flag;
};

typedef long long LongType;
/////////////////////////////////////////////////////////////////
//����������
class SaveAdapter
{
public:
	virtual void Save(const char* fmt, ...) = 0;//���麯��
};

class ConsoleAdapter :public SaveAdapter//��������������ʲô��
{
public:
	virtual void Save(const char* format, ...)
	{	
		va_list args;
		va_start(args, format);
		vfprintf(stdout, format, args);
		va_end(args);
	}
};

class FileSaveAdapter :public SaveAdapter
{
public:
	FileSaveAdapter(const char* filename)
	{
		_fout = fopen(filename, "w");
		assert(_fout);
	}

	~FileSaveAdapter()
	{
		if (_fout)
		{
			fclose(_fout);
		}
	}

	virtual void Save(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		vfprintf(_fout, format, args);
		va_end(args);
	}

protected:
	//������
	FileSaveAdapter(const FileSaveAdapter&);
	FileSaveAdapter& operator=(const FileSaveAdapter&);

protected:
	FILE* _fout;
};

//////////////////////////////////
//��Դͳ��
struct ResourceInfo
{
	LongType _peak;//����ֵ
	LongType _avg;//ƽ��ֵ

	LongType _total;//��ֵ
	LongType _count;//����

	ResourceInfo()
		:_peak(0)
		, _avg(0)
		, _total(0)
		,_count(0)
	{}

	void Update(LongType value);
	void Serialize(SaveAdapter& sa)const;
};

//////////////////////////////////////////
//��Դͳ��
class ResourceStatistics
{
public:
	ResourceStatistics();
	~ResourceStatistics();

	//��ʼͳ��
	void StartStatistics();

	//ֹͣͳ��
	void StopStatistics();

	//��ȡCPU/�ڴ���Ϣ
	const ResourceInfo& GetCpuInfo();
	const ResourceInfo& GetMemoryInfo();
private:
	void _Statistics();

#ifdef _WIN32
	//��ȡCPU����/��ȡ�ں�ʱ��
	LongType _GetKernelTime();
	int _GetCpuCount();

	//��ȡCPU/�ڴ�/IO��Ϣ
	LongType _GetCpuUsageRate();
	LongType _GetMemoryUsage();
	void _GetIOUsage(LongType& readBytes, LongType& writeBytes);
	//����ͳ����Ϣ
	void _UpdateStatistics();
#else
	void _UpdateStatistics();
#endif

public:
#ifdef _WIN32
	int _cpuCount;//cpu����
	HANDLE _processHandle; //���̾��
	
	LongType _lastSystemTime; //�����ϵͳʱ��
	LongType _lastKernelTime;//������ں�ʱ��
#else
	int _pid;
#endif //_win32

	ResourceInfo _cpuInfo;	//CPU��Ϣ
	ResourceInfo _memoryInfo;
	atomic<int> _refCount;	//���ü���
	mutex _lockMutex;	//�̻߳�����
	condition_variable _condVariable;	//�����Ƿ����ͳ�Ƶ���������
	thread _statisticsThread;	//ͳ���߳�

};

struct PPNode
{
	string _filename;
	string _function;
	int _line;
	string _desc;

	PPNode(const char* filename, const char* function, int line, const char* desc)
		:_filename(filename)
		, _function(function)
		, _line(line)
		, _desc(desc)
	{}

	bool operator<(const PPNode& node)const
	{
		if (_line < node._line)
			return true;
		else
		{
			if (_function < node._function)
				return true;
			else
				return false;
		}
	}

	bool operator==(const PPNode& p)const
	{
		return _filename == p._filename
			&& _function == p._function
			&& _line == p._line;
	}

	//��ӡPPNode�ڵ���Ϣ
	void Serialize(SaveAdapter& sa)const
	{
		sa.Save("Filename:%s, Function:%s, Line:%d\n", _filename.c_str(),
			_function.c_str(), _line);
	}

};

struct PPSection
{
public:
	PPSection()
		:_beginTime(0)
		, _totalCostTime(0)
		, _totalCallCount(0)
		, _totalRefCount(0)
	{}
	////////////////////////////////////////////
	//�����ܣ�������������һ��map<ppNode,ppSection>����PPNode�����ļ���������������ǰ�У�������Ϣ��PPSection�����˿�ʼʱ�䣬���ô���������ʱ�䣬ÿ���̻߳��ѵ�ʱ�䣬ÿ���̵߳Ĵ��������ü�����
	//ÿ��ÿ�ζ���һ��������󣬶���Ҫ�ȵ��ù��캯��,����__FUNCTION__�ᶨλ����ǰ����,��Ȼ��ǰ���������ݽ��в�����BEGIN�������㿪ʼʱ�䣬�Լ����ü��������ô�����END�����������ü���������ʱ��
	///////////////////////////////////////////////////

	//�̴߳�ӡ��Ϣ
	void Serialize(SaveAdapter& sa)
	{
		//����ܵ����ü���������0����ʾ�����β�ƥ��
		if (_totalRefCount)
			sa.Save("Performance Profiler Not Match!\n");
		//���л�Ч��ͳ����Ϣ
		auto costTimeIt = _costTimeMap.begin();
		for (; costTimeIt != _costTimeMap.end(); ++costTimeIt)
		{
			LongType callCount = _callCountMap[costTimeIt->first];
			sa.Save("Thread Id:%d, Cost Time:%.2f,Call Count:%d\n",
				costTimeIt->first, (double)costTimeIt->second / CLOCKS_PER_SEC, callCount);
		}

		sa.Save("Total CostTime:%.2f Total Call Count:%d\n",
			(double)_totalCostTime / CLOCKS_PER_SEC, _totalCallCount);
	}

	void Begin(int threadId)//int threadId)
	{
		unique_lock<mutex> lock(_mutex);
		//���µ��ô���
		++_callCountMap[threadId];
		//auto& refCount = _refCountMap[threadId];
		if (_refCountMap[threadId] == 0)
		{
			_beginTimeMap[threadId] = clock();
			//��ʼͳ����Դ
		}
		++_refCountMap[threadId];
		++_totalRefCount;
		++_totalCallCount;
	}

	void End(int threadId)
	{
		unique_lock<mutex> lock(_mutex);
		//�������ü���
		LongType refCount = --_refCountMap[threadId];
		--_totalRefCount;
		//���ü���<=0ʱ�����������λ��ѵ�ʱ��
		if (refCount <= 0)
		{
			//////////////////////////////////////////
			map<int, LongType>::iterator it = _beginTimeMap.find(threadId);
			if (it != _beginTimeMap.end())
			{
				LongType costTime = clock() - it->second;
				if (_refCountMap[threadId] == 0)
				{
					_costTimeMap[threadId] += costTime;
				}
				else
					_costTimeMap[threadId] = costTime;

				_totalCostTime += costTime;
			}
		}
	}
	
	//����
	//<threadid,��Դͳ��>
	map<int, LongType> _beginTimeMap;//��ʼʱ��ͳ��
	map<int, LongType> _costTimeMap;//����ʱ��ͳ��
	map<int, LongType> _callCountMap;//���ô���ͳ��
	map<int, LongType> _refCountMap;//���ü���ͳ��


	time_t _beginTime;//�ܵĿ�ʼʱ��
	time_t _totalCostTime;//�ܵĻ���ʱ��
	int _totalCallCount;//�ܵĵ��ô���
	int _totalRefCount;//�ܵ����ü���
	mutex _mutex;
};


class PerformanceProfiler: public Singleton<PerformanceProfiler>//ΪʲôҪ�õ�������Ϊ��һ����Ա����map���԰Ѳ�ͬ�ĺ��������뵽map��ȥ�������öഴ���࣬���Ե���������
{
	
public:
	friend class Singleton<PerformanceProfiler>;
	PPSection* CreateSection(const char* filename, const char* function, int line, const char* desc);

	//void Output()
	//{
	//	ConsoleAdapter csa;//ʹ�õ������ã�ÿ������̨�̳�����������Save������������ӡ�ļ�������������line��������Ϣ
	//	_Output(csa);
	//	
	//	FileSaveAdapter fsa("PerFormanceProfileReport.txt");//ͬ��
	//	_Output(fsa);
	//}

	static void OutPut();

protected:
	static bool CompareByCallCount(map<PPNode, PPSection*>::iterator lhs,
		map<PPNode, PPSection*>::iterator rhs);
	static bool CompareByCostTime(map<PPNode, PPSection*>::iterator lhs,
		map<PPNode, PPSection*>::iterator rhs);

	PerformanceProfiler()
	{
		// �������ʱ����������
		atexit(OutPut);

		time(&_beginTime);
	}

	// ������л���Ϣ
	void _Output(SaveAdapter& sa)//Ϊ������ʹ����vector�ܹ�����sort����ֻҪ�Լ����COMPARE���������ˣ�Ҳ���Ƿº���
	{
		sa.Save("==================Performance Profiler Report==============\n\n");
		sa.Save("Profiler Begin Time: %s\n", ctime(&_beginTime));
		unique_lock<mutex> lock(_mutex);
		vector<map<PPNode,PPSection*>::iterator> vInfos;
	
		auto it = _ppMap.begin();//PPSection��Ϊ��ѯֵ,���汣������ʱ�䣬���д�������ʼʱ��ͽ���ʱ��

		for (; it != _ppMap.end(); ++it)
		{
			vInfos.push_back(it);
		}

		//������������������������������
		int flag = ConfigManager::GetInstance()->GetOptions();
		if (flag&PPCO_SAVE_BY_COST_TIME)
			sort(vInfos.begin(), vInfos.end(), CompareByCostTime);
		else
			sort(vInfos.begin(), vInfos.end(), CompareByCallCount);

		for (int index = 0; index < vInfos.size(); ++index)
		{
			sa.Save("NO%d. Description:%s\n", index + 1, vInfos[index]->first._desc.c_str());
			vInfos[index]->first.Serialize(sa);
			vInfos[index]->second->Serialize(sa);
			sa.Save("\n");
		}

		sa.Save("================================end=======================\n\n");
		//while (it != _ppMap.end())//���е���Ϣ���洢����map��map<PPNode,PPSection*>,PPNode��Ϊ��ֵ�����汣�����ļ�������������line������
		//{
		//	sa.Save("NO%d. Desc:%s\n", num++, it->first._desc.c_str());
		//	sa.Save("Filename:%s Function:%s, Line:%d\n", it->first._filename.c_str(), it->first._function.c_str(), it->first._line);
		//	sa.Save("CostTime:%.2f,CallCount:%d\n\n", (double)it->second->_costTime / 1000, it->second->_callCount);

		//	++it;
		//}
	}
private:
	map<PPNode, PPSection*> _ppMap;
	time_t _beginTime;
	mutex _mutex;
};

//atexit
struct Release
{
	~Release()
	{
		PerformanceProfiler::GetInstance()->OutPut();
	}

};

//static Release rl;
//#define PERFORMANCE_PROFILE_RS
///////////////////////////////////////////////////////////////
//�������ܶο���
#define PERFORMANCE_PROFILER_EE_BEGIN(sign,desc)  \
	PPSection* sign##section = NULL;	\
		if (ConfigManager::GetInstance()->GetOptions()&PPCO_PROFILER)\
		{\
			sign##section = PerformanceProfiler::GetInstance()->CreateSection(__FILE__, __FUNCTION__, __LINE__, desc); \
			sign##section ->Begin(GetThreadId());\
		}


#define PERFORMANCE_PROFILER_EE_END(sign) \
if(sign##section)\
	sign##section->End(GetThreadId())


//��������ѡ��
#define SET_PERFORMANCE_PROFILER_OPTIONS(flag)	\
	ConfigManager::GetInstance()->SetOptions(flag)


;