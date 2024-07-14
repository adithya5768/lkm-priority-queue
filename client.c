#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>

// ioctl
typedef struct obj_info {
	int32_t prio_que_size; 	// current number of elements in priority-queue
	int32_t capacity;		// maximum capacity of priority-queue
} obj_info;

#define PB2_SET_CAPACITY    _IOW(0x10, 0x31, int32_t*)
#define PB2_INSERT_INT      _IOW(0x10, 0x32, int32_t*)
#define PB2_INSERT_PRIO     _IOW(0x10, 0x33, int32_t*)
#define PB2_GET_INFO        _IOR(0x10, 0x34, obj_info*)
#define PB2_GET_MIN         _IOR(0x10, 0x35, int32_t*)
#define PB2_GET_MAX         _IOR(0x10, 0x36, int32_t*)

int main(int argc, char** argv)
{
    int bufsize = 0;

    int fd = open("/proc/partb_2_8", O_RDWR);

    while (1)
    {
        char command;
        char line[100], *pline;
        fgets(line, 100, stdin);
        command = line[0];
        pline = line + 2;

        

        if (command == 'w')
        {
            bufsize = strlen(pline);
            write(fd, pline, bufsize);

            
        }
        else if (command == 'r')
        {
            char s[100];
            read(fd, s, bufsize);
            printf("Read(%d): %s\n", bufsize, s);
        }
        else if (command == 'q')
        {
            break;
        }
        else if (command == 's')
        {
            bufsize = strlen(pline);

            int32_t q;

            sscanf(pline, "%d", &q);
 
            if (ioctl(fd, PB2_SET_CAPACITY, &q) == -1)
            {
                perror("query_apps ioctl get");
            }
        }
        else if (command == 'i')
        {
            bufsize = strlen(pline);

            int32_t q;

            sscanf(pline, "%d", &q);
 
            if (ioctl(fd, PB2_INSERT_INT, &q) == -1)
            {
                perror("query_apps ioctl get");
            }
        }
        else if (command == 'p')
        {
            bufsize = strlen(pline);

            int32_t q;

            sscanf(pline, "%d", &q);
 
            if (ioctl(fd, PB2_INSERT_PRIO, &q) == -1)
            {
                perror("query_apps ioctl get");
            }
        }
        else if(command == 'm')
        {
            int32_t q;
 
            if (ioctl(fd, PB2_GET_MAX, &q) == -1)
            {
                perror("query_apps ioctl get");
            }
            else
            {
                printf("extracted max: %d\n", q);
            }
        }
        else if (command == 'g')
        {
            obj_info q;
            if (ioctl(fd, PB2_GET_INFO, &q) == -1)
            {
                perror("query_apps ioctl get");
            }
            else
            {
                printf("extracted size : %d\n", q.prio_que_size);
                printf("extracted capacity : %d\n", q.capacity);

            }
        }
        else if (command == 'n')
        {
            int32_t q;

            if (ioctl(fd, PB2_GET_MIN, &q) == -1)
            {
                perror("query_apps ioctl get");
            }
            else
            {
                printf("extracted max: %d\n", q);
            }

        }
        else
        {
            printf("invalid operation\n");
        }
    }

    close(fd);

    return 0;
}
