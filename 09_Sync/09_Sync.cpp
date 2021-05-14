#include <iostream>
#include <thread>
using namespace std;

// 메모리 일관성 테스트
// 메모리의 내용이 한순간에 업데이트 되지 않을 때도 있다.
// 뮤텍스를 걸면 해결된다.

volatile int* bounce;
volatile bool done = false;
int error = 0;

void changer() {
	for (int i = 0; i < 25000000; i++) {
		*bounce = -(1 + *bounce);
	}
	done = true;
}

void checker() {
	while(false == done){
		int v = *bounce;
		if ((v != 0) && (v != -1)){
			error++;
			printf("%X, ", v); // 2바이트 밀리면 0000FFFF나 FFFF0000으로 바뀐다.
			// v가 이상한 값이 되는 순간을 디버깅해서 볼수없다.
		}
	}
}

void main() {
	for (int i = 0; i < 1'000'000; ++i) {
		int arr[17];
		auto addr = reinterpret_cast<long long>(&(arr[16])); // 캐시라인이 64바이트라서 이렇게 하는거같다
		int offset = addr % 64;
		addr = addr - offset;
		addr = addr - 2; //이 한줄이 없을땐 애러가 0이었는데 한줄을 추가한거만으로 애러가 난다.
		bounce = reinterpret_cast<int*>(addr);
		*bounce = 0;
		thread t1{ changer};
		thread t2{ checker};
		t1.join();
		t2.join();
		cout << "error: " << error << endl;
		done = false;
		error = 0;
	}
	// 데이터를 크게 읽어서 캐시에 태그를 붙인다. 실제 데이터의 크기가 캐시보다 커지게 된다. (무슨소리지)
	// 캐시의 구조는 64의 배수로 자른다.
	// 캐시라인이 64바이트인 이유는 벤치마킹을 했을때 가장 빠르고 효율적이어서다
	// 바운더리 문제는 포인트만 안쓰면 알아서 해결됨
	// 구형 cpu에 문제 32비트 cpu는 메모리 버스가 32비트인데 주소가 무조건 4의 배수여야 한번에 읽을수있다. 4의 배수가 아니면 2번에 걸쳐서 읽어야한다. int a의 시작주소가 102주소처럼 2의 배수이면 죽어버린다.
}