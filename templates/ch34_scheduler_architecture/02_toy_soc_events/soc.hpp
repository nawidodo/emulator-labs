#pragma once
// SoC integration: one master clock (the scheduler's now()), the fx8 CPU,
// a timer and a UART as event-driven clients. Every dispatch lands in an
// event log so golden tests can pin the exact interleaving.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include "devices.hpp"
#include "fx8.hpp"
#include "scheduler_01.hpp"  // ch34/01_event_queue reference, vendored

namespace soc {

enum class Dev : int { kCpu = 0, kTimer = 1, kUart = 2 };

inline const char* dev_name(Dev d) {
    switch (d) {
        case Dev::kCpu: return "cpu";
        case Dev::kTimer: return "timer";
        case Dev::kUart: return "uart";
    }
    return "?";
}

class SoC {
public:
    fx8::Cpu cpu;
    TimerDevice timer;
    UartDevice uart;
    std::vector<std::string> event_log;
    std::vector<std::string> insn_trace;  // one line per executed insn

    explicit SoC(std::span<const uint8_t> program) {
        cpu.load(program);
        cpu.reset();
        // OUT writes go to the UART queue; the SoC starts the wire.
        cpu.on_out = [this](uint8_t b) { on_cpu_out(b); };
    }

    // Schedule the next deadline for one device. `ready_at` overrides the
    // deadline (used for the CPU continuation, ready only AFTER the
    // current instruction's cycles).
    void schedule_device(Dev d, uint64_t ready_at) {
//@LABS-BEGIN 5
//@LABS-SOLUTION
        if (ready_at == kNoDeadline) return;
        uint64_t when = ready_at;
        if (d == Dev::kTimer) when = timer.next_event();
        if (d == Dev::kUart) when = uart.next_event();
        if (when == kNoDeadline) return;  // idle device stays unscheduled
        sched_.schedule(when, [this, d] { dispatch(d); }, dev_name(d));
//@LABS-STUB
        // TODO(5): compute the device's absolute deadline — `ready_at`
        // for the CPU, next_event() for timer/uart — and schedule it on
        // sched_ with dev_name(d) as label. Idle devices (deadline ==
        // kNoDeadline) must NOT be scheduled.
        (void)d;
        (void)ready_at;  // wrong on purpose: devices never enter queue
//@LABS-END
    }

    // Initial schedule at power-on: CPU at 0, timer at its first period,
    // UART idle.
    void boot() {
//@LABS-BEGIN 6
//@LABS-SOLUTION
        timer.last_fire = 0;
        uart.busy = false;
        schedule_device(Dev::kCpu, 0);
        schedule_device(Dev::kTimer, 0);
//@LABS-STUB
        // TODO(6): anchor timer.last_fire at 0, mark the UART idle, then
        // schedule the CPU (ready at cycle 0) and the timer (its first
        // deadline). The idle UART must NOT be scheduled yet.
//@LABS-END
    }

    // Run the master clock up to `limit` cycles or until the CPU halts.
    void run_until(uint64_t limit) {
//@LABS-BEGIN 7
//@LABS-SOLUTION
        sched_.run_until(limit);
//@LABS-STUB
        (void)limit;  // TODO(7): forward to sched_.run_until(limit).
//@LABS-END
    }

    uint64_t now() const { return sched_.now(); }
    size_t pending_events() const { return sched_.pending(); }
    bool halted() const { return halted_; }
    uint64_t events_dispatched() const { return events_; }

    // Dispatch exactly one event (public driver for runners/tests).
    bool step_event() { return sched_.step(); }

private:
    void on_cpu_out(uint8_t b) {
        uart.push(b);
        const uint64_t now = sched_.now();
        if (!uart.busy && uart.pending.size() == 1) {
            // Line was idle: transmission starts now, completes byte_period
            // later. (A busy UART just queues behind pending bytes.)
            uart.busy = true;
            uart.busy_until = now + uart.byte_period;
            schedule_device(Dev::kUart, 0);
        }
    }

    void dispatch(Dev d) {
        ++events_;
        const uint64_t now = sched_.now();
        switch (d) {
            case Dev::kCpu: {
//@LABS-BEGIN 8
//@LABS-SOLUTION
                if (halted_) return;  // stay retired; do not reschedule
                const int cost = cpu.step();
                {
                    char buf[80];
                    std::snprintf(buf, sizeof(buf),
                                  "pc=%02X op=%02X a=%02X cyc=%llu",
                                  cpu.last_pc(), cpu.last_op(), cpu.a,
                                  static_cast<unsigned long long>(now));
                    insn_trace.emplace_back(buf);
                }
                event_log.push_back("cyc=" + std::to_string(now) +
                                    " dev=cpu");
                // Next CPU readiness is AFTER this instruction's cycles;
                // scheduling at `now` would loop forever on one opcode.
                if (!cpu.halted)
                    sched_.schedule(now + uint64_t(cost),
                                    [this] { dispatch(Dev::kCpu); }, "cpu");
                else
                    halted_ = true;
                break;
//@LABS-STUB
                // TODO(8): a dispatched-but-halted CPU returns without
                // logging. Otherwise: step the CPU, append to insn_trace
                // ("pc=%02X op=%02X a=%02X cyc=<start>") and event_log
                // ("cyc=<now> dev=cpu"), then reschedule the continuation
                // at now + cost. On HALT set halted_ and do NOT
                // reschedule.
                (void)now;  // wrong on purpose: CPU never advances
//@LABS-END
            }
            case Dev::kTimer:
                timer.fire(now);
                event_log.push_back("cyc=" + std::to_string(now) +
                                    " dev=timer");
                schedule_device(Dev::kTimer, 0);
                break;
            case Dev::kUart:
                uart.fire(now);
                event_log.push_back("cyc=" + std::to_string(now) +
                                    " dev=uart");
                if (uart.busy || !uart.pending.empty())
                    schedule_device(Dev::kUart, 0);
                break;
        }
    }

    Sched sched_;
    bool halted_ = false;
    uint64_t events_ = 0;
};

}  // namespace soc
