#pragma once
#include <libriscv/machine.hpp>
static constexpr bool verbose_syscalls = false;

//#define SYSCALL_VERBOSE 1
#ifdef SYSCALL_VERBOSE
#define SYSPRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define SYSPRINT(fmt, ...) /* fmt */
#endif

template <int W>
struct State
{
	int exit_code = 0;
	std::string output;
};

template <int W>
void setup_minimal_syscalls(State<W>&, riscv::Machine<W>&);
