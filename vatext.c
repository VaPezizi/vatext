#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

struct termios originalSettings;		//Saving the terminals original attributes here

//Simply restores the terminal to 
void disableRawMode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalSettings);
}

//Enables "Raw mode" (Gives data directly to the Program without interprenting special characters)
void enableRawMode(){
	tcgetattr(STDIN_FILENO, &originalSettings);	//Saving settings here
	atexit(disableRawMode);

	struct termios new = originalSettings;
	new.c_lflag &= ~(ECHO | ICANON);		//Turning off some variables (~ is a bitwise NOT operator)
							//Note to self:
							//	ECHO, ICANON etc are bitflags
							//	c_lflag is a series of 1 and 0 and for example ECHO is 1000
							//	We set attributes here by doing a bit flip on ECHO and ICANON bits on c_lflag
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &new);	//Setting modified variables.

}


int main(){
	enableRawMode();	//Enabling rawmode here	

	char c;
	while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q'){
		if(iscntrl(c)){
			printf("%d\n", c);
		}else{
			printf("%d ('%c')\n", c, c);
		}
	}


	return 0;
}
