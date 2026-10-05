#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
#define ERROR_FD 10

int main()
{
    int pipe1[2];
    int pipe2[2];

    if(pipe(pipe1) == -1){
        write(2, "pipe1 error\n", 12);
        exit(1);
    }
    if(pipe(pipe2) == -1){
        write(2, "pipe2 error\n", 12);
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    int filename_len = read(0, buffer, BUFFER_SIZE -1);
    if (filename_len <= 0){
        write(2, "filename read error\n", 20);
        exit(1);
    }
    if (buffer[filename_len-1] == '\n'){
        buffer[filename_len-1] = '\0';
    }
    else {
        buffer[filename_len] = '\0';
    }

    int file_fd = open(buffer, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1){
        write(2, "file open error\n", 16);
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1){
        write(2, "fork error\n", 11);
        close(file_fd);
        exit(1);
    }

    if (pid == 0){
        close(pipe1[1]);
        close(pipe2[0]);
        
        if(dup2(pipe1[0], 0) == -1){
            write(2, "dup2 stdin error\n", 17);
            exit(1);
        }
        if(dup2(file_fd, 1) == -1){
            write(2, "dup2 stdout error\n", 18);
            exit(1);
        }
        if(dup2(pipe2[1], ERROR_FD) == -1){
            write(2, "dup2 error fd error\n", 20);
            exit(1);
        }
        
        close(pipe1[0]);
        close(file_fd);
        close(pipe2[1]);

        char *args[] = {
            (char *)"./child",
            NULL
        };

        char *env[] = {
            NULL
        };

        execve("./child", args, env);
        write(2, "exec error\n", 11);
        exit(1);
    }

    close(pipe1[0]);
    close(pipe2[1]);
    close(file_fd);

    while(1){
        int b_read = read(0, buffer, BUFFER_SIZE);
        if(b_read == 0){
            break;
        }

        if(b_read < 0){
            write(2, "read error\n", 11);
            break;
        }

        int b_written = 0;

        while(b_written < b_read){
            int res = write(pipe1[1], buffer + b_written, b_read - b_written);
            if (res < 0){
                write(2, "pipe1 write error\n", 17);
                break;
            }
            b_written += res;
        }
    }

    close(pipe1[1]);

    while(1){
        int b_read = read(pipe2[0], buffer, BUFFER_SIZE);
        if(b_read == 0){
            break;
        }
        if(b_read < 0){
            write(2, "pipe2 read error\n", 17);
            break;
        }
        write(1, buffer, b_read);
    }

    close(pipe2[0]);
    waitpid(pid, NULL, 0);
    return 0;
}