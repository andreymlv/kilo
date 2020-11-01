
/*** includes ***/

#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/*** defines ***/

#define BUFFER_SIZE 32
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig
{
    int screenrows;
    int screencolumns;
    struct termios original_termios;  
};

struct editorConfig E;

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1)
        die("tcsetattr");
}

void enableRawMode()
{
    // getting attributes from original structure
    if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1)
        die("tcgetattr");

    // at exit of program return original terminal flags
    atexit(disableRawMode);

    // structure storage flags for our environment
    struct termios raw = E.original_termios;
    
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

int getCursorPosition(int *rows, int *colums)
{
    char buffer[BUFFER_SIZE];
    unsigned int index = 0;

    // getting cursor postion
    if (write(STDIN_FILENO, "\x1b[6n", 4) != 4)
    {
        return -1;
    }
    
    while (index < BUFFER_SIZE - 1)
    {
        if (read(STDIN_FILENO, &buffer[index], 1) != 1)
        {
            break;
        }
        if (buffer[index] == 'R')
        {
            break;
        }

        index++;
    }

    buffer[index] = '\0';

    if (buffer[0] != '\x1b' || buffer[1] != '[')
    {
        return -1;
    }
    if (sscanf(&buffer[2], "%d;%d", rows, colums) != 2)
    {
        return -1;
    }
    
    return 0;
}

int getWindowSize(int *rows, int *colums)
{
    // getting windows size
    struct winsize ws;
    
    // ioctl setting up to windows size params
    if ((ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) || ws.ws_col == 0)
    {
        // moving the cursor to the bottom-right corner
        if (write(STDIN_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        {
            return -1;
        }
        return getCursorPosition(rows, colums);
    }
    else
    {
        // setting up rows and colums
        *rows = ws.ws_row;
        *colums = ws.ws_col;
        return 0;
    }
}

/*** append buffer ***/

struct append_buffer
{
    char *buffer;
    int len;
};

#define APPEND_BUFFER_INIT {NULL, 0}

void ab_append(struct append_buffer *ab, const char *s, int len)
{
    char *new = realloc(ab->buffer, ab->len + len);

    if (new == NULL)
    {
        return;
    }

    memcpy(&new[ab->len], s, len);
    ab->buffer = new;
    ab->len += len;
}

void ab_free(struct append_buffer *ab)
{
    // a destructor that frees memory used by abuf
    free(ab->buffer);
}

/*** output ***/

void editorDrawRows(struct append_buffer *ab)
{
    int y;
    for (y = 0; y < E.screenrows; ++y)
    {
        ab_append(ab, "~", 1);

        if (y < E.screenrows - 1)
        {
            ab_append(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen()
{
    struct append_buffer ab = APPEND_BUFFER_INIT;

    // clear entire screen
    ab_append(&ab, "\x1b[2J", 4);
    // move cursor to upper left side
    ab_append(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    ab_append(&ab, "\x1b[H", 3);
    write(STDIN_FILENO, ab.buffer, ab.len);

    ab_free(&ab);
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

void initEditor()
{
    if (getWindowSize(&E.screenrows, &E.screencolumns) == -1)
        die("getWindowSize");
}

int main(void)
{
    // disabling canonical mode and enabling raw mode
    enableRawMode();
    // setting up window size of terminal
    initEditor();


    // reading 1 bit and setting that bit to [c]
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

