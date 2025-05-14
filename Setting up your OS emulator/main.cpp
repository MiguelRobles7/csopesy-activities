#include <iostream>
#include <string>
#include <cstdlib>

void printHeader() {
    std::cout
		<< "   ___________ ____  ____  _____________  __ \n"
		<< "  / ____/ ___// __ \\/ __ \\/ ____/ ___/\\ \\/ / \n"
		<< " / /    \\__ \\/ / / / /_/ / __/  \\__ \\  \\  /  \n"
		<< "/ /___ ___/ / /_/ / ____/ /___ ___/ /  / /   \n"
		<< "\\____//____/\\____/_/   /_____//____/  /_/    \n"
		<< "                                             \n"
        << "Type 'exit' to quit, 'clear' to clear the screen\n\n";
}

int main() {
    printHeader();

    std::string cmd;
    while (true) {
        std::cout << "Enter a command: ";
        if (!std::getline(std::cin, cmd)) break;

        if (cmd == "exit") {
            break;
        }
        else if (cmd == "clear") {
    #ifdef _WIN32
            std::system("cls");
    #else
            std::system("clear");
    #endif
            printHeader();
        }
        else if (cmd == "initialize"
              || cmd == "screen"
              || cmd == "scheduler-test"
              || cmd == "scheduler-stop"
              || cmd == "report-util")
        {
            std::cout << cmd << " command recognized. Doing something.\n";
        }
        else {
            std::cout << "Unknown command: " << cmd << "\n";
        }
    }

    return 0;
}
