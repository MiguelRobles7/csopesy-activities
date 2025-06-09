#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

using namespace std;

// Enable or disable non-blocking input
void setNonBlockingInput(bool enable)
{
    static struct termios oldt, newt;
    if (enable)
    {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    }
    else
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, 0);
    }
}

// Clear current terminal line
void clearLine()
{
    cout << "\33[2K\r" << flush;
}

// Get current terminal width
int getTerminalWidth()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col > 0 ? w.ws_col : 80; // Fallback to 80 if not available
}

// Marquee display logic
void marquee(const string &message, int refreshRateMs)
{
    int pos = 0;
    setNonBlockingInput(true);

    while (true)
    {
        int width = getTerminalWidth();
        string spacer(width, ' ');
        string view = spacer;

        for (int i = 0; i < message.length(); ++i)
        {
            if ((pos + i) < width)
                view[pos + i] = message[i];
        }

        clearLine();
        cout << view << flush;

        pos = (pos + 1) % width;

        char ch;
        if (read(STDIN_FILENO, &ch, 1) > 0 && ch == 'q')
            break;

        this_thread::sleep_for(chrono::milliseconds(refreshRateMs));
    }

    setNonBlockingInput(false);
}

int main()
{
    string msg;
    int refreshRate = 50;

    cout << "******************************" << endl;
    cout << "* Displaying a marquee console! *" << endl;
    cout << "******************************" << endl;

    cout << "Enter your marquee message: ";
    getline(cin, msg);

    cout << "\nPress 'q' to exit the marquee display.\n\n";

    marquee(msg, refreshRate);

    cout << "\nExited marquee.\n";
    return 0;
}
