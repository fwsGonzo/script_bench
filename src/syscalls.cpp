#include "syscalls.hpp"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
using namespace riscv;
static constexpr uint32_t G_SHMEM_BASE = 0x70000000;
static const uint32_t sbrk_start = 0x40000000;
static const uint32_t sbrk_max   = sbrk_start + 0x1000000;
static const uint32_t heap_start = sbrk_max;
static const uint32_t heap_max   = 0xF0000000;

struct iovec32 {
    uint32_t iov_base;
    int32_t  iov_len;
};

template <int W>
long State<W>::syscall_exit(Machine<W>& machine)
{
	this->exit_code = machine.template sysarg<int> (0);
	machine.stop();
	return this->exit_code;
}

template <int W>
long State<W>::syscall_write(Machine<W>& machine)
{
	const int  fd      = machine.template sysarg<int>(0);
	const auto address = machine.template sysarg<address_type<W>>(1);
	const size_t len   = machine.template sysarg<address_type<W>>(2);
	SYSPRINT("SYSCALL write: addr = 0x%X, len = %zu\n", address, len);
	// we only accept standard pipes, for now :)
	if (fd >= 0 && fd < 3) {
		char buffer[1024];
		const size_t len_g = std::min(sizeof(buffer), len);
		machine.memory.memcpy_out(buffer, address, len_g);
		output += std::string(buffer, len_g);
#ifdef RISCV_DEBUG
		return write(fd, buffer, len);
#else
		return len_g;
#endif
	}
	return -EBADF;
}

template <int W>
long State<W>::syscall_writev(Machine<W>& machine)
{
	const int  fd     = machine.template sysarg<int>(0);
	const auto iov_g  = machine.template sysarg<uint32_t>(1);
	const auto count  = machine.template sysarg<int>(2);
	if constexpr (false) {
		printf("SYSCALL writev called, iov = %#X  cnt = %d\n", iov_g, count);
	}
	if (count < 0 || count > 256) return -EINVAL;
	// we only accept standard pipes, for now :)
	if (fd >= 0 && fd < 3) {
        const size_t size = sizeof(iovec32) * count;

        std::vector<iovec32> vec(count);
        machine.memory.memcpy_out(vec.data(), iov_g, size);

        int res = 0;
        for (const auto& iov : vec)
        {
			char buffer[1024];
            auto src_g = (uint32_t) iov.iov_base;
            auto len_g = std::min(sizeof(buffer), (size_t) iov.iov_len);
            machine.memory.memcpy_out(buffer, src_g, len_g);
			output += std::string(buffer, len_g);
#ifdef RISCV_DEBUG
			res += write(fd, buffer, len_g);
#else
			res += len_g;
#endif
        }
        return res;
	}
	return -EBADF;
}

template <int W>
long syscall_close(riscv::Machine<W>& machine)
{
	const int fd = machine.template sysarg<int>(0);
	if constexpr (verbose_syscalls) {
		printf("SYSCALL close called, fd = %d\n", fd);
	}
	if (fd <= 2) {
		return 0;
	}
	printf(">>> Close %d\n", fd);
	return -1;
}

template <int W>
long syscall_ebreak(riscv::Machine<W>& machine)
{
	printf("\n>>> EBREAK at %#X\n", machine.cpu.pc());
#ifdef RISCV_DEBUG
	machine.print_and_pause();
#else
	throw std::runtime_error("Unhandled EBREAK instruction");
#endif
	return 0;
}

template <int W>
long syscall_gettimeofday(Machine<W>& machine)
{
	const auto buffer = machine.template sysarg<address_type<W>>(0);
	SYSPRINT("SYSCALL gettimeofday called, buffer = 0x%X\n", buffer);
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	int32_t timeval32[2] = { (int) tv.tv_sec, (int) tv.tv_usec };
	machine.copy_to_guest(buffer, timeval32, sizeof(timeval32));
    return 0;
}

template <int W>
long syscall_brk(Machine<W>& machine)
{
	static uint32_t sbrk_end = sbrk_start;
	const uint32_t new_end = machine.template sysarg<uint32_t>(0);
	if constexpr (verbose_syscalls) {
		printf("SYSCALL brk called, current = 0x%X new = 0x%X\n", sbrk_end, new_end);
	}
    if (new_end == 0) return sbrk_end;
    sbrk_end = new_end;
    sbrk_end = std::max(sbrk_end, sbrk_start);
    sbrk_end = std::min(sbrk_end, sbrk_max);

	if constexpr (verbose_syscalls) {
		printf("* New sbrk() end: 0x%X\n", sbrk_end);
	}
	return sbrk_end;
}

template <int W>
long syscall_stat(Machine<W>& machine)
{
	const auto  fd      = machine.template sysarg<int>(0);
	const auto  buffer  = machine.template sysarg<uint32_t>(1);
	if constexpr (verbose_syscalls) {
		printf("SYSCALL stat called, fd = %d  buffer = 0x%X\n",
				fd, buffer);
	}
	if (false) {
		struct stat result;
		std::memset(&result, 0, sizeof(result));
		result.st_dev     = 6;
		result.st_ino     = fd;
		result.st_mode    = 0x21b6;
		result.st_nlink   = 1;
		result.st_rdev    = 265;
		result.st_blksize = 512;
		result.st_blocks  = 0;
		machine.copy_to_guest(buffer, &result, sizeof(result));
		return 0;
	}
	return -EBADF;
}

template <int W>
long syscall_uname(Machine<W>& machine)
{
	const auto buffer = machine.template sysarg<uint32_t>(0);
	if constexpr (verbose_syscalls) {
		printf("SYSCALL uname called, buffer = 0x%X\n", buffer);
	}
    static constexpr int UTSLEN = 65;
    struct uts32 {
        char sysname [UTSLEN];
        char nodename[UTSLEN];
        char release [UTSLEN];
        char version [UTSLEN];
        char machine [UTSLEN];
        char domain  [UTSLEN];
    } uts;
    strcpy(uts.sysname, "RISC-V C++ Emulator");
    strcpy(uts.nodename,"libriscv");
    strcpy(uts.release, "5.0.0");
    strcpy(uts.version, "");
    strcpy(uts.machine, "rv32imac");
    strcpy(uts.domain,  "(none)");

    machine.copy_to_guest(buffer, &uts, sizeof(uts32));
	return 0;
}

template <int W>
inline void add_mman_syscalls(Machine<W>& machine)
{
	// munmap
	machine.install_syscall_handler(215,
	[] (Machine<W>& machine) {
		const uint32_t addr = machine.template sysarg<uint32_t> (0);
		const uint32_t len  = machine.template sysarg<uint32_t> (1);
		SYSPRINT(">>> munmap(0x%X, len=%u)\n", addr, len);
		// TODO: deallocate pages completely
		machine.memory.set_page_attr(addr, len, {
			.read  = false,
			.write = false,
			.exec  = false
		});
		return 0;
	});
	// mmap
	machine.install_syscall_handler(222,
	[] (Machine<W>& machine) {
		const int  addr_g = machine.template sysarg<uint32_t>(0);
		const auto length = machine.template sysarg<uint32_t>(1);
		const auto prot   = machine.template sysarg<int>(2);
	    const auto flags  = machine.template sysarg<int>(3);
		SYSPRINT("SYSCALL mmap called, addr %#X  len %u prot %#x flags %#X\n",
	            addr_g, length, prot, flags);
	    if (addr_g == 0 && (length % Page::size()) == 0)
	    {
	        static uint32_t nextfree = heap_start;
	        const uint32_t addr = nextfree;
			// anon pages need to be zeroed
			if (flags & MAP_ANONYMOUS) {
				// ... but they are already CoW
				//machine.memory.memset(addr, 0, length);
			}
	        nextfree += length;
	        return addr;
	    }
		return UINT32_MAX; // = MAP_FAILED;
	});
	// mprotect
	machine.install_syscall_handler(226,
	[] (Machine<W>& machine) {
		const uint32_t addr = machine.template sysarg<uint32_t> (0);
		const uint32_t len  = machine.template sysarg<uint32_t> (1);
		const int      prot = machine.template sysarg<int> (2);
		SYSPRINT(">>> mprotect(0x%X, len=%u, prot=%x)\n", addr, len, prot);
		machine.memory.set_page_attr(addr, len, {
			.read  = bool(prot & 1),
			.write = bool(prot & 2),
			.exec  = bool(prot & 4)
		});
		return 0;
	});
	// madvise
	machine.install_syscall_handler(233,
	[] (Machine<W>& machine) {
		const uint32_t addr = machine.template sysarg<uint32_t> (0);
		const uint32_t len  = machine.template sysarg<uint32_t> (1);
		const int      advice = machine.template sysarg<int> (2);
		SYSPRINT(">>> madvise(0x%X, len=%u, prot=%x)\n", addr, len, advice);
		switch (advice) {
			case MADV_NORMAL:
			case MADV_RANDOM:
			case MADV_SEQUENTIAL:
			case MADV_WILLNEED:
				return 0;
			case MADV_DONTNEED:
				machine.memory.free_pages(addr, len);
				return 0;
			case MADV_REMOVE:
			case MADV_FREE:
				machine.memory.free_pages(addr, len);
				return 0;
			default:
				return -EINVAL;
		}
	});
}

template <int W>
inline void setup_newlib_syscalls(State<W>& state, Machine<W>& machine)
{
	machine.install_syscall_handler(SYSCALL_EBREAK, syscall_ebreak<W>);
	machine.install_syscall_handler(64, {&state, &State<W>::syscall_write});
	machine.install_syscall_handler(93, {&state, &State<W>::syscall_exit});
	machine.install_syscall_handler(214, syscall_brk<W>);
	add_mman_syscalls(machine);
}

/* le sigh */
template void setup_newlib_syscalls<4>(State<4>&, Machine<4>&);
