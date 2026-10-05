#include <unistd.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
#define ERROR_FD 10

int main(){
    char buffer[BUFFER_SIZE];
    while(1){
        int b_read = read(0, buffer, BUFFER_SIZE);
        if(b_read == 0){
            break;
        }
        if(b_read < 0){
            write(2, "read error\n", 11);
            exit(1);
        }

        int start = 0;
        while(start < b_read){
            int end = start;
            while(end < b_read && buffer[end] != '\n'){
                end++;
            }
            int last = end -1;
            if(last >= start && (buffer[last] == '.' || buffer[last] == ';')){
                int length = end - start;
                int b_written = 0;
                while(b_written < length){
                    int res = write(1, buffer + start + b_written, length - b_written);
                    if(res < 0){
                        write(2, "file write error\n", 17);
                        exit(1);
                    }
                    b_written += res;
                }
                if(end < b_read && buffer[end] == '\n'){
                    write(1, "\n", 1);
                }
            }
            else{
                char error_mes[] = "Error: string must end with '.', or ';'\n";
                write(ERROR_FD, error_mes, sizeof(error_mes)-1);
            }
            if (end < b_read && buffer[end] == '\n'){
                start = end +1;
            }
            else{
                start = end;
            }
        }
    }
    return 0;
}