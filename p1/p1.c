#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char** argv)
{
	//computes 1+2
	long result = syscall(333, 1, 2);
	printf("test_p1: my_syscall result = %ld\n", result);
	
	//replaces 'o's with '0's 
	char test_string[] = "asdfoqwrovnjipoooo oo a o 8294ijndfg oo";
	printf("test_p1: test_string = %s\n", test_string);
	
	result = (long) syscall(334, test_string);
	printf("test_p1: my_syscall2 result = %ld\n", result);
	printf("test_p1: test_string = %s\n", test_string);
	
	char test_string2[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	printf("test_p1: test_string2: %s\n", test_string2);
	result = (long) syscall(334, test_string2);
	printf("test_p1: my_syscall2 result = %ld\n", result);
	printf("test_p1: test_string2 = %s\n", test_string2);

	return 0;
}
