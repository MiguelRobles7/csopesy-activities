# 🖥️ OS Simulator in C++

This project is a **Command-Line Operating System Simulator** built with C++. It simulates basic terminal behavior including session management through "screens", rudimentary command parsing, and screen navigation. It provides a foundation for extending into more complex OS or shell behavior simulations.

Made by: ROBLES, Miguel [S20] | IMPERIAL, Izabella | VICTORIA, Alfred [S20]

---

## 🔧 Features

* 🪟 **Virtual Screens**: Create and resume named screens (sessions)
* 🕒 **Timestamping**: Screens have creation timestamps for tracking
* 🔄 **Command Parsing**: Input is parsed and interpreted like a shell
* 🧼 **Cross-platform Clear**: Clears terminal screen across OSes
* 🧪 **Stub Commands**: Placeholder commands like `initialize`, `report-util`, etc.

---

## 🛠 Requirements

* C++17-compatible compiler (e.g., `g++`, `clang++`, MSVC)
* Terminal access (Linux, macOS, Windows, WSL)

---

## 🚀 Compilation

### On Linux/macOS/WSL:

```bash
g++ -std=c++17 -o os_simulator main.cpp
```

### On Windows (MinGW):

```bash
g++ -std=c++17 -o os_simulator.exe main.cpp
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

---

## 📝 Supported Commands

| Command            | Description                                          |
| ------------------ | ---------------------------------------------------- |
| `screen -s <name>` | Create and switch to a new screen titled `<name>`    |
| `screen -r <name>` | Resume an existing screen session                    |
| `exit`             | Return to main menu, or exit if already in main menu |
| `clear`            | Clear terminal (only works in main menu)             |
| `initialize`       | Placeholder: Simulate initialization task            |
| `scheduler-test`   | Placeholder: Simulate scheduler testing              |
| `scheduler-stop`   | Placeholder: Simulate stopping the scheduler         |
| `report-util`      | Placeholder: Simulate resource utilization reporting |

---

## 🧪 Sample Usage

```bash
Enter a command: screen -s LogScreen
# Creates and enters a screen named LogScreen

Enter a command: exit
# Returns to Main Menu

Enter a command: screen -r LogScreen
# Resumes the screen named LogScreen

Enter a command: initialize
# Simulated response: "initialize command recognized. Doing something."
```

---

## 📌 Notes

* Screen data is held **in memory only during runtime**.
* `currentLine` and `totalLines` are currently placeholders; you may link these to future terminal buffer or content logic.
* Designed as a starting point for simulating an interactive OS interface.

---
