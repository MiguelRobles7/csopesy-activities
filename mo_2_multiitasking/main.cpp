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
#include <unordered_map>

std::atomic<bool> schedulerRunning(false);
std::thread schedulerGeneratorThread;
std::mutex screensMutex;                                  // guards the `screens` vector during pushes
std::unordered_map<std::string, uint16_t> physicalMemory; // key = "0x500"
std::mutex physicalMemoryMutex;

struct Config
{
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
std::string schedulerAlgo = "rr";
std::string output_dir = "./";
std::atomic<int> totalTicks{0};
std::atomic<int> activeTicks{0};
std::atomic<int> idleTicks{0};
std::atomic<int> pagesPagedIn{0};
std::atomic<int> pagesPagedOut{0};

std::deque<std::thread> cpuThreads;
int MEM_TOTAL = 16384;
int MEM_FRAME_SIZE = 16;
int MIN_MEM_PER_PROC = 64;
int MAX_MEM_PER_PROC = 4096;

struct MemoryBlock
{
    int start;
    int size;
    std::string owner;
};

std::vector<MemoryBlock> memoryBlocks = {{0, MEM_TOTAL, ""}}; // initially all free
std::mutex memMutex;

struct FrameTableEntry
{
    bool occupied = false;
    std::string ownerProcess;
    int virtualPageNumber = -1; // which page of the process is stored here
};

std::vector<FrameTableEntry> frameTable;
std::queue<int> fifoFrameQueue; // tracks frame usage order for FIFO replacement

bool isValidMemoryAccess(const std::string &procName, const std::string &hexAddress)
{
    // Convert hex string to int
    int addr = 0;
    try
    {
        addr = std::stoi(hexAddress, nullptr, 16);
    }
    catch (...)
    {
        return false; // Invalid hex format
    }

    std::lock_guard<std::mutex> lock(memMutex);
    for (const auto &block : memoryBlocks)
    {
        if (block.owner == procName)
        {
            if (addr >= block.start && addr < block.start + block.size)
            {
                return true;
            }
        }
    }
    return false;
}

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

int allocateMemory(const std::string &procName, int memSize)
{
    std::lock_guard<std::mutex> lock(memMutex);
    for (size_t i = 0; i < memoryBlocks.size(); ++i)
    {
        auto &block = memoryBlocks[i];
        if (block.owner.empty() && block.size >= memSize)
        {
            int allocStart = block.start;

            // Case 1: Exact fit
            if (block.size == memSize)
            {
                block.owner = procName;
                return allocStart;
            }

            // Case 2: Need to split
            MemoryBlock allocated{block.start, memSize, procName};
            MemoryBlock leftover{block.start + memSize, block.size - memSize, ""};

            // Replace the original free block with two new blocks
            memoryBlocks[i] = allocated;
            memoryBlocks.insert(memoryBlocks.begin() + i + 1, leftover);

            return allocStart;
        }
    }
    return -1; // no fit found
}

void freeMemory(const std::string &procName)
{
    std::lock_guard<std::mutex> lock(memMutex);
    for (auto &block : memoryBlocks)
    {
        if (block.owner == procName)
        {
            block.owner = "";
        }
    }

    for (size_t i = 0; i + 1 < memoryBlocks.size();)
    {
        if (memoryBlocks[i].owner.empty() && memoryBlocks[i + 1].owner.empty())
        {
            memoryBlocks[i].size += memoryBlocks[i + 1].size;
            memoryBlocks.erase(memoryBlocks.begin() + i + 1);
        }
        else
        {
            ++i;
        }
    }

    int totalMem = 0;
    for (const auto &b : memoryBlocks)
        totalMem += b.size;
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

    virtual ~Screen() = default; // <— make it polymorphic
};

enum class InstructionType
{
    DECLARE,
    PRINT,
    ADD,
    SUBTRACT,
    SLEEP,
    READ,
    WRITE
};

struct Instruction
{
    InstructionType type;
    std::string var1, var2, var3;
    std::string message;
    uint16_t value = 0;
    uint8_t sleepTicks = 0;
    std::vector<Instruction> subInstructions;
    int repeatCount = 1;
};

struct ProcessMemory
{
    std::unordered_map<std::string, uint16_t> vars;
};

struct ExecutableScreen : public Screen
{
    std::vector<Instruction> instructions;
    ProcessMemory memory;
    int instructionPointer = 0;
    std::vector<std::pair<int, int>> forStack; // pair<index, remaining count>
    bool isShutdown = false;
    std::string shutdownMessage;
    int memorySize = 0;
    std::vector<std::string> consoleOutput;

    struct PageTableEntry
    {
        bool present = false;
        int frameNumber = -1;
        bool dirty = false;
    };

    std::unordered_map<int, PageTableEntry> pageTable; // key = virtual page number
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

void shutdownProcess(ExecutableScreen &proc, const std::string &offendingAddr)
{
    proc.isShutdown = true;
    proc.finishedTime = getCurrentDateTime();

    std::string formattedAddr = offendingAddr;
    if (formattedAddr.find("0x") != 0)
    {
        try
        {
            int addrVal = std::stoi(offendingAddr);
            std::stringstream ss;
            ss << "0x" << std::uppercase << std::hex << addrVal;
            formattedAddr = ss.str();
        }
        catch (...)
        {
        }
    }

    proc.shutdownMessage = "Process " + proc.name +
                           " shut down due to memory access violation error that occurred at " +
                           proc.finishedTime + ". " + formattedAddr + " invalid.";
    freeMemory(proc.name);
}

void printScreen(const ExecutableScreen &screen)
{
    std::cout << "Screen Title: " << screen.name << "\n";
    std::cout << "Current Line: " << screen.currentLine << "/" << screen.totalLines << "\n";
    std::cout << "Created Date: " << screen.createdDate << "\n";
    std::cout << "Variables:\n";
    for (const auto &kv : screen.memory.vars)
    {
        std::cout << "  " << kv.first << " = " << kv.second << "\n";
    }
    if (!screen.consoleOutput.empty())
    {
        std::cout << "Console Output:\n";
        for (const auto &line : screen.consoleOutput)
        {
            std::cout << "  " << line << "\n";
        }
    }
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

int findFreeFrame()
{
    for (int i = 0; i < (int)frameTable.size(); ++i)
    {
        if (!frameTable[i].occupied)
            return i;
    }
    return -1;
}

bool restorePageFromBackingStore(const std::string &procName, int virtualPage, int frameNum)
{
    std::ifstream in("csopesy-backing-store.txt");
    if (!in.is_open())
        return false;

    std::string line;
    while (std::getline(in, line))
    {
        std::istringstream iss(line);
        std::string name;
        int page;
        iss >> name >> page;

        if (name == procName && page == virtualPage)
        {
            int baseAddr = frameNum * MEM_FRAME_SIZE;
            for (int i = 0; i < MEM_FRAME_SIZE; ++i)
            {
                uint16_t val;
                if (!(iss >> val))
                    break;
                std::string addr = "0x" + std::to_string(baseAddr + i);
                std::lock_guard<std::mutex> lock(physicalMemoryMutex);
                physicalMemory[addr] = val;
            }
            return true;
        }
    }
    return false;
}

int evictPageAndReturnFrame()
{
    if (fifoFrameQueue.empty())
        return -1; // no pages to evict

    int victimFrame = fifoFrameQueue.front();
    fifoFrameQueue.pop();

    FrameTableEntry &victim = frameTable[victimFrame];
    std::string procName = victim.ownerProcess;
    int virtualPage = victim.virtualPageNumber;

    // Write to backing store (simulate swap out)
    std::ofstream backingFile("csopesy-backing-store.txt", std::ios::app);
    backingFile << procName << " " << virtualPage << " ";
    int baseAddr = victimFrame * MEM_FRAME_SIZE;
    for (int i = 0; i < MEM_FRAME_SIZE; ++i)
    {
        std::string addr = "0x" + std::to_string(baseAddr + i);
        uint16_t val = 0;
        {
            std::lock_guard<std::mutex> lock(physicalMemoryMutex);
            if (physicalMemory.count(addr))
            {
                val = physicalMemory[addr];
            }
        }
        backingFile << val << " ";
    }
    backingFile << "\n";

    pagesPagedOut++;

    // Update victim process page table
    std::queue<ExecutableScreen *> tempQueue;

    while (!readyQueue.empty())
    {
        ExecutableScreen *s = readyQueue.front();
        readyQueue.pop();

        if (s->name == procName)
        {
            s->pageTable[virtualPage].present = false;
            s->pageTable[virtualPage].frameNumber = -1;
        }

        tempQueue.push(s); // requeue
    }

    // Restore original queue
    readyQueue = tempQueue;

    // Mark frame as free
    victim.occupied = false;
    victim.ownerProcess = "";
    victim.virtualPageNumber = -1;

    return victimFrame;
}

void loadPageIntoFrame(ExecutableScreen &proc, int virtualPage)
{
    int frame = findFreeFrame();
    if (frame == -1)
    {
        frame = evictPageAndReturnFrame();
        if (frame == -1)
        {
            std::cout << "ERROR: No frame available for loading page.\n";
            return;
        }
    }

    // Load the page into frame
    frameTable[frame].occupied = true;
    frameTable[frame].ownerProcess = proc.name;
    frameTable[frame].virtualPageNumber = virtualPage;
    restorePageFromBackingStore(proc.name, virtualPage, frame);

    fifoFrameQueue.push(frame);

    // Update page table
    proc.pageTable[virtualPage].present = true;
    proc.pageTable[virtualPage].frameNumber = frame;

    pagesPagedIn++;
}

bool ensurePageLoaded(ExecutableScreen &proc, int memoryAddress)
{
    int virtualPage = memoryAddress / MEM_FRAME_SIZE;

    if (!proc.pageTable[virtualPage].present)
    {
        // Page fault: load it in
        loadPageIntoFrame(proc, virtualPage);
        return true; // page was loaded
    }
    return false; // already present
}

void cpuWorker(int coreId)
{
    while (!stopScheduler)
    {
        // Wait for a process to schedule
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, []
                { return !readyQueue.empty() || stopScheduler; });
        if (readyQueue.empty())
        {
            idleTicks++;
            totalTicks++;
            continue;
        }

        // Pop the next process
        ExecutableScreen *execScreen = readyQueue.front();
        readyQueue.pop();
        lock.unlock();
        if (!execScreen)
            continue;

        // Initialize log header if first time
        {
            std::ofstream headerOut(output_dir + "/" + execScreen->name + ".txt", std::ios::app);
            if (headerOut.is_open())
            {
                headerOut << "Process name: " << execScreen->name << "\nLogs:\n\n";
            }
        }

        // Choose scheduling algorithm
        if (schedulerAlgo == "rr")
        {
            // Round-Robin: execute up to 'quantum' instructions then requeue
            int slice = quantum;
            while (slice > 0 && execScreen->instructionPointer < (int)execScreen->instructions.size())
            {
                Instruction &inst = execScreen->instructions[execScreen->instructionPointer];
                std::string logEntry;
                // --- Instruction handling (same as FCFS) ---
                switch (inst.type)
                {
                case InstructionType::DECLARE:
                {
                    if (execScreen->memory.vars.size() >= 32)
                    {
                        logEntry = "DECLARE skipped: symbol table full.";
                        break;
                    }

                    // Simulate address inside symbol table
                    int varOffset = execScreen->memory.vars.size() * 2;
                    int symbolTableAddress = varOffset;

                    if (symbolTableAddress >= 64)
                    {
                        logEntry = "DECLARE failed: out of symbol table bounds.";
                        break;
                    }

                    ensurePageLoaded(*execScreen, symbolTableAddress); // simulate demand loading

                    execScreen->memory.vars[inst.var1] = inst.value;
                    logEntry = "Declared " + inst.var1 + " = " + std::to_string(inst.value);
                    break;
                }
                case InstructionType::PRINT:
                {
                    std::ostringstream log;
                    log << inst.message;

                    if (!inst.var1.empty())
                    {
                        auto it = execScreen->memory.vars.find(inst.var1);
                        if (it != execScreen->memory.vars.end())
                        {
                            log << it->second;
                        }
                        else
                        {
                            log << "[undefined var: " << inst.var1 << "]";
                        }
                    }

                    std::string output = log.str();
                    execScreen->consoleOutput.push_back(output);

                    logEntry = output;
                    break;
                }

                case InstructionType::ADD:
                {
                    uint16_t a = execScreen->memory.vars[inst.var2];
                    uint16_t b = execScreen->memory.vars[inst.var3];
                    execScreen->memory.vars[inst.var1] = a + b;
                    logEntry = "Added: " + inst.var1 + " = " + std::to_string(execScreen->memory.vars[inst.var1]);
                    break;
                }
                case InstructionType::SUBTRACT:
                {
                    uint16_t a = execScreen->memory.vars[inst.var2];
                    uint16_t b = execScreen->memory.vars[inst.var3];
                    execScreen->memory.vars[inst.var1] = a - b;
                    logEntry = "Subtracted: " + inst.var1 + " = " + std::to_string(execScreen->memory.vars[inst.var1]);
                    break;
                }
                case InstructionType::SLEEP:
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(inst.sleepTicks * delayPerExec));
                    logEntry = "Slept for " + std::to_string(inst.sleepTicks) + " ticks.";
                    break;
                }
                case InstructionType::WRITE:
                {
                    std::string address = inst.var1;
                    std::string valRef = inst.var2;

                    int addr = 0;
                    try
                    {
                        addr = std::stoi(address, nullptr, 16);
                    }
                    catch (...)
                    {
                        shutdownProcess(*execScreen, address);
                        return;
                    }

                    if (addr < 0 || addr >= MEM_TOTAL)
                    {
                        shutdownProcess(*execScreen, address);
                        return;
                    }

                    ensurePageLoaded(*execScreen, addr); // simulate demand paging

                    uint16_t val = 0;
                    if (execScreen->memory.vars.count(valRef))
                    {
                        val = execScreen->memory.vars[valRef];
                    }
                    else
                    {
                        try
                        {
                            val = static_cast<uint16_t>(std::stoi(valRef));
                        }
                        catch (...)
                        {
                            logEntry = "WRITE failed: value not found.";
                            break;
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(physicalMemoryMutex);
                        physicalMemory[address] = val;
                    }

                    logEntry = "Wrote value " + std::to_string(val) + " to " + address;
                    break;
                }
                case InstructionType::READ:
                {
                    std::string varName = inst.var1;
                    std::string address = inst.var2;

                    int addr = 0;
                    try
                    {
                        addr = std::stoi(address, nullptr, 16);
                    }
                    catch (...)
                    {
                        shutdownProcess(*execScreen, address);
                        return;
                    }

                    if (addr < 0 || addr >= MEM_TOTAL)
                    {
                        shutdownProcess(*execScreen, address);
                        return;
                    }

                    ensurePageLoaded(*execScreen, addr);

                    uint16_t val = 0;
                    {
                        std::lock_guard<std::mutex> lock(physicalMemoryMutex);
                        if (physicalMemory.count(address))
                        {
                            val = physicalMemory[address];
                        }
                        else
                        {
                            val = 0; // uninitialized
                        }
                    }

                    execScreen->memory.vars[varName] = val;
                    logEntry = "Read value " + std::to_string(val) + " from " + address + " into " + varName;
                    break;
                }
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
                if (outFile.is_open())
                {
                    outFile << "(" << execScreen->lastLogTime << ") Core:" << coreId << " " << logEntry << "\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
                totalTicks++;
                activeTicks++;
                slice--;
                static int snapshotCounter = 0;
                snapshotCounter++;
                if (snapshotCounter % quantum == 0)
                {
                    std::ofstream snap("memory_stamp_" + std::to_string(snapshotCounter) + ".txt");
                    snap << "Timestamp: (" << getCurrentDateTime() << ")\n";

                    int inMemCount = 0;
                    for (const auto &b : memoryBlocks)
                        if (!b.owner.empty())
                            inMemCount++;
                    snap << "Number of processes in memory: " << inMemCount << "\n";

                    int externalFrag = 0;
                    for (const auto &b : memoryBlocks)
                        if (b.owner.empty())
                            externalFrag += b.size;
                    snap << "Total external fragmentation in KB: " << externalFrag / 1024 << "\n\n";

                    snap << "----end---- = " << MEM_TOTAL << "\n";
                    int cur = MEM_TOTAL;
                    for (auto it = memoryBlocks.rbegin(); it != memoryBlocks.rend(); ++it)
                    {
                        if (!it->owner.empty())
                        {
                            snap << cur << "\n"
                                 << it->owner << "\n"
                                 << (cur - it->size) << "\n";
                        }
                        cur -= it->size;
                    }
                    snap << "----start---- = 0\n";
                }
            }

            if (execScreen->isShutdown)
            {
                freeMemory(execScreen->name);
                execScreen->finishedTime = getCurrentDateTime();
                continue; // Skip requeueing
            }

            if (execScreen->instructionPointer < (int)execScreen->instructions.size())
            {
                // not finished: re-enqueue for next round
                std::lock_guard<std::mutex> requeueLock(queueMutex);
                readyQueue.push(execScreen);
                cv.notify_one();
            }
            else
            {
                freeMemory(execScreen->name);
                execScreen->finishedTime = getCurrentDateTime();
            }
        }
        else
        {
            // FCFS: execute until completion
            while (execScreen->instructionPointer < (int)execScreen->instructions.size())
            {
                Instruction &inst = execScreen->instructions[execScreen->instructionPointer];
                std::string logEntry;
                // [Same instruction handling switch as above]
                switch (inst.type)
                {
                case InstructionType::DECLARE:
                    execScreen->memory.vars[inst.var1] = inst.value;
                    logEntry = "Declared " + inst.var1 + " = " + std::to_string(inst.value);
                    break;
                case InstructionType::PRINT:
                    if (inst.message.find("Hello world from") != std::string::npos)
                    {
                        logEntry = inst.message;
                    }
                    else
                    {
                        logEntry = inst.message;
                        logEntry += execScreen->memory.vars.count(inst.var1)
                                        ? std::to_string(execScreen->memory.vars[inst.var1])
                                        : std::string("undefined");
                    }
                    break;

                case InstructionType::ADD:
                {
                    uint16_t a = execScreen->memory.vars[inst.var2];
                    uint16_t b = execScreen->memory.vars[inst.var3];
                    execScreen->memory.vars[inst.var1] = a + b;
                    logEntry = "Added: " + inst.var1 + " = " + std::to_string(execScreen->memory.vars[inst.var1]);
                    break;
                }
                case InstructionType::SUBTRACT:
                {
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
                if (outFile.is_open())
                {
                    outFile << "(" << execScreen->lastLogTime << ") Core:" << coreId << " " << logEntry << "\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
            }
            freeMemory(execScreen->name);
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

std::vector<Instruction> parseInstructionString(const std::string &raw, const std::string &procName)
{
    std::vector<Instruction> instructions;
    std::istringstream iss(raw);
    std::string inst;

    while (std::getline(iss, inst, ';'))
    {
        std::istringstream tokenStream(inst);
        std::string type;
        tokenStream >> type;
        type.erase(type.find_last_not_of(" \t\n\r\"") + 1); // trim right
        type.erase(0, type.find_first_not_of(" \t\n\r\"")); // trim left
        if (type.find("PRINT") == 0)
            type = "PRINT";

        if (type == "DECLARE")
        {
            Instruction ins{InstructionType::DECLARE};
            tokenStream >> ins.var1 >> ins.value;
            instructions.push_back(ins);
        }
        else if (type == "ADD")
        {
            Instruction ins{InstructionType::ADD};
            tokenStream >> ins.var1 >> ins.var2 >> ins.var3;
            instructions.push_back(ins);
        }
        else if (type == "SUBTRACT")
        {
            Instruction ins{InstructionType::SUBTRACT};
            tokenStream >> ins.var1 >> ins.var2 >> ins.var3;
            instructions.push_back(ins);
        }
        else if (type == "SLEEP")
        {
            Instruction ins{InstructionType::SLEEP};
            int ticks;
            tokenStream >> ticks;
            ins.sleepTicks = static_cast<uint8_t>(ticks);
            instructions.push_back(ins);
        }
        else if (type == "PRINT")
        {
            Instruction ins{InstructionType::PRINT};
            std::string fullLine = inst.substr(inst.find("PRINT") + 5); // Skip "PRINT"

            if (fullLine.empty())
            {
                ins.message = "Hello world from " + procName + "!";
            }
            else
            {
                // Remove quotes and trim
                size_t plusPos = fullLine.find('+');
                if (plusPos != std::string::npos)
                {
                    std::string msgPart = fullLine.substr(0, plusPos);
                    std::string varPart = fullLine.substr(plusPos + 1);

                    // clean quotes/spaces
                    msgPart.erase(0, msgPart.find_first_not_of(" \t\""));
                    msgPart.erase(msgPart.find_last_not_of(" \t\"") + 1);
                    // Also remove starting ( or trailing )
                    msgPart.erase(
                        std::remove_if(msgPart.begin(), msgPart.end(), [](char c)
                                       { return c == '"' || c == '(' || c == ')'; }),
                        msgPart.end());

                    varPart.erase(0, varPart.find_first_not_of(" \t\""));
                    varPart.erase(varPart.find_last_not_of(" \t\"") + 1);
                    if (!varPart.empty() && !std::isalnum(varPart.back()))
                    {
                        varPart.pop_back();
                    }

                    ins.message = msgPart;
                    ins.var1 = varPart;
                }
                else
                {
                    // pure string print
                    fullLine.erase(0, fullLine.find_first_not_of(" \t\""));
                    fullLine.erase(fullLine.find_last_not_of(" \t\"") + 1);
                    ins.message = fullLine;
                }
            }
            ins.message.erase(
                std::remove(ins.message.begin(), ins.message.end(), '\\'),
                ins.message.end());
            instructions.push_back(ins);
        }
        else if (type == "WRITE")
        {
            Instruction ins{InstructionType::WRITE};
            tokenStream >> ins.var1 >> ins.var2;
            instructions.push_back(ins);
        }
        else if (type == "READ")
        {
            Instruction ins{InstructionType::READ};
            tokenStream >> ins.var1 >> ins.var2;
            instructions.push_back(ins);
        }
    }

    return instructions;
}

void readConfigFile(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file)
    {
        std::cerr << "Error opening " << filename << "\n";
        return;
    }

    std::string param;
    std::cout << "Loaded config parameters:\n";

    while (file >> param)
    {
        if (param == "num-cpu")
        {
            file >> CPU_CORES;
            std::cout << " - num-cpu: " << CPU_CORES << "\n";
        }
        else if (param == "scheduler")
        {
            file >> schedulerAlgo;
            std::cout << " - scheduler: " << schedulerAlgo << "\n";
        }
        else if (param == "quantum-cycles")
        {
            file >> quantum;
            std::cout << " - quantum-cycles: " << quantum << "\n";
        }
        else if (param == "batch-process-freq")
        {
            file >> batchFreq;
            std::cout << " - batch-process-freq: " << batchFreq << "\n";
        }
        else if (param == "min-ins")
        {
            file >> minInstructions;
            std::cout << " - min-ins: " << minInstructions << "\n";
        }
        else if (param == "max-ins")
        {
            file >> maxInstructions;
            std::cout << " - max-ins: " << maxInstructions << "\n";
        }
        else if (param == "delay-per-exec")
        {
            file >> delayPerExec;
            std::cout << " - delay-per-exec: " << delayPerExec << "\n";
        }
        else if (param == "max-overall-mem")
        {
            file >> MEM_TOTAL;
            std::cout << " - max-overall-mem: " << MEM_TOTAL << "\n";
        }
        else if (param == "mem-per-frame")
        {
            file >> MEM_FRAME_SIZE;
            std::cout << " - mem-per-frame: " << MEM_FRAME_SIZE << "\n";
        }
        else if (param == "min-mem-per-proc")
        {
            file >> MIN_MEM_PER_PROC;
            std::cout << " - min-mem-per-proc: " << MIN_MEM_PER_PROC << "\n";
        }
        else if (param == "max-mem-per-proc")
        {
            file >> MAX_MEM_PER_PROC;
            std::cout << " - max-mem-per-proc: " << MAX_MEM_PER_PROC << "\n";
        }

        else
        {
            std::string junk;
            file >> junk;
            std::cout << "Unknown config parameter: " << param << " = " << junk << "\n";
        }
    }

    // Reinitialize memory blocks
    {
        std::lock_guard<std::mutex> lock(memMutex);
        memoryBlocks.clear();
        memoryBlocks.push_back({0, MEM_TOTAL, ""});
    }

    // Reset page/frame system
    {
        std::lock_guard<std::mutex> lock(physicalMemoryMutex);
        physicalMemory.clear();
    }
    frameTable.clear();
    fifoFrameQueue = std::queue<int>();

    int totalFrames = MEM_TOTAL / MEM_FRAME_SIZE;
    frameTable = std::vector<FrameTableEntry>(totalFrames);
    std::cout << " - total-frames: " << totalFrames << "\n";
}

std::vector<Instruction> generateRandomInstructions(int count, const std::string &processName = "")
{
    std::vector<Instruction> instructions;
    std::vector<std::string> vars = {"x", "y", "z"};

    for (int i = 0; i < count; ++i)
    {
        int type = getRand(0, 6);
        switch (type)
        {
        case 0: // DECLARE
        {
            Instruction inst{InstructionType::DECLARE};
            inst.var1 = vars[getRand(0, 2)];
            inst.value = getRand(1, 100);
            instructions.push_back(inst);
            break;
        }
        case 1: // PRINT
        {
            Instruction inst{InstructionType::PRINT};
            inst.var1 = vars[getRand(0, 2)];
            inst.message = "Hello world from " + processName + "!";
            instructions.push_back(inst);
            break;
        }
        case 2: // ADD
        {
            Instruction inst{InstructionType::ADD};
            inst.var1 = vars[getRand(0, 2)];
            inst.var2 = vars[getRand(0, 2)];
            inst.var3 = vars[getRand(0, 2)];
            instructions.push_back(inst);
            break;
        }
        case 3: // SUBTRACT
        {
            Instruction inst{InstructionType::SUBTRACT};
            inst.var1 = vars[getRand(0, 2)];
            inst.var2 = vars[getRand(0, 2)];
            inst.var3 = vars[getRand(0, 2)];
            instructions.push_back(inst);
            break;
        }
        case 4: // SLEEP
        {
            Instruction inst{InstructionType::SLEEP};
            inst.sleepTicks = getRand(1, 3);
            instructions.push_back(inst);
            break;
        }
        case 5: // WRITE
        {

            Instruction inst{InstructionType::WRITE};
            inst.var1 = "0x" + std::to_string(0x1000 + getRand(0, 1024)); // random hex address
            inst.var2 = vars[getRand(0, 2)];
            instructions.push_back(inst);
            break;
        }
        case 6: // READ
        {
            Instruction inst{InstructionType::READ};
            inst.var1 = vars[getRand(0, 2)];
            inst.var2 = "0x" + std::to_string(0x1000 + getRand(0, 1024)); // random hex address
            instructions.push_back(inst);
            break;
        }
        }
    }

    return instructions;
}

bool isPowerOfTwo(int x)
{
    return (x >= 64 && x <= 8192) && (x & (x - 1)) == 0;
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

        if (!isInitialized && !(command[0] == "initialize"))
        {
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

                schedulerGeneratorThread = std::thread([&screens]()
                                                       {
                    int nextPid = 1;
                    while (schedulerRunning) {
                        ExecutableScreen exec{};
                        exec.name = "p" + std::to_string(nextPid++);
                        exec.instructions = generateRandomInstructions(
                                            getRand(minInstructions, maxInstructions), exec.name);
                        exec.totalLines    = exec.instructions.size();
                        exec.createdDate   = getCurrentDateTime();
                        
                        int memSize;
                        do {
                            memSize = getRand(MIN_MEM_PER_PROC, MAX_MEM_PER_PROC);
                        } while (!isPowerOfTwo(memSize));

                        int allocStart = allocateMemory(exec.name, memSize);
                        exec.memorySize = memSize;

                        if (allocStart == -1) {
                            // std::cout << "No memory for " << exec.name << ", requeueing.\n";
                            std::this_thread::sleep_for(std::chrono::milliseconds(batchFreq * delayPerExec));
                            continue;
                        }


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
                    } });
                schedulerGeneratorThread.detach();
                if (!isPrinting)
                {
                    isPrinting = true;
                    stopScheduler = false;
                    // Spawn CPU workers once
                    for (int i = 0; i < CPU_CORES; ++i)
                    {
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
        else if (command[0] == "process-smi")
        {
            if (currentScreen.name == "Main Menu")
            {
                std::cout << "Cannot do this in the main menu";
            }
            else
            {
                // Find the live process in your master list
                auto it = std::find_if(
                    screens.begin(), screens.end(),
                    [&](const ExecutableScreen &s)
                    { return s.name == currentScreen.name; });
                if (it != screens.end())
                {
                    auto &proc = *it;
                    std::cout << "Process: " << proc.name;
                    if (proc.currentLine < proc.totalLines)
                    {
                        std::cout << " (Running)\n";
                        std::cout << "  Executed " << proc.currentLine << " / " << proc.totalLines << " instructions\n";
                    }
                    else
                    {
                        std::cout << " (Finished)\n";
                    }
                    std::cout << "  Last Log Time: " << proc.lastLogTime << "\n";
                    std::cout << "  CPU Core: " << proc.cpuId << "\n";
                    std::cout << "  Variables:\n";
                    for (auto &kv : proc.memory.vars)
                    {
                        std::cout << "    " << kv.first << " = " << kv.second << "\n";
                    }
                }
                else
                {
                    std::cout << "Process " << currentScreen.name << " not found.\n";
                }
            }
        }
        else if (command[0] == "vmstat")
        {
            std::lock_guard<std::mutex> lock(memMutex);
            int usedMem = 0;
            int freeMem = 0;
            for (const auto &block : memoryBlocks)
            {
                if (block.owner.empty())
                    freeMem += block.size;
                else
                    usedMem += block.size;
            }

            std::cout << "\n------ VMSTAT REPORT ------\n";
            std::cout << "Total memory       : " << MEM_TOTAL << " bytes\n";
            std::cout << "Used memory        : " << usedMem << " bytes\n";
            std::cout << "Free memory        : " << freeMem << " bytes\n";
            std::cout << "Active CPU ticks   : " << activeTicks.load() << "\n";
            std::cout << "Idle CPU ticks     : " << idleTicks.load() << "\n";
            std::cout << "Total CPU ticks    : " << totalTicks.load() << "\n";
            std::cout << "Pages Paged In     : " << pagesPagedIn.load() << "\n";
            std::cout << "Pages Paged Out    : " << pagesPagedOut.load() << "\n";
            std::cout << "----------------------------\n\n";
        }
        else if (command[0] == "clear" && currentScreen.name == "Main Menu")
        {
            clearScreen();
            printHeader();
        }
        else if (command[0] == "screen" && currentScreen.name == "Main Menu")
        {
            /* if (command[1] == "-s" && command.size() == 3)
             {
                 ExecutableScreen proc = createScreen(command[2]);

                 proc.instructions = generateRandomInstructions(
                     getRand(minInstructions, maxInstructions));

                 proc.totalLines = static_cast<int>(proc.instructions.size());
                 {
                     std::lock_guard<std::mutex> lg(screensMutex);
                     screens.push_back(std::move(proc));
                     // **Immediately** enqueue the new process:
                     ExecutableScreen *p = &screens.back();
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
             else */
            if (command[1] == "-s" && command.size() == 4)
            {
                std::string procName = command[2];
                int memSize = std::stoi(command[3]);

                if (memSize < 64 || memSize > 8192 || (memSize & (memSize - 1)) != 0)
                {
                    std::cout << "Invalid memory allocation.\n";
                    continue;
                }

                ExecutableScreen proc = createScreen(procName);
                proc.memorySize = memSize;
                proc.instructions = generateRandomInstructions(
                    getRand(minInstructions, maxInstructions), procName);
                proc.totalLines = static_cast<int>(proc.instructions.size());

                int allocStart = allocateMemory(procName, memSize);
                if (allocStart == -1)
                {
                    std::cout << "Memory allocation failed.\n";
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lg(screensMutex);
                    screens.push_back(std::move(proc));
                    ExecutableScreen *p = &screens.back();
                    {
                        std::lock_guard<std::mutex> ql(queueMutex);
                        readyQueue.push(p);
                    }
                    cv.notify_one();
                }

                currentScreen = screens.back();
                clearScreen();
                printScreen(currentScreen);
            }
            else if (command[1] == "-r" && command.size() == 3)
            {
                bool found = false;
                for (auto &s : screens)
                {
                    if (s.name == command[2])
                    {
                        currentScreen = s;
                        clearScreen();

                        if (s.isShutdown)
                        {
                            std::cout << s.shutdownMessage << "\n";
                        }
                        else
                        {
                            printScreen(s);
                        }
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    std::cout << "Process " + command[2] + " not found.\n";
                }
            }
            else if (command[1] == "-ls" && command.size() == 2)
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
            else if (command[1] == "-c" && command.size() >= 4)
            {
                std::string procName = command[2];
                int memSize = std::stoi(command[3]);

                // Memory validation
                if (memSize < 64 || memSize > 8192 || (memSize & (memSize - 1)) != 0)
                {
                    std::cout << "Invalid memory allocation.\n";
                    return 0;
                }

                // Reconstruct instruction string (everything after the 4th token)
                size_t firstQuote = cmd.find("\"");
                size_t lastQuote = cmd.rfind("\"");
                std::string rawInstructions;
                if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote)
                {
                    rawInstructions = cmd.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                }
                else
                {
                    std::cout << "Invalid instruction format.\n";
                    continue;
                }

                ExecutableScreen proc = createScreen(procName);
                proc.instructions = parseInstructionString(rawInstructions, procName);

                if (proc.instructions.size() < 1 || proc.instructions.size() > 50)
                {
                    std::cout << "Number of instructions should be between 1-50\n";
                    continue;
                }

                proc.totalLines = static_cast<int>(proc.instructions.size());

                proc.memorySize = memSize;
                int allocStart = allocateMemory(procName, memSize);
                if (allocStart == -1)
                {
                    std::cout << "Memory allocation failed.\n";
                    return 0;
                }

                {
                    std::lock_guard<std::mutex> lg(screensMutex);
                    screens.push_back(std::move(proc));
                    ExecutableScreen *p = &screens.back();
                    {
                        std::lock_guard<std::mutex> ql(queueMutex);
                        readyQueue.push(p);
                    }
                    cv.notify_one();
                }

                if (!isPrinting)
                {
                    isPrinting = true;
                    stopScheduler = false;
                    for (int i = 0; i < CPU_CORES; ++i)
                    {
                        cpuThreads.emplace_back(cpuWorker, i);
                    }
                }

                currentScreen = screens.back();
                clearScreen();
                printScreen(currentScreen);
            }
        }
        else if (command[0] == "generate" && command.size() == 2)
        {
            int n = std::stoi(command[1]);
            for (int i = 0; i < n; ++i)
            {
                std::string pname = "p" + std::to_string(i + 1);
                ExecutableScreen proc = createScreen(pname);
                // ← assign real instructions here:
                proc.instructions = generateRandomInstructions(
                    getRand(minInstructions, maxInstructions), pname);
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
            totalTicks = 0;
            activeTicks = 0;
            idleTicks = 0;
            pagesPagedIn = 0;
            pagesPagedOut = 0;
            std::cout << command[0] << " command recognized. Doing something.\n";
            readConfigFile("config.txt");
            isInitialized = true;
        }
        else if (command[0] == "scheduler-test" && currentScreen.name == "Main Menu")
        {
            std::cout << "Starting scheduler test...\n";

            std::thread testThread([&screens]()
                                   {
                int nextPid = 1;
                int count = 5;  // You can change or read this from a config/command if needed
                for (int i = 0; i < count; ++i)
                {
                    ExecutableScreen exec{};
                    exec.name = "test" + std::to_string(nextPid++);
                    exec.instructions = generateRandomInstructions(getRand(minInstructions, maxInstructions), exec.name);
                    exec.totalLines = static_cast<int>(exec.instructions.size());
                    exec.createdDate = getCurrentDateTime();

                    int memSize;
                    do {
                        memSize = getRand(MIN_MEM_PER_PROC, MAX_MEM_PER_PROC);
                    } while (!isPowerOfTwo(memSize));

                    int allocStart = allocateMemory(exec.name, memSize);
                    exec.memorySize = memSize;

                    if (allocStart == -1) {
                        std::cout << "[scheduler-test] No memory for " << exec.name << ", skipping.\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(batchFreq * delayPerExec));
                        continue;
                    }

                    {
                        std::lock_guard<std::mutex> lg(screensMutex);
                        screens.push_back(std::move(exec));
                        ExecutableScreen* p = &screens.back();
                        {
                            std::lock_guard<std::mutex> ql(queueMutex);
                            readyQueue.push(p);
                        }
                        cv.notify_one();
                    }

                    std::cout << "[scheduler-test] Generated process " << exec.name << " with " 
                            << memSize << " bytes and " << exec.totalLines << " instructions.\n";

                    std::this_thread::sleep_for(std::chrono::milliseconds(batchFreq * delayPerExec));
                } });

            testThread.detach();

            if (!isPrinting)
            {
                isPrinting = true;
                stopScheduler = false;
                for (int i = 0; i < CPU_CORES; ++i)
                {
                    cpuThreads.emplace_back(cpuWorker, i);
                }
            }
        }

        else if (command[0] == "scheduler-stop" && currentScreen.name == "Main Menu")
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