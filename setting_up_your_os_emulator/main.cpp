#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <vector>

std::string getCurrentDateTime()
{
    time_t t = time(nullptr);
    tm *tm_info = localtime(&t);

    int hour = tm_info->tm_hour % 12;
    if (hour == 0)
        hour = 12;
    const char *ampm = (tm_info->tm_hour >= 12) ? "PM" : "AM";

    char buf[30];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d, %02d:%02d:%02d %s",
             tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_year + 1900,
             hour, tm_info->tm_min, tm_info->tm_sec, ampm);

    return std::string(buf);
}

struct Screen
{
    int currentLine;
    int totalLines;
    std::string createdDate;
    std::string title;
};

struct Process
{
    int gpu;
    std::string gi;
    std::string ci;
    int pid;
    std::string type;
    std::string name;
    std::string memUsage;
};

void createScreen(std::string title)
{
    Screen newScreen;
    newScreen.currentLine = 0;
    newScreen.totalLines = 0;
    newScreen.createdDate = "MM/DD/YYYY HH:MM:SS AM/PM";
    newScreen.title = title;
}

void printScreen(const Screen &screen)
{
    std::cout << "Screen Title: " << screen.title << "\n";
    std::cout << "Current Line: " << screen.currentLine << "/" << screen.totalLines << "\n";
    std::cout << "Created Date: " << screen.createdDate << "\n";
}

void printHeader()
{
    std::cout
        << "   ___________ ____  ____  _____________  __ \n"
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

int main()
{
    printHeader();

    std::string cmd;
    std::vector<Screen> screens;
    Screen currentScreen;
    Screen mainMenu;
    mainMenu.title = "Main Menu";
    currentScreen = mainMenu;

    // Dummy process info
    std::vector<Process> dummyProcesses = {
        {0, "N/A", "N/A", 9021, "C+G", "C:\\Windows\\System32\\dwm.exe", "N/A"},
        {0, "N/A", "N/A", 1468, "C+G", "WutheringWavesLauncher.exe", "N/A"},
        {0, "N/A", "N/A", 9348, "C+G", "msedgewebview2.exe", "N/A"},
        {0, "N/A", "N/A", 1238, "C+G", "StartMenuExperienceHost.exe", "N/A"},
        {0, "N/A", "N/A", 1789, "C+G", "THISISTOTALLYNOTAVIRUS12345678901238971612312312.exe", "N/A"},

    };

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

        if (command.empty())
        {
            continue;
        }
        else if (command[0] == "exit")
        {
            if (currentScreen.title != "Main Menu")
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
        else if (command[0] == "clear" && currentScreen.title == "Main Menu")
        {
            clearScreen();
            printHeader();
        }
        else if (command[0] == "screen" && command.size() == 3 && currentScreen.title == "Main Menu")
        {
            if (command[1] == "-s")
            { // Create a new screen
                Screen newScreen;
                newScreen.currentLine = 0;
                newScreen.totalLines = screens.size() + 1; // TODO: Replace with actual line count in specs?
                newScreen.createdDate = getCurrentDateTime();
                newScreen.title = command[2];
                screens.push_back(newScreen);

                currentScreen = newScreen;
                clearScreen();
                printScreen(newScreen);
            }
            else if (command[1] == "-r")
            { // Resume existing screen
                bool found = false;
                for (int i = 0; i < screens.size(); i++)
                {
                    if (screens[i].title == command[2])
                    {
                        currentScreen = screens[i];
                        clearScreen();
                        printScreen(screens[i]);
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    std::cout << "Screen not found.\n";
                }
            }
        }
        else if (command[0] == "nvidia-smi")
        {

            std::cout << getCurrentDateTime() << "\n";
            std::cout << "+----------------------------------------------------------------------------------------+\n";
            std::cout << "| NVIDIA-SMI 551.86          Driver Version: 551.86               CUDA Version: 12.4     |\n";
            std::cout << "|---------------------------------------+-------------------------+----------------------|\n";
            std::cout << "| GPU  Name                   TCC/WDDM  | Bus-Id           Disp.A | Volatile Uncorr. ECC |\n";
            std::cout << "| Fan  Temp  Perf        Pwr:Usage/Cap  |            Memory-Usage | GPU-Util  Compute M. |\n";
            std::cout << "|                                       |                         | MIG M.               |\n";
            std::cout << "|=======================================+=========================+======================|\n";
            std::cout << "|   0  NVIDIA GeForce GTX 5060 Ti  WDDM |    00000000:26:00.0  On |                  N/A |\n";
            std::cout << "|  28%   37C    P8           11W / 180W |       701MiB /  8192MiB |       0%     Default |\n";
            std::cout << "|                                       |                         |                  N/A |\n";
            std::cout << "+---------------------------------------+-------------------------+----------------------+\n\n";
            std::cout << "+----------------------------------------------------------------------------------------+\n";
            std::cout << "| Processes:                                                                  GPU Memory |\n";
            std::cout << "|  GPU   GI   CI        PID     Type  Process name                                 Usage |\n";
            std::cout << "|========================================================================================|\n";

            int maxLength = 35;
            std::string formattedName;

            for (const auto &p : dummyProcesses)
            {
                formattedName = p.name;
                if (formattedName.length() <= maxLength)
                {
                    formattedName = formattedName + std::string(maxLength - formattedName.length(), ' ');
                }
                else
                {
                    formattedName = formattedName.substr(0, maxLength - 3) + "...";
                }

                std::cout << "|  " << std::setw(3) << p.gpu
                          << "   " << std::setw(3) << p.gi
                          << "  " << std::setw(3) << p.ci
                          << "   " << std::setw(8) << p.pid
                          << "  " << std::setw(5) << p.type
                          << "   " << std::setw(35) << std::left << formattedName
                          << std::right << std::setw(15) << p.memUsage << " |\n";
            }

            std::cout << "+----------------------------------------------------------------------------------------+\n";
        }
        else if ((command[0] == "initialize" || command[0] == "scheduler-test" || command[0] == "scheduler-stop" || command[0] == "report-util") && currentScreen.title == "Main Menu")
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
