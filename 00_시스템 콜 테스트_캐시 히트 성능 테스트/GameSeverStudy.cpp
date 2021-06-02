#include <iostream>
#include <chrono>
#include <thread>

using namespace std;
using namespace chrono;

int main()
{
	// 시스템 콜 테스트
	//auto start_t = system_clock::now();
	//long long sum = 0;
	//for (int i = 1; i < 10'000'000; ++i)
	//{
	//	//this_thread::yield(); // 아무것도 안하는 운영체제 시스템 콜 호출
	//	sum += i;
	//}
	//cout << "Sum = " << sum << endl;
	//auto exec_t = system_clock::now() - start_t;
	//int ex_mili = duration_cast<milliseconds>(exec_t).count();
	//cout << "Exec_Time = " << ex_mili << "ms\n";

	// 캐시 히트 성능 테스트
	/*for (int i = 0; i < 21; ++i) {
		int size = (1024 * 1024) << i;
		char* a = (char*)malloc(size);
		unsigned int index = 0;
		volatile unsigned int tmp = 0;
		auto start = high_resolution_clock::now();
		for (int j = 0; j < 100000000; ++j) {
			tmp += a[index % size];
			index += CACHE_LINE_SIZE * 11;
		}
		auto dur = high_resolution_clock::now() - start;
	}*/
	return 0;
}