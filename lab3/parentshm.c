#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>

#define SHM_SIZE 8192
#define PART_SIZE 4096
#define BUFFER_SIZE 4092

int main(){

	char shm_name[64] = "/shm-";
	char sem_zap_name[64] = "/sem_zap-";
	char sem_obr_name[64] = "/sem_obr-";
	pid_t ppid = getpid();

	int i_shm = 5;
	int i_sem_zap = 9;
	int i_sem_obr = 9;
	do{
		int didgit = ppid%10;
		shm_name[i_shm++] = '0' + didgit;
		sem_zap_name[i_sem_zap++] = '0' + didgit;
		sem_obr_name[i_sem_obr++] = '0' + didgit;
		ppid /= 10;
	}while (ppid > 0);

	shm_name[i_shm] = '\0';
	sem_zap_name[i_sem_zap] = '\0';
	sem_obr_name[i_sem_obr] = '\0';	

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

    int file_fd = open(buffer, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
    if (file_fd == -1){
        write(2, "file open error\n", 16);
        exit(1);
    }

	int shm = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if(shm == -1){
		write(2, "SHM open/create error\n", 22);
		close(file_fd);
		exit(1);
	}

	if (ftruncate(shm, SHM_SIZE) == -1){
		write(2, "SHM resize error\n", 17);
		close(shm);
    	shm_unlink(shm_name);
    	close(file_fd);
		exit(1);
	}

	char *shm_buf = mmap(NULL, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);
	if (shm_buf == MAP_FAILED){
		write(2, "SHM map error\n", 14);
		close(shm);
    	shm_unlink(shm_name);
    	close(file_fd);
		exit(1);
	}

	sem_t *sem_zap = sem_open(sem_zap_name, O_CREAT | O_EXCL, 0600, 0);
	if (sem_zap == SEM_FAILED){
		write(2, "SEM_zap open error\n", 15);
		munmap(shm_buf, SHM_SIZE);
    	close(shm);
    	shm_unlink(shm_name);
    	close(file_fd);
		exit(1);
	}

	sem_t *sem_obr = sem_open(sem_obr_name, O_CREAT | O_EXCL, 0600, 0);
	if (sem_obr == SEM_FAILED){
		write(2, "SEM_obr open error\n", 15);
		sem_close(sem_zap);
    	sem_unlink(sem_zap_name);
    	munmap(shm_buf, SHM_SIZE);
    	close(shm);
    	shm_unlink(shm_name);
    	close(file_fd);
		exit(1);
	}

    pid_t pid = fork();
    if (pid == -1){
        write(2, "fork error\n", 11);
        close(file_fd);
		munmap(shm_buf, 8192);
		close(shm);
		shm_unlink(shm_name);
		sem_close(sem_zap);
		sem_close(sem_obr);
		sem_unlink(sem_zap_name);
		sem_unlink(sem_obr_name);
        exit(1);
    }

    if (pid == 0){
		close(STDIN_FILENO);
        if(dup2(file_fd, 1) == -1){
            write(2, "dup2 stdout error\n", 18);
            exit(1);
        }
        close(file_fd);

        char *args[] = {
            "./childshm",
			shm_name,
			sem_zap_name,
			sem_obr_name,
            NULL
        };
        execv("./childshm", args);
        write(2, "exec error\n", 11);
        _exit(1);
    }

    if(dup2(file_fd, 1) == -1){
            write(2, "dup2 stdout error\n", 18);
            exit(1);
        }
    close(file_fd);

	bool running = true;
    while(running){
        char buf[PART_SIZE - sizeof(uint32_t)];
		ssize_t bytes = read(0, buf, sizeof(buf));
		if (bytes == -1){
			write(2, "stdin read error\n", 17);
			exit(1);
		}
		uint32_t *length = (uint32_t *)shm_buf;
		char *str = shm_buf + sizeof(uint32_t);

		uint32_t *error_length = (uint32_t *)(shm_buf + PART_SIZE);
		char *err = shm_buf + PART_SIZE + sizeof(uint32_t);
		if(bytes > 0){
			*length = bytes;
			memcpy(str, buf, bytes);
		} else {
			*length = UINT32_MAX;
			running = false;
		}
		sem_post(sem_obr);
		sem_wait(sem_zap);

		if (*error_length > 0) {
    	write(2, err, *error_length);
		write(1, err, *error_length);
}
    }  

    wait(NULL);
	sem_unlink(sem_zap_name);
	sem_unlink(sem_obr_name);
	sem_close(sem_zap);
	sem_close(sem_obr);
	munmap(shm_buf, SHM_SIZE);
	shm_unlink(shm_name);
	close(shm);
	
	
    return 0;
}