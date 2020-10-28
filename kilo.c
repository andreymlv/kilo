
/*** includes ***/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios original_termios;


/*** terminal ***/

void die(const char *s)
{
    // clear entire screen
    write(STDIN_FILENO, "\x1b[2J", 4);
    // move cursor to upper left side
    write(STDIN_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) == -1)
        die("tcsetattr");
}

void enableRawMode()
{
    // getting attributes from original structure
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
        die("tcgetattr");

    // at exit of program return original terminal flags
    atexit(disableRawMode);

    // structure storage flags for our environment
    struct termios raw = original_termios;
    
    // flipping bits in raw.c_lflag ("local flags") for not echoing
    // and for disabling canonical mode for immediately read bit by bit
    // not line by line
    // also disabling ctrl-z ctrl-c
    // disabling ctrl-v
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    
    // disabling ctrl-s and ctrl-q
    // disabling ctrl-m
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON | ICRNL);

    // disabling post-processing output (disabling auto-adding "\r\n")
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    
    // data what to be read if input is waiting from user
    raw.c_cc[VMIN] = 0;
    // time between adding new idle items
    raw.c_cc[VTIME] = 10;

    // setting up attributes to raw and wait for all input
    // and discards any input that hasn't been read
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");

}

char editorReadKey()
{
    int nread;
    char c;
    
    // reading bit by bit to var while no error
    while ((nread = read(STDIN_FILENO, &c, 1))== -1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    return c;
}

/*** output ***/

void editorDrawRows()
{
    int y;
    for (y = 0; y < 24; ++y)
    {
        write(STDIN_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen()
{
    // clear entire screen
    write(STDIN_FILENO, "\x1b[2J", 4);
    // move cursor to upper left side
    write(STDIN_FILENO, "\x1b[H", 3);

    editorDrawRows();

    write(STDIN_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeypress()
{
    char c = editorReadKey();

    switch (c)
    {
        case CTRL_KEY('q'):
            // clear entire screen
            write(STDIN_FILENO, "\x1b[2J", 4);
            // move cursor to upper left side
            write(STDIN_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/*** initialization ***/

int main(void)
{
    // disabling canonical mode and enabling raw mode
    enableRawMode();

    // reading 1 bit and setting that bit to [c]
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

