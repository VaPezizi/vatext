#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>


int main(){
	char c;
	while(read(STDIN_FILENO, &c, 1) == 1)puts("MORO");
	return 0;
}
