#include <cassert>
#include <include/libc.hpp>
#include <crc32.hpp>
#include <microthread.hpp>
extern "C" __attribute__((noreturn)) void fastexit(int);
#define FAST_RETURN() { asm volatile("ebreak"); __builtin_unreachable(); }
#define FAST_RETVAL(x) \
		{register long __a0 asm("a0") = (long) (x); \
		asm volatile("ebreak" :: "r"(__a0)); __builtin_unreachable(); }

#ifdef __clang__
#define PUBLIC_API extern "C" __attribute__((used))
#define PUBLIC_RETURN()  return;
#define PUBLIC_RETVAL(x) return (long)(x);
#else
#define PUBLIC_API extern "C" __attribute__((used, naked))
#define PUBLIC_RETURN()  FAST_RETURN()
#define PUBLIC_RETVAL(x) FAST_RETVAL(x)
#endif

#include <include/syscall.hpp>
inline long sys_print(const char* data)
{
	asm ("" ::: "memory");
	return syscall(20, (long) data);
}
inline long sys_longcall(const char* data, int b, int c, int d, int e, int f, int g)
{
	return syscall(21, (long) data, b, c, d, e, f, g);
}


namespace std {
	void __throw_bad_alloc() {
		sys_print("exception: bad_alloc thrown\n");
		fastexit(-1);
	}
}

int main(int, const char**)
{
	fastexit(666);
}

PUBLIC_API long selftest(int i, float f, long long number)
{
	uint64_t value = 555ull / number;
	static int bss = 0;
	assert(i == 1234);
	assert(f == 5678.0);
	assert(bss == 0);
	assert(value == 111ull);
	assert(bss == 0);

	// test sending a 64-bit integral value
	uint64_t testvalue = 0x5678000012340000;
	syscall(20, testvalue, testvalue >> 32);

	auto thread = microthread::create(
		[] (int a, int b, long long c) -> long {
			return a + b + c;
		}, 111, 222, 333);
	PUBLIC_RETVAL(microthread::join(thread));
}

#include <array>
static std::array<int, 2001> array;
static int counter = 0;

PUBLIC_API void empty_function()
{
	PUBLIC_RETURN();
}

PUBLIC_API void test(int arg1)
{
	array[counter++] = arg1;
	PUBLIC_RETURN();
}

struct Test {
	int32_t a;
	int64_t b;
};

PUBLIC_API long
test_args(uint32_t a1, Test& a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
	if (a1 == crc32("This is a string") //__builtin_strcmp("This is a string", a1) == 0
	&& (a2.a == 222 && a2.b == 666) && (a3 == 333) && (a4 == 444) && (a5 == 555)
	&& (a6 == 666) && (a7 == 777) && (a8 == 888)) PUBLIC_RETVAL(666);
	PUBLIC_RETVAL(-1);
}

PUBLIC_API long test_maffs(int a1, int a2)
{
	int a = a1 + a2;
	int b = a1 - a2;
	int c = a1 * a2;
	int d = a1 / a2;
	PUBLIC_RETVAL(a + b + c + d);
}

PUBLIC_API void test_print()
{
	sys_print("This is a string");
	PUBLIC_RETURN();
}

PUBLIC_API void test_longcall()
{
	for (int i = 0; i < 10; i++)
		sys_longcall("This is a string", 2, 3, 4, 5, 6, 7);
	PUBLIC_RETURN();
}

PUBLIC_API void test_threads()
{
	microthread::direct(
		[] {
			microthread::yield();
		});
	microthread::yield();
	PUBLIC_RETURN();
}
PUBLIC_API long test_threads_args()
{
	auto thread = microthread::create(
		[] (int a, int b, int c, int d) -> long {
			microthread::yield();
			return a + b + c + d;
		}, 1, 2, 3, 4);
	microthread::yield();
	PUBLIC_RETVAL(microthread::join(thread));
}

uint8_t src_array[300];
uint8_t dst_array[300];
static_assert(sizeof(src_array) == sizeof(dst_array), "!");

PUBLIC_API long test_memcpy()
{
	const char* src = (const char*) src_array;
	char* dest = (char*) dst_array;
	char* dest_end = dest + sizeof(dst_array);
	
	while (dest < dest_end)
		*dest++ = *src++;
	PUBLIC_RETVAL(dest);
}

PUBLIC_API long test_syscall_memcpy()
{
	PUBLIC_RETVAL(memcpy(dst_array, src_array, sizeof(dst_array)));
}


#include <include/event_loop.hpp>
static std::array<Events, 2> events;

PUBLIC_API void event_loop()
{
	while (true) {
		sys_print("event_loop: Checking for work\n");
		for (auto& ev : events) ev.handle();
		sys_print("event_loop: Going to sleep!\n");
		asm volatile("ebreak" ::: "memory");
	}
}

PUBLIC_API bool add_work()
{
	Events::Work work {
		[] {
			sys_print("work: Doing some work!\n");
		}
	};
	sys_print("add_work: Adding work\n");
	for (auto& ev : events)
		if (ev.delegate(work)) PUBLIC_RETVAL(true);
	sys_print("add_work: Not adding work this time\n");
	PUBLIC_RETVAL(false);
}
