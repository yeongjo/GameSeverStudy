#include <iostream>
#include <mutex>
#include <thread>

using namespace std;

volatile int sum = 0;

volatile int v = 0;
volatile bool flags[2] = { false, false };

void p_lock(int t_id)
{
	int other = 1 - t_id;
	flags[t_id] = true;
	v = other;
	while ((true == flags[other]) && v == t_id);
}

void p_unlock(int t_id)
{
	flags[t_id] = false;
}

void worker(int t_id)
{
	for (int i = 0; i < 25'000'000; ++i)
	{
		p_lock(t_id);
		sum = sum + 2;
		p_unlock(t_id);
	}
}

void main()
{
	thread con{ worker, 0 };
	thread pro{ worker, 1};
	con.join();
	pro.join();
	cout << "sum : " << sum << endl;
}
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
