#include <iostream>
#include <thread>

void do_thread()
{
	std::cout << "I'm a another thread.\n";
}

int main()
{
	std::thread t1 { do_thread };
    std::cout << "Hello World!\n";
	t1.join();
}
