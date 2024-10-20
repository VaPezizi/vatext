#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/*====(Defines)====*/
#define CONTROL_KEY(k) ((k) & 0x1f)	//Sets upper 3 bits to 0. (Basically mimics how Ctrl key works in the terminal)

/*====( Variables )====*/

struct termios originalSettings;		//Saving the terminals original attributes here

/* ====(Terminal related functions (Raw mode etc))==== */

void kill(const char * c){
	perror(c);
	exit(1);
}

//Simply restores the terminal to 
void disableRawMode(){
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalSettings) == -1) kill("tcsetattr");
}

//Enables "Raw mode" (Gives data directly to the Program without interprenting special characters)
void enableRawMode(){
	if(tcgetattr(STDIN_FILENO, &originalSettings) == -1) kill("tcsetattr");	//Saving settings here
	atexit(disableRawMode);

	//What these Flags mean, can be found here: https://www.man7.org/linux/man-pages/man3/termios.3.html
	struct termios new = originalSettings;
	new.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);		//Turning off some variables (~ is a bitwise NOT operator)
	new.c_iflag &= ~(BRKINT | ISTRIP | INPCK | IXON | ICRNL);			//Note to self:
	new.c_oflag &= ~(OPOST);	//Output flags			//	ECHO, ICANON etc are bitflags
	new.c_cflag |= (CS8);						//	c_lflag is a series of 1 and 0 and for example ECHO is 1000
	new.c_cc[VMIN] = 0;						//	We set attributes here by doing a bit flip on for example ECHO and ICANON bits on c_lflag
	new.c_cc[VTIME] = 1;
	
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &new)==-1)kill("tcsetattr");	//Setting modified variables.

}

char readKey(){
	int error;
	char c;
	while((error = read(STDIN_FILENO, &c, 1)) != 1){
		if(error == -1 && errno != EAGAIN) kill("read");
	}

	return c;
}


/* ====(input)==== */

void processKeyPress(){
	char c = readKey();
	switch (c) {
		case CONTROL_KEY('q'):
			exit(0);
			break;	
	}

}

/* ====(init)==== */

int main(){
	enableRawMode();	//Enabling rawmode here	
	while(1){
/*		char c = '\0';
		if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) kill("read");
		if(iscntrl(c)){
			printf("%d\r\n", c);
		}else{
			printf("%d ('%c')\r\n", c, c);	//\r is carriage return, Puts the cursor to the left side, when starting a new line
		}
		if(c== CONTROL_KEY('q'))break;
	}*/
		processKeyPress();
	}

	return 0;
}
