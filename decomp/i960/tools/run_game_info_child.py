#!/usr/bin/env python3
"""Run recovered fa_game_info child 0x18644 against one snapshot via ptrace.

The harness resolves native symbol offsets with `nm`, so source changes do not
require hard-coded executable offsets. Linux/x86-64 only; intended for local
reverse-engineering and CI analysis artifacts.
"""

from __future__ import annotations

import argparse
import ctypes
import os
import signal
import subprocess
import tempfile
from pathlib import Path

from game_info_18644_common import (
    GAME_INFO_CHILD, GAME_INFO_FIRST_CALL, GAME_INFO_FIRST_RETURN,
    GAME_INFO_SECOND_CALL, GAME_INFO_SECOND_RETURN, IP_OFFSET, SCHEDULER_RETURN,
    u32, write_u32,
)

PTRACE_TRACEME = 0
PTRACE_PEEKDATA = 2
PTRACE_POKEDATA = 5
PTRACE_CONT = 7
PTRACE_GETREGS = 12
PTRACE_SETREGS = 13

libc = ctypes.CDLL(None, use_errno=True)
libc.ptrace.restype = ctypes.c_long


class Regs(ctypes.Structure):
    _fields_ = [
        ("r15", ctypes.c_ulonglong), ("r14", ctypes.c_ulonglong),
        ("r13", ctypes.c_ulonglong), ("r12", ctypes.c_ulonglong),
        ("rbp", ctypes.c_ulonglong), ("rbx", ctypes.c_ulonglong),
        ("r11", ctypes.c_ulonglong), ("r10", ctypes.c_ulonglong),
        ("r9", ctypes.c_ulonglong), ("r8", ctypes.c_ulonglong),
        ("rax", ctypes.c_ulonglong), ("rcx", ctypes.c_ulonglong),
        ("rdx", ctypes.c_ulonglong), ("rsi", ctypes.c_ulonglong),
        ("rdi", ctypes.c_ulonglong), ("orig_rax", ctypes.c_ulonglong),
        ("rip", ctypes.c_ulonglong), ("cs", ctypes.c_ulonglong),
        ("eflags", ctypes.c_ulonglong), ("rsp", ctypes.c_ulonglong),
        ("ss", ctypes.c_ulonglong), ("fs_base", ctypes.c_ulonglong),
        ("gs_base", ctypes.c_ulonglong), ("ds", ctypes.c_ulonglong),
        ("es", ctypes.c_ulonglong), ("fs", ctypes.c_ulonglong),
        ("gs", ctypes.c_ulonglong),
    ]


def ptrace(request: int, pid: int, address: int = 0, data: int = 0) -> int:
    ctypes.set_errno(0)
    result = libc.ptrace(request, pid, ctypes.c_void_p(address), ctypes.c_void_p(data))
    error = ctypes.get_errno()
    if result == -1 and error:
        raise OSError(error, os.strerror(error))
    return result


def getregs(pid: int) -> Regs:
    registers = Regs()
    ptrace(PTRACE_GETREGS, pid, 0, ctypes.addressof(registers))
    return registers


def setregs(pid: int, registers: Regs) -> None:
    ptrace(PTRACE_SETREGS, pid, 0, ctypes.addressof(registers))


def peek(pid: int, address: int) -> int:
    ctypes.set_errno(0)
    result = libc.ptrace(PTRACE_PEEKDATA, pid, ctypes.c_void_p(address), None)
    error = ctypes.get_errno()
    if result == -1 and error:
        raise OSError(error, os.strerror(error))
    return ctypes.c_ulonglong(result).value


def poke(pid: int, address: int, value: int) -> None:
    ptrace(PTRACE_POKEDATA, pid, address, value)


def set_breakpoint(pid: int, address: int) -> int:
    aligned = address & ~0x7
    shift = (address - aligned) * 8
    original = peek(pid, aligned)
    patched = (original & ~(0xFF << shift)) | (0xCC << shift)
    poke(pid, aligned, patched)
    return original


def restore_breakpoint(pid: int, address: int, original: int) -> None:
    poke(pid, address & ~0x7, original)


def wait_stop(pid: int) -> int:
    waited, status = os.waitpid(pid, 0)
    if waited != pid:
        raise RuntimeError("waitpid returned wrong pid")
    if os.WIFEXITED(status):
        raise RuntimeError(f"host exited with {os.WEXITSTATUS(status)}")
    if os.WIFSIGNALED(status):
        raise RuntimeError(f"host terminated by signal {os.WTERMSIG(status)}")
    if not os.WIFSTOPPED(status):
        raise RuntimeError(f"unexpected wait status {status}")
    return os.WSTOPSIG(status)


def symbol_offsets(binary: Path) -> dict[str, int]:
    names = {
        "hybrid_execute_game_info_child",
        "vf2_i960_snapshot_capture",
        "vf2_i960_snapshot_restore",
        "vf2_i960_snapshot_write_file",
    }
    completed = subprocess.run(["nm", "-n", str(binary)], capture_output=True, text=True)
    if completed.returncode != 0:
        raise RuntimeError(f"nm failed: {completed.stderr}")
    result: dict[str, int] = {}
    for line in completed.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-1] in names:
            result[parts[-1]] = int(parts[0], 16)
    missing = names - result.keys()
    if missing:
        raise RuntimeError(f"missing native symbols: {', '.join(sorted(missing))}")
    return result


def load_bias(pid: int, binary: Path) -> int:
    elf_header = subprocess.run(
        ["readelf", "-h", str(binary)], capture_output=True, text=True, check=True
    ).stdout
    if "Type:" in elf_header and "EXEC" in elf_header.split("Type:", 1)[1].splitlines()[0]:
        return 0
    target = os.path.realpath(binary)
    candidates: list[int] = []
    for line in Path(f"/proc/{pid}/maps").read_text().splitlines():
        parts = line.split()
        if len(parts) < 6 or os.path.realpath(parts[-1]) != target:
            continue
        start = int(parts[0].split("-")[0], 16)
        offset = int(parts[2], 16)
        candidates.append(start - offset)
    if not candidates:
        raise RuntimeError("binary mapping not found")
    return min(candidates)


def find_string(pid: int, needle: bytes) -> int:
    with open(f"/proc/{pid}/mem", "rb", buffering=0) as memory:
        for line in Path(f"/proc/{pid}/maps").read_text().splitlines():
            parts = line.split()
            if "r" not in parts[1]:
                continue
            start, end = (int(value, 16) for value in parts[0].split("-"))
            size = end - start
            if size > 64 * 1024 * 1024:
                continue
            try:
                memory.seek(start)
                block = memory.read(size)
            except OSError:
                continue
            index = block.find(needle)
            if index >= 0:
                return start + index
    raise RuntimeError("output snapshot path string not found in host process")


def remote_call(pid: int, function: int, args: list[int], trap: int) -> int:
    saved = getregs(pid)
    original_trap = set_breakpoint(pid, trap)
    new_rsp = saved.rsp - 8
    old_stack = peek(pid, new_rsp)
    poke(pid, new_rsp, trap)
    registers = Regs()
    ctypes.memmove(ctypes.addressof(registers), ctypes.addressof(saved), ctypes.sizeof(registers))
    registers.rsp = new_rsp
    registers.rip = function
    for name, value in zip(("rdi", "rsi", "rdx", "rcx", "r8", "r9"), args):
        setattr(registers, name, value)
    setregs(pid, registers)
    ptrace(PTRACE_CONT, pid, 0, 0)
    stop_signal = wait_stop(pid)
    result = getregs(pid)
    if stop_signal != signal.SIGTRAP or result.rip != trap + 1:
        raise RuntimeError(
            f"remote call stopped unexpectedly: signal={stop_signal} rip=0x{result.rip:x}"
        )
    return_code = ctypes.c_int(result.rax & 0xFFFFFFFF).value
    restore_breakpoint(pid, trap, original_trap)
    result.rip = trap
    setregs(pid, result)
    poke(pid, new_rsp, old_stack)
    return return_code


def preexec() -> None:
    if libc.ptrace(PTRACE_TRACEME, 0, None, None) != 0:
        os._exit(127)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("input_snapshot", type=Path)
    parser.add_argument("output_snapshot", type=Path)
    parser.add_argument("--return-address", type=lambda value: int(value, 0), required=True)
    args = parser.parse_args()

    offsets = symbol_offsets(args.binary)
    source_data = bytearray(args.input_snapshot.read_bytes())
    source_ip = u32(source_data, IP_OFFSET)
    allowed_call = {
        GAME_INFO_FIRST_RETURN: GAME_INFO_FIRST_CALL,
        GAME_INFO_SECOND_RETURN: GAME_INFO_SECOND_CALL,
    }.get(args.return_address)
    if source_ip == allowed_call:
        write_u32(source_data, IP_OFFSET, args.return_address)
    elif source_ip != args.return_address:
        raise RuntimeError(
            f"input IP 0x{source_ip:08x} is neither the matching call site "
            f"nor return 0x{args.return_address:08x}"
        )

    with tempfile.TemporaryDirectory() as temporary:
        native_input = Path(temporary) / "native-input.vf2snap"
        native_input.write_bytes(source_data)
        command = [
            str(args.binary), "native-resume", str(args.rom_directory),
            str(native_input), "1000", "0", hex(SCHEDULER_RETURN),
            str(args.output_snapshot),
        ]
        process = subprocess.Popen(
            command,
            preexec_fn=preexec,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        pid = process.pid
        try:
            wait_stop(pid)
            bias = load_bias(pid, args.binary)
            child = bias + offsets["hybrid_execute_game_info_child"]
            capture = bias + offsets["vf2_i960_snapshot_capture"]
            restore = bias + offsets["vf2_i960_snapshot_restore"]
            write_file = bias + offsets["vf2_i960_snapshot_write_file"]

            original_restore = set_breakpoint(pid, restore)
            ptrace(PTRACE_CONT, pid, 0, 0)
            stop_signal = wait_stop(pid)
            registers = getregs(pid)
            if stop_signal != signal.SIGTRAP or registers.rip != restore + 1:
                raise RuntimeError("did not intercept vf2_i960_snapshot_restore")
            snapshot, cpu, machine = registers.rdi, registers.rsi, registers.rdx
            restore_breakpoint(pid, restore, original_restore)
            registers.rip = restore
            setregs(pid, registers)

            return_to_host = peek(pid, registers.rsp)
            original_return = set_breakpoint(pid, return_to_host)
            ptrace(PTRACE_CONT, pid, 0, 0)
            stop_signal = wait_stop(pid)
            registers = getregs(pid)
            if stop_signal != signal.SIGTRAP or registers.rip != return_to_host + 1:
                raise RuntimeError("snapshot restore did not return to expected host address")
            restore_breakpoint(pid, return_to_host, original_return)
            registers.rip = return_to_host
            setregs(pid, registers)

            output_pointer = find_string(pid, str(args.output_snapshot).encode() + b"\0")
            child_status = remote_call(
                pid, child, [machine, cpu, GAME_INFO_CHILD, args.return_address], return_to_host
            )
            if child_status != 0:
                raise RuntimeError(f"hybrid_execute_game_info_child returned {child_status}")
            capture_status = remote_call(pid, capture, [snapshot, cpu, machine], return_to_host)
            if capture_status != 0:
                raise RuntimeError(f"vf2_i960_snapshot_capture returned {capture_status}")
            write_status = remote_call(pid, write_file, [snapshot, output_pointer], return_to_host)
            if write_status != 0:
                raise RuntimeError(f"vf2_i960_snapshot_write_file returned {write_status}")
            print(
                f"MATCHABLE child=0x{GAME_INFO_CHILD:08x} return=0x{args.return_address:08x} "
                f"output={args.output_snapshot}"
            )
        finally:
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            try:
                os.waitpid(pid, 0)
            except ChildProcessError:
                pass


if __name__ == "__main__":
    main()
