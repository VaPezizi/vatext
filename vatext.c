#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/*====(Defines)====*/
#define CONTROL_KEY(k) ((k) & 0x1f)	//Sets upper 3 bits to 0. (Basically mimics how Ctrl key works in the terminal)

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

int getWindowSize(int *rows, int * columns){
	struct winsize ws;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1 || ws.ws_col == 0){	//TIOCGWINSZ is just a request wich gives the current window size (Even tough it looks scary)
		return -1;							
	}else{
		*columns = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/* ====(Output)====*/

void drawRows(){
	int x;
	for(x=0; x < config.winRows; x++){
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}

void refreshScreen(){
	write(STDOUT_FILENO, "\x1b[2J", 4);	//Clearing the Screen
	write(STDOUT_FILENO, "\x1b[H", 3);	//Setting the cursor to home position

	drawRows();				//Draws

	write(STDOUT_FILENO, "\x1b[H", 3);
}

/* ====(Input)==== */

//Reading input goes trough this "Filter" type of function, where we look for special keys etc
void processKeyPress(){
	char c = readKey();
	switch (c) {
		case CONTROL_KEY('q'):
			
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);

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
