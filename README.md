# 🖥️ OS Simulator in C++

This project is a **Command-Line Operating System Simulator** built with C++. It simulates basic terminal behavior including session management through "screens", rudimentary command parsing, and screen navigation. It provides a foundation for extending into more complex OS or shell behavior simulations.

Made by: `ROBLES, Miguel [S20]` | `IMPERIAL, Izabella [S19]` | `VICTORIA, Alfred [S20]`

---

## 💡 Key Features

- 🪟 **Virtual Screens**  
  Named screen sessions with timestamped creation and memory size allocation.

- 🧮 **Memory Management**  
  Demand paging allocator with backing store access (`csopesy-backing-store.txt`), page faults, and frame-level eviction.

- 📊 **Memory Visualization**  
  Use `vmstat` and `process-smi` to debug system memory and process states.

- ⚙️ **Instruction Simulation**  
  Supports `DECLARE`, `ADD`, `SUBTRACT`, `SLEEP`, `PRINT`, `READ`, `WRITE`, `FOR`, and more.

- 🧵 **Multicore Scheduler**  
  Configurable CPU cores with round-robin or FCFS scheduling via `config.txt`.

- 🛑 **Access Violation Detection**  
  Processes crash gracefully when reading/writing invalid memory locations.

- 🧪 **Custom Process Scripts**  
  Launch full instruction scripts via `screen -c <name> <mem> "<inst>"`.
---

## 🛠 Requirements

* C++17-compatible compiler (e.g., `g++`, `clang++`, MSVC)
* Terminal access (Linux, macOS, Windows, WSL)

---

## 🚀 Compilation

### On Linux/macOS/WSL:

```bash
clang++ -fcolor-diagnostics -fansi-escape-codes -g -std=c++17 -stdlib=libc++ -lncurses main.cpp -o os_simulator
```

### On Windows (MinGW):

```bash
g++ -std=c++17 -o os_simulator.exe main.cpp -lncurses
```

---

## ▶️ Running the Program

### Linux/macOS/WSL:

```bash
./os_simulator
```

### Windows:

```bash
os_simulator.exe
```
