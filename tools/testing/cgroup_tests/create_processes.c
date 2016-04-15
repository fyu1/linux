#include <stdio.h>
#include <math.h>
#include <stdlib.h>			// system()
#include <unistd.h>			// chdir()

int main(int argc, char **argv) {
	
	int width, depth, no_of_tasks_per_file;
	int total = 0;
	int no_of_processes = 0;
	int i;
	FILE *fp;
	
	system("rm tasks");
	
	if (argc > 1) {
		width = atoi (argv[1]);
		depth = atoi (argv[2]);
		no_of_tasks_per_file = atoi (argv[3]);
	}
	
	while (depth > 0) {
		total += pow (width, depth);
		depth--;
	}
	
	total++;
	no_of_processes = no_of_tasks_per_file * total;
	
	pid_t pid[no_of_processes];
	
	for (i=0; i<no_of_processes; i++) {
		pid[i] = fork();
		if (pid[i] == 0)
			sleep(36000);
	}
	
	fp = fopen("tasks", "w");
	
	if (fp == NULL) {
		printf("Unable to create file\n");
		return 0;
	}
	
	for (i=0; i<no_of_processes; i++) {
		fprintf(fp, "%d\n", pid[i]);
	}
	
	fclose(fp);
	
	return 0;
}
