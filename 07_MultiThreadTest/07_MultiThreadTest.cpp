#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

const auto MAX_THREADS = 64;
const auto LOOP_COUNT = 500000000;
using namespace std;
using namespace std::chrono;

volatile int sum;

void thread_func(int num_threads) {
	for (auto i = 0; i < LOOP_COUNT / num_threads; ++i)
		sum = sum + 2;
}

int main() {
	vector<thread> threads;
	for (auto i = 1; i <= MAX_THREADS; i *= 2) {
		sum = 0;
		threads.clear();
		auto start = high_resolution_clock::now();

		for (auto j = 0; j < i; ++j)
			threads.emplace_back(thread{ thread_func, i });

		for (auto& tmp : threads)
			tmp.join();

		auto duration = high_resolution_clock::now() - start;
		cout << i << " Threads" << "  Sum = " << sum;
		cout << " Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}