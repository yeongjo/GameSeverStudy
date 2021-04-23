#include <iostream>
#include <mutex>
#include <thread>

using namespace std;

volatile bool flag = false;
int sync_data = 0;
mutex ss;

void consumer() {
	ss.lock();
	while (flag == false);
	ss.unlock();
	cout << "i received" << sync_data << endl;
}

void producer() {
	sync_data = 999;
	ss.lock();
	flag = true;
	ss.unlock();
}


void main() {
	thread con{ consumer };
	this_thread::sleep_for(1s);
	thread pro{ producer };
	con.join();
	pro.join();
}
