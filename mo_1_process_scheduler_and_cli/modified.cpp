#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <random>
#include <sstream>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>
#include <filesystem>
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <deque>

std::atomic<bool> schedulerRunning(false);
std::thread       schedulerGeneratorThread;
std::mutex        screensMutex;  // guards the `screens` vector during pushes


struct Config {
    int minIns;
    int maxIns;
    int schedulerAlgo;
    int quantum;
    int batchProcessFreq;
    int delayPerExec;
};

std::thread printThread;
bool isPrinting = false;

int CPU_CORES = 4;
int quantum = 5;
int batchFreq = 1;
int minInstructions = 5;
int maxInstructions = 10;
int delayPerExec = 100;
std::string schedulerAlgo = "fcfs";
std::string output_dir = "./";

std::deque<std::thread> cpuThreads;

int getRand(int min, int max)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

std::string getCurrentDateTime()
{
    time_t t = time(nullptr);
    tm *tm_info = localtime(&t);
    int hour = tm_info->tm_hour % 12;
    if (hour == 0)
        hour = 12;
    const char *ampm = (tm_info->tm_hour >= 12) ? "PM" : "AM";
    char buf[30];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d %s",
             tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_year + 1900,
             hour, tm_info->tm_min, tm_info->tm_sec, ampm);
    return std::string(buf);
}

struct Screen
{
    int cpuId;
    int currentLine;
    int totalLines;
    std::string createdDate;
    std::string finishedDate;
    std::string name;
    std::string lastLogTime;
    std::string finishedTime;

    virtual ~Screen() = default;      // <— make it polymorphic

};

enum class InstructionType {
    DECLARE,
    PRINT,
    ADD,
    SUBTRACT,
    SLEEP,
};

struct Instruction {
    InstructionType type;
    std::string var1, var2, var3;
    std::string message;
    uint16_t value = 0;
    uint8_t sleepTicks = 0;
    std::vector<Instruction> subInstructions;
    int repeatCount = 1;
};

struct ProcessMemory {
    std::unordered_map<std::string, uint16_t> vars;
};

struct ExecutableScreen : public Screen {
    std::vector<Instruction> instructions;
    ProcessMemory memory;
    int instructionPointer = 0;
    std::vector<std::pair<int, int>> forStack; // pair<index, remaining count>
};

ExecutableScreen createScreen(std::string name)
{
    ExecutableScreen newScreen;
    newScreen.cpuId = 0;
    newScreen.memory.vars["x"] = 0;
    newScreen.cpuId = 0;
    newScreen.currentLine = 0;
    newScreen.totalLines = 100;
    newScreen.createdDate = getCurrentDateTime();
    newScreen.name = name;
    return newScreen;
}

void printScreen(const ExecutableScreen &screen)
{
    std::cout << "Screen Title: " << screen.name << "\n";
    std::cout << "Current Line: " << screen.currentLine << "/" << screen.totalLines << "\n";
    std::cout << "Created Date: " << screen.createdDate << "\n";
}

void printHeader()
{
    std::cout << "   ___________ ____  ____  _____________  __ \n"
              << "  / ____/ ___// __ \\/ __ \\/ ____/ ___/\\ \\/ / \n"
              << " / /    \\__ \\/ / / / /_/ / __/  \\__ \\  \\  /  \n"
              << "/ /___ ___/ / /_/ / ____/ /___ ___/ /  / /   \n"
              << "\\____//____/\\____/_/   /_____//____/  /_/    \n"
              << "                                             \n"
              << "Type 'exit' to quit, 'clear' to clear the screen\n\n";
}

void clearScreen()
{
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

std::queue<ExecutableScreen *> readyQueue;
std::mutex queueMutex;
std::condition_variable cv;
bool stopScheduler = false;

void cpuWorker(int coreId) {
    while (!stopScheduler) {
        // Wait for a process to schedule
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, []{
            return !readyQueue.empty() || stopScheduler;
        });
        if (stopScheduler && readyQueue.empty())
            break;

        // Pop the next process
        ExecutableScreen* execScreen = readyQueue.front();
        readyQueue.pop();
        lock.unlock();
        if (!execScreen) continue;

        // Initialize log header if first time
        {
            std::ofstream headerOut(output_dir + "/" + execScreen->name + ".txt", std::ios::app);
            if (headerOut.is_open()) {
                headerOut << "Process name: " << execScreen->name << "\nLogs:\n\n";
            }
        }

        // Choose scheduling algorithm
        if (schedulerAlgo == "rr") {
            // Round-Robin: execute up to 'quantum' instructions then requeue
            int slice = quantum;
            while (slice > 0 && execScreen->instructionPointer < (int)execScreen->instructions.size()) {
                Instruction& inst = execScreen->instructions[execScreen->instructionPointer];
                std::string logEntry;
                // --- Instruction handling (same as FCFS) ---
                switch (inst.type) {
                    case InstructionType::DECLARE:
                        execScreen->memory.vars[inst.var1] = inst.value;
                        logEntry = "Declared " + inst.var1 + " = " + std::to_string(inst.value);
                        break;
                    case InstructionType::PRINT:
                        if (inst.message.find("Hello world from") != std::string::npos) {
                            logEntry = inst.message;
                        } else {
                            logEntry = inst.message;
                            logEntry += execScreen->memory.vars.count(inst.var1)
                                ? std::to_string(execScreen->memory.vars[inst.var1])
                                : std::string("undefined");
                        }
                        break;
                    case InstructionType::ADD: {
                        uint16_t a = execScreen->memory.vars[inst.var2];
                        uint16_t b = inst.var3.empty()
                                    ? inst.value
                                    : execScreen->memory.vars[inst.var3];
                        execScreen->memory.vars[inst.var1] = a + b;
                        logEntry = "Added: " + inst.var1
                                + " = " + std::to_string(execScreen->memory.vars[inst.var1]);
                        break;
                    }
                    case InstructionType::SUBTRACT: {
                        uint16_t a = execScreen->memory.vars[inst.var2];
                        uint16_t b = execScreen->memory.vars[inst.var3];
                        execScreen->memory.vars[inst.var1] = a - b;
                        logEntry = "Subtracted: " + inst.var1 + " = " + std::to_string(execScreen->memory.vars[inst.var1]);
                        break;
                    }
                    case InstructionType::SLEEP:
                        std::this_thread::sleep_for(std::chrono::milliseconds(inst.sleepTicks * delayPerExec));
                        logEntry = "Slept for " + std::to_string(inst.sleepTicks) + " ticks.";
                        break;
                    default:
                        break;
                }
                // --- End instruction handling ---

                // Update metadata and log
                execScreen->instructionPointer++;
                execScreen->currentLine++;
                execScreen->cpuId = coreId;
                execScreen->lastLogTime = getCurrentDateTime();
                std::ofstream outFile(output_dir + "/" + execScreen->name + ".txt", std::ios::app);
                if (outFile.is_open()) {
                    outFile << "(" << execScreen->lastLogTime << ") Core:" << coreId << " " << logEntry << "\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
                slice--;
            }
            if (execScreen->instructionPointer < (int)execScreen->instructions.size()) {
                // not finished: re-enqueue for next round
                std::lock_guard<std::mutex> requeueLock(queueMutex);
                readyQueue.push(execScreen);
                cv.notify_one();
            } else {
                execScreen->finishedTime = getCurrentDateTime();
            }
        } else {
            // FCFS: execute until completion
            while (execScreen->instructionPointer < (int)execScreen->instructions.size()) {
                Instruction& inst = execScreen->instructions[execScreen->instructionPointer];
                std::string logEntry;
                // [Same instruction handling switch as above]
                switch (inst.type) {
                    case InstructionType::DECLARE:
                        execScreen->memory.vars[inst.var1] = inst.value;
                        logEntry = "Declared " + inst.var1 + " = " + std::to_string(inst.value);
                        break;
                    case InstructionType::PRINT:
                        if (inst.message.find("Hello world from") != std::string::npos) {
                            logEntry = inst.message;
                        } else {
                            logEntry = inst.message;
                            logEntry += execScreen->memory.vars.count(inst.var1)
                                ? std::to_string(execScreen->memory.vars[inst.var1])
                                : std::string("undefined");
                        }
                        break;

                    case InstructionType::ADD: {
                        uint16_t a = execScreen->memory.vars[inst.var2];
                        uint16_t b = inst.var3.empty()
                                    ? inst.value
                                    : execScreen->memory.vars[inst.var3];
                        execScreen->memory.vars[inst.var1] = a + b;
                        logEntry = "Added: " + inst.var1
                                + " = " + std::to_string(execScreen->memory.vars[inst.var1]);
                        break;
                    }
                    case InstructionType::SUBTRACT: {
                        uint16_t a = execScreen->memory.vars[inst.var2];
                        uint16_t b = execScreen->memory.vars[inst.var3];
                        execScreen->memory.vars[inst.var1] = a - b;
                        logEntry = "Subtracted: " + inst.var1 + " = " + std::to_string(execScreen->memory.vars[inst.var1]);
                        break;
                    }
                    case InstructionType::SLEEP:
                        std::this_thread::sleep_for(std::chrono::milliseconds(inst.sleepTicks * delayPerExec));
                        logEntry = "Slept for " + std::to_string(inst.sleepTicks) + " ticks.";
                        break;
                    default:
                        break;
                }
                execScreen->instructionPointer++;
                execScreen->currentLine++;
                execScreen->cpuId = coreId;
                execScreen->lastLogTime = getCurrentDateTime();
                std::ofstream outFile(output_dir + "/" + execScreen->name + ".txt", std::ios::app);
                if (outFile.is_open()) {
                    outFile << "(" << execScreen->lastLogTime << ") Core:" << coreId << " " << logEntry << "\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
            }
            execScreen->finishedTime = getCurrentDateTime();
        }
    }
}

void schedulerThreadFunc(std::deque<ExecutableScreen> &screens)
{
    for (auto &screen : screens)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        readyQueue.push(&screen);
        lock.unlock();
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void startPrintJob(std::deque<ExecutableScreen> &screens)
{
    isPrinting = true;
    std::thread scheduler(schedulerThreadFunc, std::ref(screens));

    std::vector<std::thread> cpuThreads;
    for (int i = 0; i < CPU_CORES; ++i)
    {
        cpuThreads.emplace_back(cpuWorker, i);
    }

    scheduler.join();

    // Wait for queue to empty instead of fixed sleep
    while (true)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (readyQueue.empty())
            break;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stopScheduler = true;
    }

    cv.notify_all();

    for (auto &t : cpuThreads)
    {
        t.join();
    }

    isPrinting = false;
    // std::cout << "✅ Print job completed. Logs saved in: " << output_dir << "\n";
}

void readConfigFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error opening " << filename << "\n";
        return;
    }

    std::string param;
    std::cout << "Loaded config parameters:\n";

    while (file >> param) {
        if (param == "num-cpu") {
            file >> CPU_CORES;
            std::cout << " - num-cpu: " << CPU_CORES << "\n";
        } else if (param == "scheduler") {
            file >> schedulerAlgo;
            std::cout << " - scheduler: " << schedulerAlgo << "\n";
        } else if (param == "quantum-cycles") {
            file >> quantum;
            std::cout << " - quantum-cycles: " << quantum << "\n";
        } else if (param == "batch-process-freq") {
            file >> batchFreq;
            std::cout << " - batch-process-freq: " << batchFreq << "\n";
        } else if (param == "min-ins") {
            file >> minInstructions;
            std::cout << " - min-ins: " << minInstructions << "\n";
        } else if (param == "max-ins") {
            file >> maxInstructions;
            std::cout << " - max-ins: " << maxInstructions << "\n";
        } else if (param == "delay-per-exec") {
            file >> delayPerExec;
            std::cout << " - delay-per-exec: " << delayPerExec << "\n";
        } else {
            std::string junk;
            file >> junk;
            std::cout << "Unknown config parameter: " << param << " = " << junk << "\n";
        }
    }
}

std::vector<Instruction> generateRandomInstructions(int count) {
    std::vector<Instruction> instructions;
    instructions.reserve(count);

    for (int i = 0; i < count; ++i) {
        if (i % 2 == 0) {
            // even index → PRINT
            Instruction inst{ InstructionType::PRINT };
            inst.var1    = "x";
            inst.message = "Value from: ";
            instructions.push_back(inst);
        } else {
            // odd index → ADD(x, x, [1–10])
            Instruction inst{ InstructionType::ADD };
            inst.var1 = "x";
            inst.var2 = "x";
            inst.value = getRand(1, 10);
            // leave inst.var3 empty so we know to use inst.value
            instructions.push_back(inst);
        }
    }
    return instructions;
}


int main()
{
    bool isInitialized = false; 
    // TODO: Read config file and store said parameters. Replace any that can be used from FCFS implementation.
    // Said parameters are crucial and relevant to scheduling. 
    printHeader();
    std::string cmd;
    std::deque<ExecutableScreen> screens;
    ExecutableScreen currentScreen;
    ExecutableScreen mainMenu;
    mainMenu.name = "Main Menu";
    currentScreen = mainMenu;
    std::string report_file_name = "csopesy-log.txt";
    std::string report_util;
    std::ostringstream report_stream;

    while (true)
    {   
        std::cout << "Enter a command: ";
        if (!std::getline(std::cin, cmd))
            break;

        std::istringstream inputStream(cmd);
        std::vector<std::string> command;
        std::string token;
        while (inputStream >> token)
        {
            command.push_back(token);
        }

        if (!isInitialized && !(command[0] == "initialize")) {
            std::cout << "Please run the 'initialize' command first.\n";
            continue;
        }
        // Command processing
        // TODO: "scheduler-start" and "scheduler-stop"
        if (command.empty())
        {
            continue;
        }
        // TODO: improve ux
        else if (command[0] == "scheduler-start" && currentScreen.name == "Main Menu")
        {
            if (!schedulerRunning)
            {
                schedulerRunning = true;

                schedulerGeneratorThread = std::thread([&screens]() {
                    int nextPid = 1;
                    while (schedulerRunning) {
                        ExecutableScreen exec{};
                        exec.name = "p" + std::to_string(nextPid++);
                        exec.instructions = generateRandomInstructions(
                                            getRand(minInstructions, maxInstructions));
                        exec.totalLines    = exec.instructions.size();
                        exec.createdDate   = getCurrentDateTime();

                        {
                            std::lock_guard<std::mutex> lg(screensMutex);
                            screens.push_back(std::move(exec));
                            // **Immediately** enqueue the new process:
                            ExecutableScreen* p = &screens.back();
                            {
                                std::lock_guard<std::mutex> ql(queueMutex);
                                readyQueue.push(p);
                            }
                            cv.notify_one();
                        }

                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(batchFreq * delayPerExec));
                    }
                });

                if (!isPrinting) {
                    isPrinting = true;
                    stopScheduler = false;
                    // Spawn CPU workers once
                    for (int i = 0; i < CPU_CORES; ++i) {
                        cpuThreads.emplace_back(cpuWorker, i);
                    }
                }

                std::cout << "Scheduler started.\n";
            }
            else
            {
                std::cout << "Scheduler is already running.\n";
            }
        }
        else if (command[0] == "scheduler-stop" && currentScreen.name == "Main Menu")
        {
            if (schedulerRunning)
            {
                // Stop generating
                schedulerRunning = false;
                if (schedulerGeneratorThread.joinable())
                schedulerGeneratorThread.join();

                // Tell CPU workers to quit once the queue is empty
                {
                std::lock_guard<std::mutex> lock(queueMutex);
                stopScheduler = true;
                }
                cv.notify_all();

                // Join all worker threads
                for (auto &t : cpuThreads)
                t.join();
                cpuThreads.clear();
                isPrinting = false;

            }
            else
            {
                std::cout << "Scheduler is not running.\n";
            }
        }
        else if (command[0] == "exit")
        {
            if (currentScreen.name != "Main Menu")
            {
                currentScreen = mainMenu;
                clearScreen();
                printHeader();
                continue;
            }
            else
            {
                break;
            }
        }
        else if (command[0] == "process-smi") {
            if (currentScreen.name == "Main Menu") {
                std::cout << "Cannot do this in the main menu";
            }
            else {
                // Find the live process in your master list
                auto it = std::find_if(
                    screens.begin(), screens.end(),
                    [&](const ExecutableScreen &s) { return s.name == currentScreen.name; }
                );
                if (it != screens.end()) {
                    auto &proc = *it;
                    std::cout << "Process: " << proc.name;
                    if (proc.currentLine < proc.totalLines) {
                        std::cout << " (Running)\n";
                        std::cout << "  Executed " << proc.currentLine << " / " << proc.totalLines << " instructions\n";
                    } else {
                        std::cout << " (Finished)\n";
                    }
                    std::cout << "  Last Log Time: " << proc.lastLogTime << "\n";
                    std::cout << "  CPU Core: " << proc.cpuId << "\n";
                    std::cout << "  Variables:\n";
                    for (auto &kv : proc.memory.vars) {
                        std::cout << "    " << kv.first << " = " << kv.second << "\n";
                    }
                } else {
                    std::cout << "Process " << currentScreen.name << " not found.\n";
                }
            }
        }
        else if (command[0] == "clear" && currentScreen.name == "Main Menu")
        {
            clearScreen();
            printHeader();
        }
        else if (command[0] == "screen" && (command.size() == 3 || command.size() == 2) && currentScreen.name == "Main Menu")
        {
            if (command[1] == "-s") 
            {
                ExecutableScreen proc = createScreen(command[2]);

                proc.instructions = generateRandomInstructions(
                                    getRand(minInstructions, maxInstructions));

                proc.totalLines = static_cast<int>(proc.instructions.size());
                {
                    std::lock_guard<std::mutex> lg(screensMutex);
                    screens.push_back(std::move(proc));
                    // **Immediately** enqueue the new process:
                    ExecutableScreen* p = &screens.back();
                    {
                        std::lock_guard<std::mutex> ql(queueMutex);
                        readyQueue.push(p);
                    }
                    cv.notify_one();
                }

                currentScreen = proc;
                clearScreen();
                printScreen(proc);
            }
            else if (command[1] == "-r")
            {
                bool found = false;
                for (auto &s : screens)
                {
                    if (s.name == command[2])
                    {
                        currentScreen = s;
                        clearScreen();
                        printScreen(s);
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    std::cout << "Screen not found.\n";
                }
            }
            else if (command[1] == "-ls")
            {
                // Count active/running processes
                int activeCores = 0;
                for (const auto &s : screens)
                {
                    if (s.currentLine > 0 && s.currentLine < s.totalLines)
                        activeCores++;
                }

                int totalCores = CPU_CORES;
                int availableCores = totalCores - activeCores;
                float utilization = (static_cast<float>(activeCores) / totalCores) * 100.0f;

                // Write CPU stats
                report_stream << "CPU Utilization: " << std::fixed << std::setprecision(2) << utilization << "%\n";
                report_stream << "Cores Used: " << activeCores << "\n";
                report_stream << "Cores Available: " << availableCores << "\n\n";

                // Generate Report
                report_stream << "------------------------------\nRunning processes:\n";
                for (size_t i = 0; i < screens.size(); ++i)
                {
                    if (screens[i].currentLine > 0 && screens[i].currentLine < screens[i].totalLines)
                    {
                        report_stream << "process" << i << "  "
                                      << screens[i].lastLogTime << "    "
                                      << "Core " << screens[i].cpuId << "    "
                                      << screens[i].currentLine << " / "
                                      << screens[i].totalLines << "\n";
                    }
                }

                report_stream << "\nFinished processes:\n";
                for (size_t i = 0; i < screens.size(); ++i)
                {
                    if (screens[i].currentLine == screens[i].totalLines)
                    {
                        report_stream << "process" << i << "  "
                                      << (screens[i].finishedTime.empty() ? "Getting finishing time..." : screens[i].finishedTime) << "    "
                                      << "Finished    "
                                      << screens[i].currentLine << " / "
                                      << screens[i].totalLines << "\n";
                    }
                }
                report_stream << "------------------------------\n";

                // Save report to local
                report_util = report_stream.str();

                // Print report
                std::cout << report_util;

                report_stream.clear();
            }
        }
        else if (command[0] == "generate" && command.size() == 2) {
            int n = std::stoi(command[1]);
            for (int i = 0; i < n; ++i) {
                std::string pname = "p" + std::to_string(i+1);  
                ExecutableScreen proc = createScreen(pname);
                // ← assign real instructions here:
                proc.instructions = generateRandomInstructions(
                                    getRand(minInstructions, maxInstructions));
                // optional: make totalLines match actual instruction count
                proc.totalLines = static_cast<int>(proc.instructions.size());
                {
                    std::lock_guard<std::mutex> lg(screensMutex);
                    screens.push_back(std::move(proc));
                }
            }
            std::cout << "Generated " << n << " processes!\n";
        }
        else if (command[0] == "report-util") 
        {
            namespace fs = std::filesystem;
            std::ofstream writeReport(report_file_name);
            writeReport << report_util;
            writeReport.close();
            std::cout << "Report generated at " << fs::path(report_file_name) << "!\n";
            report_util.clear(); 
        }
        else if (command[0] == "print")
        {
            if (isPrinting)
            {
                // std::cout << "Print job already running.\n";
            }
            else
            {
                stopScheduler = false; // reset in case of re-run
                printThread = std::thread(startPrintJob, std::ref(screens));
                printThread.detach(); // run in background
                // std::cout << "⏳ Print job started in background.\n";
            }
        }
        else if ((command[0] == "initialize"))
        {
            std::cout << command[0] << " command recognized. Doing something.\n";
            readConfigFile("config.txt");
            isInitialized = true;
        }
        else if ((command[0] == "scheduler-test" || command[0] == "scheduler-stop") && currentScreen.name == "Main Menu")
        {
            std::cout << command[0] << " command recognized. Doing something.\n";
        }
        else
        {
            std::cout << "Unknown command: " << cmd << "\n";
        }
    }
    return 0;
}