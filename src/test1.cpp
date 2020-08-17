#include <include/syscall_helpers.hpp>
#include "luascript.hpp"
#include "testhelp.hpp"
#include "include/crc32.hpp"

using namespace riscv;
static std::vector<uint8_t> rvbinary;
static Machine<RISCV32>* machine = nullptr;
static State<RISCV32> state;
static uint32_t test_1_empty_addr = 0x0;
static uint32_t test_1_syscall_addr = 0x0;
static uint32_t test_1_address = 0x0;

struct FunctionAddress {
	uint32_t addr = 0;

	uint32_t get(Machine<RISCV32>* m, const char* func) {
		if (addr) return addr;
		return (addr = m->address_of(func));
	}
};

static Script* luascript = nullptr;
extern const char* TEST_BINARY;
static const std::string str("This is a string");

template <int W>
long syscall_print(Machine<W>& machine)
{
	// get string directly from memory, with max-length
	const auto rvs = machine.template sysarg<riscv::Buffer>(0);
	if (str != rvs.to_string()) {
		abort();
	}
	return 0;
}
template <int W>
long syscall_longcall(Machine<W>& machine)
{
	const auto address = machine.template sysarg<address_type<W>>(0);
	// get string directly from memory, with max-length
	//if (str.compare(machine.memory.memstring(address))) {
	if (machine.memory.memcmp(str.data(), address, str.size())) {
		abort();
	}
	return 0;
}
template <int W>
long syscall_nothing(Machine<W>&)
{
	return 0;
}
template <int W>
long syscall_fmod(Machine<W>& machine)
{
	auto& regs = machine.cpu.registers();
	auto& f1 = regs.getfl(0).f32[0];
	auto& f2 = regs.getfl(1).f32[0];
	f1 = std::fmod(f1, f2);
	return 0;
}
template <int W>
long syscall_powf(Machine<W>& machine)
{
	auto& regs = machine.cpu.registers();
	auto& f1 = regs.getfl(0).f32[0];
	auto& f2 = regs.getfl(1).f32[0];
	f1 = std::pow(f1, f2);
	return 0;
}
template <int W>
long syscall_strcmp(Machine<W>& machine)
{
	const auto a1 = machine.template sysarg<riscv::Buffer>(0);
	const auto a2 = machine.template sysarg<address_type<W>>(2);
	// this is slightly faster because we know one of the lengths
	const std::string str1 = a1.to_string();
	return machine.memory.memcmp(str1.c_str(), a2, str1.size());
}

void test_setup()
{
	if (rvbinary.empty()) rvbinary = load_file(TEST_BINARY);
	delete machine;
	machine = new Machine<RISCV32> {rvbinary, 4*1024*1024};
#ifndef RISCV_DEBUG
	assert(machine->address_of("fastexit") != 0);
	machine->memory.set_exit_address(machine->address_of("fastexit"));
#endif

	// the minimum number of syscalls needed for malloc and C++ exceptions
	setup_minimal_syscalls(state, *machine);
	auto* arena = setup_native_heap_syscalls(*machine, 4*1024*1024);
	setup_native_memory_syscalls(*machine, true);
	auto* threads = setup_native_threads(*machine, arena);
	machine->install_syscall_handler(30, syscall_print<RISCV32>);
	machine->install_syscall_handler(31, syscall_longcall<RISCV32>);
	machine->install_syscall_handler(32, syscall_nothing<RISCV32>);

	machine->install_syscall_handler(33, syscall_fmod<RISCV32>);
	machine->install_syscall_handler(34, syscall_powf<RISCV32>);
	machine->install_syscall_handler(35, syscall_strcmp<RISCV32>);
	machine->setup_argv({"rvprogram"});

	try {
		// run until it stops
		machine->simulate();
	} catch (riscv::MachineException& me) {
		printf(">>> Machine exception %d: %s (data: %d)\n",
				me.type(), me.what(), me.data());
#ifdef RISCV_DEBUG
		machine->print_and_pause();
#endif
	}
	assert(machine->cpu.reg(10) == 0);

	assert(machine->address_of("empty_function") != 0);
	assert(machine->address_of("test") != 0);
	assert(machine->address_of("test_args") != 0);
	assert(machine->address_of("test_maffs1") != 0);
	assert(machine->address_of("test_maffs2") != 0);
	assert(machine->address_of("test_maffs3") != 0);
	assert(machine->address_of("test_print") != 0);
	assert(machine->address_of("test_longcall") != 0);
	assert(machine->address_of("test_memcpy") != 0);
	assert(machine->address_of("test_syscall_memcpy") != 0);
	test_1_empty_addr = machine->address_of("empty_function");
	test_1_address = machine->address_of("test");
	test_1_syscall_addr = machine->address_of("test_syscall");

	delete luascript;
	luascript = new Script("../luaprogram/script.lua");

	extern void reset_native_tests();
	reset_native_tests();
}

void bench_fork()
{
	riscv::MachineOptions<4> options {
		.owning_machine = machine
	};
	riscv::Machine<4> other {rvbinary, options};
}
void bench_install_syscall()
{
	machine->install_syscall_handlers({
		{0,	[] (auto&) {
			return 0L;
		}},
		{1,	[] (auto&) {
			return 0L;
		}},
		{2,	[] (auto&) {
			return 0L;
		}},
		{3,	[] (auto&) {
			return 0L;
		}},
		{4,	[] (auto&) {
			return 0L;
		}},
		{5,	[] (auto&) {
			return 0L;
		}},
		{6,	[] (auto&) {
			return 0L;
		}},
		{7,	[] (auto&) {
			return 0L;
		}}
	});
}

void test_1_riscv_empty()
{
	machine->vmcall<0>(test_1_empty_addr);
}
void test_1_lua_empty()
{
	luascript->call("empty_function");
}

void test_1_riscv()
{
#ifdef RISCV_DEBUG
	try {
		machine->vmcall<0>("test", 555);
	} catch (riscv::MachineException& me) {
		printf(">>> test_1 Machine exception %d: %s (data: %d)\n",
				me.type(), me.what(), me.data());
		machine->print_and_pause();
	}
#else
	machine->vmcall<0>("test", 555);
#endif
}
void test_1_riscv_direct()
{
	machine->vmcall<0>(test_1_address, 555);
}
void test_1_lua()
{
	luascript->call("test", 555);
}

void test_2_riscv()
{
	static FunctionAddress fa;
	const struct Test {
		int32_t a = 222;
		int64_t b = 666;
	} test;
#ifdef RISCV_DEBUG
	try {
		int ret =
		machine->vmcall(fa.get(machine, "test_args"), "This is a string", test,
									333, 444, 555, 666, 777, 888);
		if (ret != 666) abort();
	} catch (riscv::MachineException& me) {
		printf(">>> test_2 Machine exception %d: %s (data: %d)\n",
				me.type(), me.what(), me.data());
		machine->print_and_pause();
	}
#else
	int ret =
	machine->vmcall(fa.get(machine, "test_args"), "This is a string", test,
								333, 444, 555, 666, 777, 888);
	if (ret != 666) abort();
#endif
}
void test_2_lua()
{
	auto tab = luascript->new_table();
	tab[0] = 222;
	tab[1] = 666;
	auto ret =
	luascript->retcall("test_args", "This is a string",
			tab, 333, 444, 555, 666, 777, 888);
	if ((int) ret != 666) abort();
}

void test_3_riscv()
{
	static FunctionAddress fa;
#ifdef RISCV_DEBUG
	try {
		machine->vmcall<0>(fa.get(machine, "test_maffs1"), 111, 222);
	} catch (riscv::MachineException& me) {
		printf(">>> test_3 Machine exception %d: %s (data: %d)\n",
				me.type(), me.what(), me.data());
		machine->print_and_pause();
	}
#else
	machine->vmcall(fa.get(machine, "test_maffs1"), 111, 222);
#endif
}
void test_3_riscv_math2()
{
	static FunctionAddress fa;
	machine->vmcall(fa.get(machine, "test_maffs2"), 3.0, 3.0, 3.0);
}
void test_3_riscv_math3()
{
	static FunctionAddress fa;
	machine->vmcall(fa.get(machine, "test_maffs3"), 3.0, 3.0, 3.0);
}
void test_3_riscv_fib()
{
	static FunctionAddress fa;
	machine->vmcall(fa.get(machine, "test_fib"), 40, 0, 1);
}
void test_3_lua_math1()
{
	luascript->call("test_maffs1", 111, 222);
}
void test_3_lua_math2()
{
	luascript->call("test_maffs2", 3.0, 3.0, 3.0);
}
void test_3_lua_math3()
{
	luascript->call("test_maffs3", 3.0, 3.0, 3.0);
}

void test_4_riscv_syscall()
{
	machine->vmcall(test_1_syscall_addr);
}
void test_4_riscv()
{
	static FunctionAddress fa;
	machine->vmcall(fa.get(machine, "test_print"));
}
void test_4_lua()
{
	luascript->call("test_print");
}
void test_4_lua_syscall()
{
	luascript->call("test_syscall");
}

void test_5_riscv()
{
	static FunctionAddress fa;
	machine->vmcall(fa.get(machine, "test_longcall"));
}
void test_5_lua()
{
	luascript->call("test_longcall");
}

void test_6_riscv()
{
	static FunctionAddress fa;
	machine->vmcall(fa.get(machine, "test_threads"));
}
void test_6_lua()
{
	luascript->call("test_threads");
}

void test_7_riscv_1()
{
	static FunctionAddress fa;
	machine->vmcall(fa.get(machine, "test_threads_args1"));
}
void test_7_riscv_2()
{
	static FunctionAddress fa;
	machine->vmcall(fa.get(machine, "test_threads_args2"));
}
void test_7_lua_1()
{
	luascript->call("test_threads_args1");
}
void test_7_lua_2()
{
	luascript->call("test_threads_args2");
}

void test_8_riscv()
{
	static FunctionAddress fa;
	machine->vmcall<1'000'000>(fa.get(machine, "test_memcpy"));
}
void test_8_native_riscv()
{
	static FunctionAddress fa;
	machine->vmcall<1'000'000>(fa.get(machine, "test_syscall_memcpy"));
}
void test_8_lua()
{
	luascript->call("test_memcpy");
}
