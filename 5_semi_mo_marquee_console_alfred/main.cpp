#include <windows.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

std::atomic<bool> running(true);
const int inputRow = 15;
const int animHeight = 10;

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

void drawAt(int x, int y, char ch) {
    DWORD written;
    COORD pos = {(SHORT)x, (SHORT)y};
    WriteConsoleOutputCharacterA(hConsole, &ch, 1, pos, &written);
}

void clearAt(int x, int y) {
    drawAt(x, y, ' ');
}

void animate() {
    int x = 0, y = 0;
    int dx = 1, dy = 1;
    const int width = 40;

    while (running) {
        drawAt(x, y, '*');
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        clearAt(x, y);

        x += dx;
        y += dy;

        if (x <= 0 || x >= width - 1) dx = -dx;
        if (y <= 0 || y >= animHeight - 1) dy = -dy;
    }
}

void inputLoop() {
    DWORD mode;
    GetConsoleMode(hConsole, &mode);
    SetConsoleMode(hConsole, ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);

    std::vector<char> buffer;
    INPUT_RECORD record;
    DWORD events;

    COORD cursor = {0, inputRow};
    std::string prompt = "Type something: ";
    WriteConsoleOutputCharacterA(hConsole, prompt.c_str(), prompt.size(), cursor, &events);
    cursor.X += (SHORT)prompt.size();

    while (running) {
        ReadConsoleInputA(hConsole, &record, 1, &events);
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
            char ch = record.Event.KeyEvent.uChar.AsciiChar;

            if (ch == '\r') {  // Enter key
                std::string line(buffer.begin(), buffer.end());
                if (line == "exit") {
                    running = false;
                    break;
                }

                // Display typed line
                COORD outPos = {0, (SHORT)(inputRow + 1)};
                std::string echo = "You typed: " + line + "                ";
                WriteConsoleOutputCharacterA(hConsole, echo.c_str(), echo.size(), outPos, &events);

                // Clear input buffer area
                for (int i = 0; i < 80; ++i)
                    WriteConsoleOutputCharacterA(hConsole, " ", 1, { (SHORT)i, inputRow }, &events);

                // Reprint prompt
                cursor = {0, inputRow};
                WriteConsoleOutputCharacterA(hConsole, prompt.c_str(), prompt.size(), cursor, &events);
                cursor.X += (SHORT)prompt.size();
                buffer.clear();
            }
            else if (ch == '\b' && !buffer.empty()) {  // Backspace
                buffer.pop_back();
                cursor.X--;
                WriteConsoleOutputCharacterA(hConsole, " ", 1, cursor, &events);
                SetConsoleCursorPosition(hConsole, cursor);
            }
            else if (ch >= 32 && ch <= 126) {  // Printable
                buffer.push_back(ch);
                WriteConsoleOutputCharacterA(hConsole, &ch, 1, cursor, &events);
                cursor.X++;
            }
        }
    }
}

int main() {
    // Optional: hide cursor for cleaner animation
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hConsole, &ci);
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &ci);

    std::thread animThread(animate);
    inputLoop();  // on main thread
    animThread.join();

    return 0;
}
