
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include "z80.h"

namespace {

using z80::fast_u8;
using z80::fast_u16;
using z80::least_u8;
using z80::unused;

#if defined(__GNUC__) || defined(__clang__)
# define LIKE_PRINTF(format, args) \
      __attribute__((__format__(__printf__, format, args)))
#else
# define LIKE_PRINTF(format, args) /* nothing */
#endif

const char program_name[] = "benchmark";

[[noreturn]] LIKE_PRINTF(1, 0)
void verror(const char *format, va_list args) {
    std::fprintf(stderr, "%s: ", program_name);
    std::vfprintf(stderr, format, args);
    std::fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

[[noreturn]] LIKE_PRINTF(1, 2)
void error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    verror(format, args);
    va_end(args);
}


static constexpr fast_u16 quit_addr = 0x0000;
static constexpr fast_u16 bdos_addr = 0x0005;
static constexpr fast_u16 entry_addr = 0x0100;


// Handles CP/M BDOS calls to write text messages.
// Reports all other unhandled BDOS calls.
// Reports cumulated clock ticks.
template<typename B>
class default_watcher : public B {
public:
    typedef B base;

    void write_char(fast_u8 c) {
        std::putchar(static_cast<char>(c));
    }

    void handle_c_write() {
        write_char(base::get_e());
    }

    void handle_c_writestr() {
        fast_u16 addr = base::get_de();
        for(;;) {
            fast_u8 c = self().on_read(addr);
            if(c == '$')
                break;

            write_char(c);
            addr = (addr + 1) % z80::address_space_size;
        }
    }

    void handle_bdos_call( fast_u8 c ) {
        switch( c ) {
        case c_write:
            handle_c_write();
            break;
        case c_writestr:
            handle_c_writestr();
            break;
        default:
            std::printf( "bdos: %d\n", c );
        }
    }

    void on_step() {
        if(base::get_pc() == bdos_addr) {
            fast_u8 c = base::get_c();
            if(c != 0) {
                handle_bdos_call( c );
            } else {
                base::set_pc( quit_addr );
                return;
            }
        }
        base::on_step();
    }

    void on_tick( unsigned t ) { ticks += t; }

    void on_report() { std::printf("ticks: %lu\n", ticks ); }

protected:
    using base::self;
    uint64_t ticks = 0;

private:
    static constexpr fast_u8 c_write = 0x02;
    static constexpr fast_u8 c_writestr = 0x09;
};


// Lets the emulator to perform at full speed.
template<typename B>
class empty_watcher : public B {
public:
    typedef B base;

    // The benchmark emulator provides no support for interrupts,
    // so no need to track the flags.
    // TODO: Remove that flag from the emulator's state at all.
    void on_set_is_int_disabled(bool f) { unused(f); }
    void on_set_iff(bool f) { unused(f); }

    void on_report() {}

protected:
    using base::self;
};


// Tracks use of CPU state.
template<typename B>
class state_watcher : public B {
public:
    typedef B base;

    fast_u16 on_get_pc() { ++pc_reads; return base::on_get_pc(); }
    void on_set_pc(fast_u16 nn) { ++pc_writes; base::on_set_pc(nn); }

    fast_u16 on_get_sp() { ++sp_reads; return base::on_get_sp(); }
    void on_set_sp(fast_u16 nn) { ++sp_writes; base::on_set_sp(nn); }

    fast_u16 on_get_wz() { ++wz_reads; return base::on_get_wz(); }
    void on_set_wz(fast_u16 nn) { ++wz_writes; base::on_set_wz(nn); }

    fast_u16 on_get_bc() { ++bc_reads; return base::on_get_bc(); }
    void on_set_bc(fast_u16 nn) { ++bc_writes; base::on_set_bc(nn); }
    fast_u8 on_get_b() { ++b_reads; return base::on_get_b(); }
    void on_set_b(fast_u8 n) { ++b_writes; base::on_set_b(n); }
    fast_u8 on_get_c() { ++c_reads; return base::on_get_c(); }
    void on_set_c(fast_u8 n) { ++c_writes; base::on_set_c(n); }

    fast_u16 on_get_de() { ++de_reads; return base::on_get_de(); }
    void on_set_de(fast_u16 nn) { ++de_writes; base::on_set_de(nn); }
    fast_u8 on_get_d() { ++d_reads; return base::on_get_d(); }
    void on_set_d(fast_u8 n) { ++d_writes; base::on_set_d(n); }
    fast_u8 on_get_e() { ++e_reads; return base::on_get_e(); }
    void on_set_e(fast_u8 n) { ++e_writes; base::on_set_e(n); }

    fast_u16 on_get_hl() { ++hl_reads; return base::on_get_hl(); }
    void on_set_hl(fast_u16 nn) { ++hl_writes; base::on_set_hl(nn); }
    fast_u8 on_get_h() { ++h_reads; return base::on_get_h(); }
    void on_set_h(fast_u8 n) { ++h_writes; base::on_set_h(n); }
    fast_u8 on_get_l() { ++l_reads; return base::on_get_l(); }
    void on_set_l(fast_u8 n) { ++l_writes; base::on_set_l(n); }

    fast_u16 on_get_af() { ++af_reads; return base::on_get_af(); }
    void on_set_af(fast_u16 nn) { ++af_writes; base::on_set_af(nn); }
    fast_u8 on_get_a() { ++a_reads; return base::on_get_a(); }
    void on_set_a(fast_u8 n) { ++a_writes; base::on_set_a(n); }
    fast_u8 on_get_f() { ++f_reads; return base::on_get_f(); }
    void on_set_f(fast_u8 n) { ++f_writes; base::on_set_f(n); }

    void on_tick( unsigned t ) { ticks += t; }

    bool on_is_int_disabled() {
        ++is_int_disabled_reads;
        return base::on_is_int_disabled(); }
    void on_set_is_int_disabled(bool f) {
        ++is_int_disabled_writes;
        base::on_set_is_int_disabled(f); }

    bool on_is_halted() {
        ++is_halted_reads;
        return base::on_is_halted(); }
    void on_set_is_halted(bool f) {
        ++is_halted_writes;
        base::on_set_is_halted(f); }

    bool on_get_iff() { ++iff_reads; return base::on_get_iff(); }
    void on_set_iff(bool f) { ++iff_writes; base::on_set_iff(f); }

    void on_report() {
        std::printf("             ticks:     %10lu\n"
                    "             pc reads:  %10lu\n"
                    "             pc writes: %10lu\n"
                    "             sp reads:  %10lu\n"
                    "             sp writes: %10lu\n"
                    "             wz reads:  %10lu\n"
                    "             wz writes: %10lu\n"
                    "             bc reads:  %10lu\n"
                    "             bc writes: %10lu\n"
                    "              b reads:  %10lu\n"
                    "              b writes: %10lu\n"
                    "              c reads:  %10lu\n"
                    "              c writes: %10lu\n"
                    "             de reads:  %10lu\n"
                    "             de writes: %10lu\n"
                    "              d reads:  %10lu\n"
                    "              d writes: %10lu\n"
                    "              e reads:  %10lu\n"
                    "              e writes: %10lu\n"
                    "             hl reads:  %10lu\n"
                    "             hl writes: %10lu\n"
                    "              h reads:  %10lu\n"
                    "              h writes: %10lu\n"
                    "              l reads:  %10lu\n"
                    "              l writes: %10lu\n"
                    "             af reads:  %10lu\n"
                    "             af writes: %10lu\n"
                    "              a reads:  %10lu\n"
                    "              a writes: %10lu\n"
                    "              f reads:  %10lu\n"
                    "              f writes: %10lu\n"
                    "            iff reads:  %10lu\n"
                    "            iff writes: %10lu\n"
                    "is_int_disabled reads:  %10lu\n"
                    "is_int_disabled writes: %10lu\n"
                    "      is_halted reads:  %10lu\n"
                    "      is_halted writes: %10lu\n",
                    ticks,
                    pc_reads,
                    pc_writes,
                    sp_reads,
                    sp_writes,
                    wz_reads,
                    wz_writes,
                    bc_reads,
                    bc_writes,
                    b_reads,
                    b_writes,
                    c_reads,
                    c_writes,
                    de_reads,
                    de_writes,
                    d_reads,
                    d_writes,
                    e_reads,
                    e_writes,
                    hl_reads,
                    hl_writes,
                    h_reads,
                    h_writes,
                    l_reads,
                    l_writes,
                    af_reads,
                    af_writes,
                    a_reads,
                    a_writes,
                    f_reads,
                    f_writes,
                    iff_reads,
                    iff_writes,
                    is_int_disabled_reads,
                    is_int_disabled_writes,
                    is_halted_reads,
                    is_halted_writes);
    }

protected:
    using base::self;

    uint64_t ticks = 0;
    uint64_t pc_reads = 0;
    uint64_t pc_writes = 0;
    uint64_t sp_reads = 0;
    uint64_t sp_writes = 0;
    uint64_t wz_reads = 0;
    uint64_t wz_writes = 0;
    uint64_t bc_reads = 0;
    uint64_t bc_writes = 0;
    uint64_t b_reads = 0;
    uint64_t b_writes = 0;
    uint64_t c_reads = 0;
    uint64_t c_writes = 0;
    uint64_t de_reads = 0;
    uint64_t de_writes = 0;
    uint64_t d_reads = 0;
    uint64_t d_writes = 0;
    uint64_t e_reads = 0;
    uint64_t e_writes = 0;
    uint64_t hl_reads = 0;
    uint64_t hl_writes = 0;
    uint64_t h_reads = 0;
    uint64_t h_writes = 0;
    uint64_t l_reads = 0;
    uint64_t l_writes = 0;
    uint64_t af_reads = 0;
    uint64_t af_writes = 0;
    uint64_t a_reads = 0;
    uint64_t a_writes = 0;
    uint64_t f_reads = 0;
    uint64_t f_writes = 0;
    uint64_t iff_reads = 0;
    uint64_t iff_writes = 0;
    uint64_t is_int_disabled_reads = 0;
    uint64_t is_int_disabled_writes = 0;
    uint64_t is_halted_reads = 0;
    uint64_t is_halted_writes = 0;
};


// Tracks use of memory.
template<typename B>
class memory_watcher : public B {
public:
    typedef B base;

    fast_u8 on_read(fast_u16 addr) {
        ++memory_reads;
        return base::on_read(addr);
    }

    void on_write(fast_u16 addr, fast_u8 n) {
        ++memory_writes;
        base::on_write(addr, n);
    }

    void on_report() {
        std::printf("         memory reads:  %10lu\n"
                    "         memory writes: %10lu\n",
                    memory_reads,
                    memory_writes);
    }

protected:
    using base::self;

    uint64_t memory_reads = 0;
    uint64_t memory_writes = 0;
};


#define WATCHER default_watcher


template<typename B>
class emulator : public WATCHER<B> {
public:
    typedef WATCHER<B> base;

    emulator() {}

    // Allow obtaining and setting register values without
    // calling register-specific handlers.
    bool on_dispatch_register_accesses() {
        return false;
    }

    fast_u8 on_read(fast_u16 addr) {
        assert(addr < z80::address_space_size);
        base::on_read(addr);
        return memory[addr];
    }

    void on_write(fast_u16 addr, fast_u8 n) {
        assert(addr < z80::address_space_size);
        base::on_write(addr, n);
        memory[addr] = static_cast<least_u8>(n);
    }

    void run(const char *program) {
        FILE *f = std::fopen(program, "rb");
        if(!f) {
            error("Cannot open file '%s': %s", program,
                  std::strerror(errno));
        }

        std::size_t count = std::fread(
            memory + entry_addr, /* size= */ 1,
            z80::address_space_size - entry_addr, f);
        if(ferror(f)) {
            error("Cannot read file '%s': %s", program,
                  std::strerror(errno));
        }
        if(count == 0)
            error("Program file '%s' is empty", program);
        if(!feof(f))
            error("Program file '%s' is too large", program);

        if(std::fclose(f) != 0) {
            error("Cannot close file '%s': %s", program,
                  std::strerror(errno));
        }

        base::set_pc(entry_addr);
        memory[bdos_addr] = 0xc9;  // ret

        for(;;) {
            fast_u16 pc = base::get_pc();
            if(pc == quit_addr)
                break;

            self().on_step();
        }

        self().on_report();
    }

protected:
    using base::self;

private:
    least_u8 memory[z80::address_space_size] = {};
};

class i8080_emulator : public emulator<z80::i8080_cpu<i8080_emulator>>
{};

class z80_emulator : public emulator<z80::z80_cpu<z80_emulator>>
{};

[[noreturn]] static void usage() {
    std::fprintf( stderr,
                "usage: benchmark [-i|-z] <program.com>\n"
                "         -i  i8080 emulation\n"
                "         -z  z80 emulation (default)\n"
    );
    exit( -1 );
}

}  // anonymous namespace


int main(int argc, char *argv[]) {
    if ( argc == 2 && *argv[1] != '-' ) {
        const char *program = argv[1];
        z80_emulator e;
        e.run(program);
        return 0;
    } else if ( argc == 3 && *argv[1] == '-') {
        const char *cpu = argv[1];
        const char *program = argv[2];
        if ( std::strcmp(cpu, "-i" ) == 0) {
            i8080_emulator e;
            e.run(program);
            return 0;
        } else if ( std::strcmp(cpu, "-z" ) == 0) {
            z80_emulator e;
            e.run(program);
            return 0;
        }
    }
    usage();
}
