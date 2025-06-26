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

std::thread printThread;
bool isPrinting = false;

int CPU_CORES = 4;
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
};

Screen createScreen(std::string name)
{
    Screen newScreen;
    newScreen.cpuId = 0;
    newScreen.currentLine = 0;
    newScreen.totalLines = 100;
    newScreen.createdDate = getCurrentDateTime();
    newScreen.name = name;
    return newScreen;
}

void printScreen(const Screen &screen)
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

std::queue<Screen *> readyQueue;
std::mutex queueMutex;
std::condition_variable cv;
bool stopScheduler = false;

void cpuWorker(int coreId)
{

    while (!stopScheduler)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, []
                { return !readyQueue.empty() || stopScheduler; });

        if (stopScheduler && readyQueue.empty())
            break;

        Screen *screen = readyQueue.front();
        readyQueue.pop();
        lock.unlock();
        // std::cout << "[Core " << coreId << "] Executing " << screen->name << "\n";

        bool headerPrinted = false;

        for (int i = 0; i < screen->totalLines; ++i)
        {
            if (i == 0)
                headerPrinted = false;

            screen->currentLine++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string now = getCurrentDateTime();
            screen->lastLogTime = now;
            std::string filename = output_dir + "/" + screen->name + ".txt";
            std::ofstream outFile(filename, std::ios::app);
            screen->cpuId = coreId;
            if (outFile.is_open())
            {
                if (!headerPrinted)
                {
                    outFile << "Process name: " << screen->name << "\nLogs:\n\n";
                    headerPrinted = true;
                }
                outFile << "(" << now << ") "
                        << "Core:" << coreId
                        << " \"Hello world from " << screen->name << "!\"\n";
                outFile.close();
            }
        }
        screen->finishedTime = getCurrentDateTime();
    }
}

void schedulerThreadFunc(std::vector<Screen> &screens)
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

void startPrintJob(std::vector<Screen> &screens)
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

int main()
{
    bool isInitialized = false; // TODO: Accept no command if "initialize" wasn't run yet. Yes, including exit.
    // TODO: Read config file and store said parameters. Replace any that can be used from FCFS implementation.
    // Said parameters are crucial and relevant to scheduling. 
    printHeader();
    std::string cmd;
    std::vector<Screen> screens;
    Screen currentScreen;
    Screen mainMenu;
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
                Screen newScreen = createScreen(command[2]);
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
        else if (command[0] == "generate") // TODO: implement this in scheduler-start
        {
            if (command[1] == "")
            {
                std::cout << "Specify number of process to generate\n\n";
            }
            else
            {
                for (int i = 0; i < std::stoi(command[1]); ++i)
                {
                    std::string pname = "process" + std::to_string(i);
                    screens.push_back(createScreen(pname));
                }
                std::cout << "Generated " << command[1] << " processes!\n";
            }
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
            isInitialized = true;
            std::cout << command[0] << " command recognized. Doing something.\n";
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
