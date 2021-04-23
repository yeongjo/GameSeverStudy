#include <iostream>
#include <thread>

using namespace std;

volatile bool flag = false;
int sync_data = 0;

void consumer() {
	while (flag == false);
	cout << "i received" << sync_data << endl;
}

void producer() {
	sync_data = 999;
	flag = true;
}


void main() {
	thread con{ consumer };
	thread pro{ producer };
	con.join();
	pro.join();
}
