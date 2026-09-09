#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#define TRUE 1
#define FALSE -1

void get_rand_str(char s[], int num)
{
	char *str = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	int i,lstr;
	struct timespec seedp = { 0, 0 };
	char ss[2] = {0};
	lstr = strlen(str);
	clock_gettime(CLOCK_REALTIME, &seedp);

	srand(seedp.tv_sec + seedp.tv_nsec * 1000000000L);
	for(i = 1; i <= num; i++) {
		sprintf(ss,"%c",str[(rand()%lstr)]);
		strcat(s,ss);
	}
}

int main(int argc, const char *argv[])
{
    int fd;
    int nread;
    int n=0,i=0;
    char* dev  = NULL;
    struct termios oldtio,newtio;
	speed_t speed = B115200;
    int next_option,havearg = 0,flow = 0;
    const char *const short_opt = "fd:";
    const struct option long_opt[] = {
        {"devices",1,NULL,'d'},
        {"hard_flow",0,NULL,'f'},
        {NULL,0,NULL,0},
    };
    do{
        next_option = getopt_long(argc,argv,short_opt,long_opt,NULL);
        switch (next_option) {
            case 'd':
                dev = optarg;
                havearg = 1;
                break;
            case '?':
				printf("Usage: %s -d <device> w/r\n", argv[0]);
                break;
            case -1:
                if(havearg)
                    break;
            default:
				printf("Usage: %s -d <device> w/r\n", argv[0]);
                exit(1);

        }
    }while(next_option != -1);
    

    if((dev == NULL) || (argc != 4))
    {
			printf("Usage: %s -d <device> w/r\n", argv[0]);
            exit(1);
    }	
    /*  ´ò¿ª´®¿Ú */
    fd = open(dev, O_RDWR | O_NONBLOCK| O_NOCTTY | O_NDELAY ); 
    if (fd < 0)	{
        printf("Can't Open Serial Port!\n");
        exit(0);	
    }
	
	printf("Welcome to uart test\n");
	
    //save to oldtio
    tcgetattr(fd,&oldtio);
    bzero(&newtio,sizeof(newtio));
    newtio.c_cflag = speed|CS8|CLOCAL|CREAD;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~PARENB;
    newtio.c_iflag = IGNPAR;  
    newtio.c_oflag = 0;
    tcflush(fd,TCIFLUSH);  
    tcsetattr(fd,TCSANOW,&newtio);  
    tcgetattr(fd,&oldtio);
	
    char test[30];
    char buffer[30];
	char to_write_register[] = { 0x01, 0x06, 0x00, 0x01, 0x00, 0xA5, 0x18, 0x71};

	if(strcmp(argv[3],"w") == 0){
		while(1)
		{
			memset(test, 0x00, sizeof(test));
			get_rand_str(test, sizeof(test) - 1);	
			//write(fd, test, sizeof(test));
			write( fd, to_write_register, sizeof(to_write_register));
			sleep(1);
			memset( buffer, 0, sizeof(buffer));
			nread = read( fd, buffer, sizeof(buffer));
			sleep(1);
			if(nread > 0)
			{
				for(int i = 0; i < sizeof(buffer); i++)
				{
					printf("buffer[%d] = 0x%x\n", i, buffer[i]);
				}
			}
			printf("send data:%s\n", test);
		}
		close(fd);
	}else if(strcmp(argv[3],"r") == 0){
		while(1)
		{
			memset(buffer,0,sizeof(buffer));
			n = 0;
			do {
				nread = read(fd, &buffer[n], sizeof(buffer) - n);
				n+= nread;
			}while (n != sizeof(buffer));

			printf("recv data:%s\n", buffer);
		}
		close(fd);
	}else{
		printf("Usage: %s -d <device> w/r\n", argv[0]);
		close(fd);
        	exit(1);
	}
}


