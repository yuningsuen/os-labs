# NJU OS 2025 Labs

This repository contains my implementations of 7 labs for the Operating Systems course at Nanjing University (2025).

## Lab Overview

### M1: Labyrinth Game (labyrinth)

A command-line multiplayer maze game backend. Supports map loading, player movement, and connectivity checking. This is a stateless backend service where each execution corresponds to one user operation, with game state persisted in files. Focuses on command-line argument parsing and UNIX tool conventions.

**Key Features:**

- Load maze maps from files
- Handle player movement commands
- Check maze connectivity
- Support various command-line options (`-m/--map`, `-p/--player`, `--move`, `--version`)

### M2: Process Tree Printer (pstree)

Implementation of a `pstree`-like tool that displays process parent-child relationships in a tree structure. Utilizes the procfs filesystem to gather process information, demonstrating the UNIX "everything is a file" design philosophy.

**Key Features:**

- Traverse `/proc` directory to gather all process information
- Parse process parent-child relationships
- Output process structure in tree format
- Support multiple display options (`-p/--show-pids`, `-n/--numeric-sort`, `-V/--version`)

### M3: System Call Profiler (sperf)

A system call performance analysis tool that tracks time spent in various system calls during program execution. Built on top of `strace`, implementing inter-process communication through pipes.

**Key Features:**

- Launch target programs and trace their system calls
- Real-time statistics of system call time distribution
- Periodic output of top 5 most time-consuming system calls
- Support for monitoring long-running programs dynamically

### M4: C Language REPL (crepl)

An interactive C language interpreter supporting dynamic compilation and execution of C functions and expressions. Utilizes dynamic linking technology to achieve "interpreted" C execution.

**Key Features:**

- Interactive input of C function definitions and expressions
- Dynamic compilation of functions into shared libraries
- Real-time loading and execution of compiled code
- Support for inter-function calls

### M5: Parallel Memory Allocator (mymalloc)

A thread-safe memory allocator supporting `mymalloc` and `myfree` functions. Works in a freestanding environment with only `vmalloc`/`vmfree` as underlying memory management.

**Key Features:**

- Multi-processor safe memory allocation and deallocation
- Non-overlapping, 8-byte aligned memory allocation
- Support for memory reuse, preventing memory leaks
- High-performance parallel allocation algorithms

### M6: GPT-2 Parallel Inference (gpt.c)

Parallelization optimization of GPT-2 neural network inference code. Uses multi-threading techniques to accelerate large language model text generation, learning AI system performance optimization.

**Key Features:**

- Parallelize GPT-2 model inference process
- Maintain consistency with serial version output
- Achieve near-linear speedup on multi-core processors
- Use producer-consumer pattern for parallel programming

### M7: HTTP Server (httpd)

A multi-threaded HTTP server capable of handling static file requests and CGI program calls. Learn network programming and basic web server principles.

**Key Features:**

- Listen for HTTP requests and parse protocol
- Support CGI programs for dynamic content generation
- Multi-threaded concurrent request handling (max 4 parallel)
- Complete HTTP response generation and error handling

## Technology Stack

- **Programming Language**: C
- **Concurrency Control**: Spinlocks, semaphores, condition variables
- **System Calls**: fork, exec, pipe, mmap, etc.
- **Network Programming**: Socket programming
- **Build Tools**: Makefile
- **Debugging Tools**: GDB, strace, perf

## Development Environment

- Operating System: Linux (Ubuntu/Debian recommended)
- Compiler: GCC
- Debugging Tools: GDB

## Project Structure

```
├── README.md
├── crepl/          # M4: C Language REPL
├── gpt/            # M6: GPT-2 Parallel Inference
├── httpd/          # M7: HTTP Server
├── labyrinth/      # M1: Labyrinth Game
├── mymalloc/       # M5: Parallel Memory Allocator
├── pstree/         # M2: Process Tree Printer
├── sperf/          # M3: System Call Profiler
└── testkit/        # Testing Framework
```

## Build and Run

Each lab directory contains a Makefile. Use the following commands:

```bash
cd <lab_directory>
make                # Compile
make clean          # Clean up
```

## References

- [Course Website](https://jyywiki.cn/OS/2025/)
- [Lab 1 labyrinth](https://jyywiki.cn/OS/2025/labs/M1.md)
- [Lab 2 pstree](https://jyywiki.cn/OS/2025/labs/M2.md)
- [Lab 3 sperf](https://jyywiki.cn/OS/2025/labs/M3.md)
- [Lab 4 crepl](https://jyywiki.cn/OS/2025/labs/M4.md)
- [Lab 5 mymalloc](https://jyywiki.cn/OS/2025/labs/M5.md)
- [Lab 6 gpt.c](https://jyywiki.cn/OS/2025/labs/M6.md)
- [Lab 7 httpd](https://jyywiki.cn/OS/2025/labs/M7.md)
