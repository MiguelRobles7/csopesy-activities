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
    FOR_LOOP
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

        // Pop the next screen and cast to ExecutableScreen
        Screen* baseScreen = readyQueue.front();
        readyQueue.pop();
        lock.unlock();
        ExecutableScreen* execScreen = dynamic_cast<ExecutableScreen*>(baseScreen);
        if (!execScreen)
            continue;

        // Initialize log file with header once
        {
            std::ofstream headerOut(output_dir + "/" + execScreen->name + ".txt", std::ios::app);
            if (headerOut.is_open()) {
                headerOut << "Process name: " << execScreen->name << "\nLogs:\n\n";
            }
        }

        // Execute instructions sequentially
        while (execScreen->instructionPointer < execScreen->instructions.size()) {
            Instruction& inst = execScreen->instructions[execScreen->instructionPointer];
            std::string logEntry;

            switch (inst.type) {
                case InstructionType::DECLARE:
                    execScreen->memory.vars[inst.var1] = inst.value;
                    logEntry = "Declared " + inst.var1 + " = " + std::to_string(inst.value);
                    break;

                case InstructionType::PRINT: {
                    logEntry = inst.message;
                    if (execScreen->memory.vars.count(inst.var1)) {
                        logEntry += std::to_string(execScreen->memory.vars[inst.var1]);
                    } else {
                        logEntry += "undefined";
                    }
                    break;
                }

                case InstructionType::ADD: {
                    uint16_t a = execScreen->memory.vars[inst.var2];
                    uint16_t b = execScreen->memory.vars[inst.var3];
                    execScreen->memory.vars[inst.var1] = a + b;
                    logEntry = "Added: " + inst.var1 + " = " + std::to_string(execScreen->memory.vars[inst.var1]);
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

            // Update execution metadata
            execScreen->currentLine++;
            execScreen->cpuId = coreId;
            execScreen->lastLogTime = getCurrentDateTime();

            // Append log entry to file
            std::ofstream outFile(output_dir + "/" + execScreen->name + ".txt", std::ios::app);
            if (outFile.is_open()) {
                outFile << "(" << execScreen->lastLogTime << ") Core:" << coreId << " " << logEntry << "\n";
            }

            // Advance instruction pointer and simulate execution delay
            execScreen->instructionPointer++;
            std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
        }

        // Mark process as finished
        execScreen->finishedTime = getCurrentDateTime();
    }
}

void schedulerThreadFunc(std::vector<ExecutableScreen> &screens)
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

void startPrintJob(std::vector<ExecutableScreen> &screens)
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
    std::vector<std::string> vars = {"x", "y", "z"};

    for (int i = 0; i < count; ++i) {
        int type = getRand(0, 4);
        switch (type) {
        case 0: { // DECLARE
            Instruction inst{InstructionType::DECLARE};
            inst.var1 = vars[getRand(0, 2)];
            inst.value = getRand(1, 100);
            instructions.push_back(inst);
            break;
        }
        case 1: { // PRINT
            Instruction inst{InstructionType::PRINT};
            inst.var1 = vars[getRand(0, 2)];
            inst.message = "Value from: ";
            instructions.push_back(inst);
            break;
        }
        case 2: { // ADD
            Instruction inst{InstructionType::ADD};
            inst.var1 = vars[getRand(0, 2)];
            inst.var2 = vars[getRand(0, 2)];
            inst.var3 = vars[getRand(0, 2)];
            instructions.push_back(inst);
            break;
        }
        case 3: { // SUBTRACT
            Instruction inst{InstructionType::SUBTRACT};
            inst.var1 = vars[getRand(0, 2)];
            inst.var2 = vars[getRand(0, 2)];
            inst.var3 = vars[getRand(0, 2)];
            instructions.push_back(inst);
            break;
        }
        case 4: { // SLEEP
            Instruction inst{InstructionType::SLEEP};
            inst.sleepTicks = getRand(1, 3);
            instructions.push_back(inst);
            break;
        }
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
    std::vector<ExecutableScreen> screens;
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
                        std::string procName = "p" + std::to_string(nextPid++);
                        auto exec = ExecutableScreen{};
                        exec.name = procName;
                        exec.instructions = generateRandomInstructions(getRand(minInstructions, maxInstructions));
                        exec.createdDate = getCurrentDateTime();
                        {
                            std::lock_guard<std::mutex> lg(screensMutex);
                            screens.push_back(std::move(exec));
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(batchFreq * delayPerExec));
                    }
                });

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
                schedulerRunning = false;
                cv.notify_all();  // wake up any waiting CPU threads
                if (schedulerGeneratorThread.joinable())
                    schedulerGeneratorThread.join();
                std::cout << "Scheduler stopped.\n";
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
        else if (command[0] == "clear" && currentScreen.name == "Main Menu")
        {
            clearScreen();
            printHeader();
        }
        else if (command[0] == "screen" && (command.size() == 3 || command.size() == 2) && currentScreen.name == "Main Menu")
        {
            if (command[1] == "-s") // TODO: Screen specific commands: "exit" and "process-smi"
            {
                ExecutableScreen newScreen = createScreen(command[2]);
                screens.push_back(newScreen);
                currentScreen = newScreen;
                clearScreen();
                printScreen(newScreen);
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