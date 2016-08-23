#define _CRT_SECURE_NO_WARNINGS 1
#include "PerformanceProFiler.h"
#include <thread>

//void test1()
//{
//	PerformanceProfiler pp;
//	PPSection* s1 = pp.CreateSection(__FILE__, __FUNCTION__, __LINE__, "���ݿ�");
//	s1->Begin();
//	Sleep(500);
//	s1->End();
//
//	PPSection* s2 = pp.CreateSection(__FILE__, __FUNCTION__, __LINE__, "����");
//	s2->Begin();
//	Sleep(1000);
//	s2->End();
//	pp.Output();
//}



void Test()
{
	PERFORMANCE_PROFILER_EE_BEGIN(PP1, "PP1");
	
	Sleep(1000);

	PERFORMANCE_PROFILER_EE_END(PP1);

	PERFORMANCE_PROFILER_EE_BEGIN(PP2, "PP2");

	Sleep(500);

	PERFORMANCE_PROFILER_EE_END(PP2);



}

//ʹ�����ü���������ݹ����⣬�ݹ�ʱ���ÿ�ζ�����ʱ�䣬ʱ��Ͳ����ۼ�ÿ�ζ����ʼ��Ϊ0���������н�������ۼ�ʱ��Ҳ���������ֻ�е����ü���Ϊ��ʱ��ʼ�����ۼ�ʱҲֻ�е����ü���Ϊ0ʱ��ʼ����

void Run(int n)
{
	while (n--)
	{

		PERFORMANCE_PROFILER_EE_BEGIN(network, "���紫��");
		Sleep(1000);
		PERFORMANCE_PROFILER_EE_END(network);

		PERFORMANCE_PROFILER_EE_BEGIN(mid, "�м��߼�");
		Sleep(500);
		PERFORMANCE_PROFILER_EE_END(mid);

		PERFORMANCE_PROFILER_EE_BEGIN(sql, "���ݿ�");
		Sleep(500);
		PERFORMANCE_PROFILER_EE_END(sql);
	}
}

void testThread()
{
	thread t1(Run, 3);
	thread t2(Run, 2);
	thread t3(Run, 1);

	t1.join();
	t2.join();
	t3.join();
}


int main()
{
	//test1();
	//Test();
	testThread();
	PerformanceProfiler::GetInstance()->OutPut();
	system("pause");
	return 0;
}