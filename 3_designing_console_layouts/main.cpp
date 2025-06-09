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