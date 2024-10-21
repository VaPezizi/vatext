#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/*====(Defines)====*/
#define CONTROL_KEY(k) ((k) & 0x1f)	//Sets upper 3 bits to 0. (Basically mimics how Ctrl key works in the terminal)

#define VATEXT_VERSION "0.0.1"
/*====( Variables )====*/

struct editorConfig{
	int winRows;
	int winCols;	
	struct termios originalSettings;	//Saving the terminals original attributes here
};	

struct editorConfig config;
/* ====(Terminal related functions (Raw mode etc))==== */

void kill(const char * c){
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	
	write(STDOUT_FILENO, "\x1b[?25h", 6);

	perror(c);
	exit(1);
}

//Simply restores the terminal to 
void disableRawMode(){
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.originalSettings) == -1) kill("tcsetattr");
}

//Enables "Raw mode" (Gives data directly to the Program without interprenting special characters)
void enableRawMode(){
	if(tcgetattr(STDIN_FILENO, &config.originalSettings) == -1) kill("tcsetattr");	//Saving settings here
	atexit(disableRawMode);

	//What these Flags mean, can be found here: https://www.man7.org/linux/man-pages/man3/termios.3.html
	struct termios new = config.originalSettings;
	new.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);		//Turning off some variables (~ is a bitwise NOT operator)
	new.c_iflag &= ~(BRKINT | ISTRIP | INPCK | IXON | ICRNL);			//Note to self:
	new.c_oflag &= ~(OPOST);	//Output flags			//	ECHO, ICANON etc are bitflags
	new.c_cflag |= (CS8);						//	c_lflag is a series of 1 and 0 and for example ECHO is 1000
	new.c_cc[VMIN] = 0;						//	We set attributes here by doing a bit flip on for example ECHO and ICANON bits on c_lflag
	new.c_cc[VTIME] = 1;
	
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &new)==-1)kill("tcsetattr");	//Setting modified variables.

}

//Does what we did in main before, reads a key and returns it
char readKey(){
	int error;
	char c;
	while((error = read(STDIN_FILENO, &c, 1)) != 1){
		if(error == -1 && errno != EAGAIN) kill("read");
	}

	return c;
}

int getCursorPosition(int *rows, int * columns){
	char buffer[32];
	unsigned int i = 0;
	
	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;		//6n Means we are asking for the cursor position

	while (i < sizeof(buffer) - 1){
		if(read(STDIN_FILENO, &buffer[i], 1) != 1)break;	//Reading the bytes in to the buffer
		if(buffer[i] == 'R')break;				//'R' is in the "Cursor Position Report", that's why we read till there
		i++;
	}
	buffer[i] = '\0';						//Adding a '0 byte' to the end of the string

	if (buffer[0] != '\x1b' || buffer[1] != '[') return -1;		
	if (sscanf(&buffer[2], "%d;%d", rows, columns) != 2) return -1;		//Parsing the resulting two numbers and saving to rows and columns
	return 0;	
}

int getWindowSize(int *rows, int * columns){
	struct winsize ws;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1 || ws.ws_col == 0){	//TIOCGWINSZ is just a request wich gives the current window size (Even tough it looks scary)
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return getCursorPosition(rows, columns);			//Using the fallback method if ioctl Fails 
	}else{
		*columns = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/* ====(Append Buffer)==== */
struct appendBuffer{
	char * buffer;
	int len;
};

#define ABUF_INIT {NULL, 0}	//"Constructor" for out appendBuffer type

//Appends a string to the end of a buffer
void appendBufferAppend(struct appendBuffer * aBuff, const char * s, int len){
	char * new = realloc(aBuff->buffer, aBuff->len + len);		//Reallocating memory for the new buffer

	if(new == NULL) return;
	memcpy(&new[aBuff->len], s, len);		//Copying the appendable string to the end of the buffer
	aBuff->buffer = new;
	aBuff->len += len;
}
//Simply frees the buffer
void freeBuffer(struct appendBuffer * aBuff){
	free(aBuff->buffer);
}


/* ====(Output)====*/

void drawRows(struct appendBuffer * aBuff){
	int x;
	for(x=0; x < config.winRows; x++){
		if(x == config.winRows / 3){
			char welcome [80];
			int welcomeLen = snprintf(welcome, sizeof(welcome), "---- VaText Ver: %s ----", VATEXT_VERSION);		//Printing a simple welcome message

			if(welcomeLen > config.winRows) welcomeLen = config.winRows;
			int padding = (config.winCols - welcomeLen) / 2;	//Getting a paadding to center the string
			
			if(padding){
				appendBufferAppend(aBuff, "~", 1);
				padding--;
			}

			while(padding--) appendBufferAppend(aBuff, " ", 1);	//Adding the padding here
			appendBufferAppend(aBuff, welcome, welcomeLen);
		
		}else{
			appendBufferAppend(aBuff, "~", 1);	//Writing tildes to start of line
		}
	
		appendBufferAppend(aBuff, "\x1b[K", 3);		
		if(x < config.winRows - 1){			//Fixes a bug with adding an extra blank line to the bottom
			//write(STDOUT_FILENO, "\r\n",2);		//Writing carriage return and newline to the screen
			appendBufferAppend(aBuff, "\r\n", 2);
		}
	}
}

void refreshScreen(){
	struct appendBuffer aBuff = ABUF_INIT;	//Creating the buffer

	appendBufferAppend(&aBuff, "\x1b[?25l", 6);	//Hides the cursor for a moment, fixing possible flickering effects (?25l hides the cursor, might not work on all terminal emulators)
	//appendBufferAppend(&aBuff, "\x1b[2J", 4);
	appendBufferAppend(&aBuff, "\x1b[H", 3);

	//write(STDOUT_FILENO, "\x1b[2J", 4);	//Clearing the Screen
	//write(STDOUT_FILENO, "\x1b[H", 3);	//Setting the cursor to home position

	drawRows(&aBuff);				//Draws

	appendBufferAppend(&aBuff, "\x1b[H", 3);
	appendBufferAppend(&aBuff, "\x1b[?25l", 6);	//Another part of fixing any flickering

	write(STDOUT_FILENO, aBuff.buffer, aBuff.len);
	freeBuffer(&aBuff);
}

/* ====(Input)==== */

//Reading input goes trough this "Filter" type of function, where we look for special keys etc
void processKeyPress(){
	char c = readKey();
	switch (c) {
		case CONTROL_KEY('q'):
			
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			//disableRawMode();
			write(STDOUT_FILENO, "\x1b[?25h", 6);
			exit(0);
			break;	
	}

}

/* ====(init)==== */

void initEditor(){
	if(getWindowSize(&config.winRows, &config.winCols) == -1) kill("Get Window size");
}

int main(){
	enableRawMode();	//Enabling rawmode here	
	initEditor();
	while(1){
		refreshScreen();
		processKeyPress();
	}

	return 0;
}
