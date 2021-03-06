#include <array>
#include <cstring>
static std::array<int, 200*2001> array;
static int counter = 0;

void reset_native_tests()
{
	counter = 0;
}

void test_1_native()
{
	array[counter++] = 44;
}
__attribute__((noinline))
long native_sieve(const long N)
{
	bool prime[N];
	std::memset(prime, true, sizeof(prime));
	long nprimes = 0;
	for (long n = 2; n < N; n++)
	{
		if (prime[n]) {
			nprimes += 1;
			for (long i = 2*n; i < N; i += n)
				prime[i] = false;
		}
	}
	return nprimes;
}

extern void test_9_native_sieve()
{
	native_sieve(10000000);
}
