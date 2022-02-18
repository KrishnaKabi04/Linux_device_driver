#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

struct mystruct{
	int repeat;
	char name[64];
};

#define WR_VALUE _IOW('a', 'a', int32_t *)
#define RD_VALUE _IOR('a', 'b', int32_t *)
#define GREETER  _IOW('a', 'c', struct mystruct *)

int main() {
	int answer;
	struct mystruct test = {80, "Krishna"};
	int dev = open("/dev/blockmma", O_WRONLY);
	if(dev == -1) {
		printf("Opening was not possible!\n");
		return -1;
	}

	ioctl(dev, RD_VALUE, &answer);
	printf("The answer is %d\n", answer);

	answer = 2204;

	ioctl(dev, WR_VALUE, &answer);
	ioctl(dev, RD_VALUE, &answer);
	printf("The answer is  now %d\n", answer);

	ioctl(dev, GREETER, &test);

	printf("Opening was successfull!\n");
	close(dev);
	return 0;
}