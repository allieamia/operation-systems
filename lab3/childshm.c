#include <stdint.h>
#include <stdbool.h>

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/mman.h>

#define SHM_SIZE 8192
#define PART_SIZE 4096
#define BUFFER_SIZE 4092

int main(int argc, char *argv[]){
	if (argc != 4){
		return 1;
	}

	const char *shm_name = argv[1];
	const char *sem_zap_name = argv[2];
	const char *sem_obr_name = argv[3];

	int shm = shm_open(shm_name, O_RDWR, 0600);
	if(shm == -1){
		write(2, "SHM open error\n", 15);
		exit(1);
	}

	char *shm_buf = mmap(NULL, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);
	if (shm_buf == MAP_FAILED){
		write(2, "SHM map error\n", 14);
		close(shm);
		exit(1);
	}

	sem_t *sem_zap = sem_open(sem_zap_name, 0);
	if (sem_zap == SEM_FAILED){
		write(2, "SEM_zap open error\n", 15);
		munmap(shm_buf, SHM_SIZE);
    	close(shm);
		exit(1);
	}

	sem_t *sem_obr = sem_open(sem_obr_name, 0);
	if (sem_obr == SEM_FAILED){
		write(2, "SEM_obr open error\n", 15);
		sem_close(sem_zap);
    	munmap(shm_buf, SHM_SIZE);
    	close(shm);
		exit(1);
	}

	char buffer[BUFFER_SIZE];
	bool running = true;
    while(running){

		sem_wait(sem_obr);
		uint32_t *length = (uint32_t *)shm_buf;
		char *str = shm_buf + sizeof(uint32_t);
		char *err = shm_buf + PART_SIZE + sizeof(uint32_t);
		uint32_t *error_length = (uint32_t *)(shm_buf + PART_SIZE);
		err[0] = '\0';
		*error_length = 0;

		if(*length == UINT32_MAX){
			running = false;
		} else if (*length > 0){
			memcpy(buffer, str, *length);
			int start = 0;
			int end = start;
			while(end < *length && buffer[end] != '\n'){
				end++;
			}
			int last = end-1;
			if(last >= start && (buffer[last] == '.' || buffer[last] == ';')){
				int res = write(1, buffer, *length);
				if (res < 0){
					write(2, "file write error\n", 17);
                    exit(1);
				}
			} else{
				const char errm[] = "Error: string must end with '.', or ';'\n";
				memcpy(err, errm, sizeof(errm) - 1);
				*error_length = sizeof(errm) - 1;
			}
		}
		sem_post(sem_zap);
        
	}
	sem_close(sem_zap);
	sem_close(sem_obr);
	munmap(shm_buf, SHM_SIZE);
	close(shm);
    return 0;
}
