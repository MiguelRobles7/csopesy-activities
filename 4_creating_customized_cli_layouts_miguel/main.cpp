#include <ncurses.h>
#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <iomanip>  // for std::put_time (needed for formatted output)

// Truncate long strings to fit in UI
std::string truncate(const std::string& str, int width) {
    return (int)str.length() > width ? str.substr(0, width - 3) + "..." : str;
}

void drawLayout() {
    initscr();             // Start curses mode
    noecho();              // Don't echo input
    curs_set(0);           // Hide cursor
    int row = 1;

    // 🕒 Real current datetime
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    std::ostringstream datetime;
    datetime << std::put_time(now, "%a %b %d %H:%M:%S %Y");

    // Header

    mvprintw(row++, 2, "%s", datetime.str().c_str());
    mvprintw(row++, 2, "+---------------------------------------------------------------------------------------+");
    mvprintw(row++, 2, "| NVIDIA-SMI 551.86               Driver Version: 551.86          CUDA Version: 12.4    |");
    mvprintw(row++, 2, "|---------------------------------------+-------------------------+---------------------+");
    mvprintw(row++, 2, "| GPU  Name                   TCC/WDDM  | Bus-Id          Disp.A | Volatile Uncorr. ECC |");
    mvprintw(row++, 2, "| Fan  Temp  Perf         Pwr:Usage/Cap |           Memory-Usage | GPU-Util  Compute M. |");
    mvprintw(row++, 2, "|                                       |                        |               MIG M. |");
    mvprintw(row++, 2, "|=======================================+========================+======================|");
    mvprintw(row++, 2, "|   0  NVIDIA GeForce GTX 1080    WDDM  |   00000000:26:00.0  On |                  N/A |");
    mvprintw(row++, 2, "| 28%%   37C    P8           11W /  180W |      701MiB /  8192MiB |      0%%      Default |");
    mvprintw(row++, 2, "|                                       |                        |                  N/A |");
    mvprintw(row++, 2, "+---------------------------------------+------------------------+----------------------+");
    row++;
    mvprintw(row++, 2, "+---------------------------------------------------------------------------------------+");
    mvprintw(row++, 2, "| Processes:                                                                            |");
    mvprintw(row++, 2, "|  GPU   GI   CI        PID   Type   Process name                            GPU Memory |");
    mvprintw(row++, 2, "|        ID   ID                                                             Usage      |");
    mvprintw(row++, 2, "|=======================================================================================|");

    // Dummy process list
    struct Process {
        int pid;
        std::string type;
        std::string name;
        std::string mem;
    };

    std::vector<Process> processes = {
        {1234, "C+G", "C:\\Windows\\System32\\dwm.exe", "150MiB"},
        {2345, "C+G", "C:\\Widgets\\widget.exe", "120MiB"},
        {3456, "C+G", "C:\\SuperLongWebViewName2.exe", "220MiB"},
        {4567, "C+G", "C:\\Windows\\explorer.exe", "95MiB"},
        {5678, "C+G", "C:\\StartMenuExperienceHost.exe", "75MiB"},
    };

    for (const auto& p : processes) {
        std::string line = "|    0   N/A  N/A      ";
        line += std::to_string(p.pid) + "    ";
        line += p.type + "   ";
        line += truncate(p.name, 35);
        while (line.length() < 79) line += " ";
        line += p.mem;
        while (line.length() < 88) line += " ";
        line += "|";
        mvprintw(row++, 2, "%s", line.c_str());
    }

    mvprintw(row++, 2, "+---------------------------------------------------------------------------------------+");

    refresh();  // Print to screen
    getch();    // Wait for user input
    endwin();   // End curses mode
}

int main() {
    drawLayout();
    return 0;
}
