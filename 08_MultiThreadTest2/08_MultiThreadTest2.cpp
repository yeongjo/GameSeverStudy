#include <iostream>
#include <mutex>
#include <thread>

using namespace std;

//volatile int sum = 0;
//volatile int v = 0;
//volatile bool flags[2] = { false, false };

// 아토믹을 써서도 할 수 있다.
atomic<int> sum = 0;
atomic<int> v = 0;
atomic<bool> flags[2] = { false, false };

void p_lock(int t_id)
{
	int other = 1 - t_id;
	flags[t_id] = true;
	v = t_id;
	atomic_thread_fence(memory_order_seq_cst); // 안넣으면 오류가 생겼었다.
	while ((true == flags[other]) && (v == t_id));
}

void p_unlock(int t_id)
{
	flags[t_id] = false;
}

void worker(int t_id)
{
	for (int i = 0; i < 25'000'000; ++i)
	{
		// 락을 안걸고 sum을 아토믹으로 만들어도 될거같지만 안된다.
		// 읽고(sum+2) 쓰는(sum=sum+2) 사이에 다른 스레드가 읽어가 버리면 소용이 없어진다.
		// 제대로 하고싶으면 sum += 2;를 하면 제대로 작동된다.
		//p_lock(t_id);
		//sum = sum + 2;
		 sum += 2;
		//p_unlock(t_id);
	}
}

void main()
{
	for (int i = 0; i < 1'000'000; ++i)
	{
		thread con{ worker, 0 };
		thread pro{ worker, 1 };
		con.join();
		pro.join();
		if (sum != 100'000'000) {
			cout << i <<")"<< "wrong sum : " << sum << endl;
		}
		cout << "good result!\n";
		sum = 0;
	}
	
}

// ============================================
//
//#include <iostream>
//#include <mutex>
//#include <thread>
//
//using namespace std;
//
//volatile bool flag = false;
//int sync_data = 0;
//
//void consumer() {
//	while (flag == false);
//	cout << "i received" << sync_data << endl;
//}
//
//void producer() {
//	sync_data = 999;
//	flag = true;
//}
//
//
//void main() {
//	thread con{ consumer };
//	this_thread::sleep_for(0.1s);
//	thread pro{ producer };
//	con.join();
//	pro.join();
//}
