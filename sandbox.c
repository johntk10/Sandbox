// John Kutbay
// CS360
// Sandbox Lab
// This program will act as a shell for the user to prevent the actual shell from being overrun

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <stdbool.h>
#include <sys/wait.h>

void prompt() {
	char *user = getenv("USER");
	char cwd[1024];
	getcwd(cwd, 1024);

	char *home = getenv("HOME");
	char dir[1024];
	if (strncmp(cwd, home, strlen(home)) == 0) { // if in home directory
		dir[0] = '~';
		strcpy(dir+1, cwd+strlen(home)); // set home directory to ~, and copy directory after
	} else {
		strcpy(dir, cwd);
	}
	printf("%s@sandbox:%s> ", user, dir);
}

int getCommands(char *commands[]) {
	char line[1024];
	fgets(line, 1024, stdin); // used https://www.scaler.com/topics/c/c-string-input-output-function/
	line[strlen(line)-1] = '\0'; // get line from command line

	char *command = strtok(line, " "); // used https://stackoverflow.com/questions/4513316/split-string-in-c-every-white-space
	int count = 0;
	while (command != NULL) { // loop through each command separated by a " "
		commands[count] = malloc(strlen(command)+1); // allocate memory and store each in commands
		strcpy(commands[count++], command);
		command = strtok(NULL, " ");
	}
	commands[count] = NULL;
	return count;
}

void expandEV(char *commands[], int count) {
	for (int i = 0;i < count;i++) {
		if (commands[i][0] == '$') {
			int index = 0;
			while (commands[i][index] != '/' && commands[i][index] != '\0') index++;
			
			char *EV;

			if (commands[i][index] != '/') { // if not necessary to concat after the EV, do not
				EV = getenv(commands[i]+1);
				if (EV == NULL) EV = "";
				free(commands[i]);
				commands[i] = strdup(EV);
			} else {
				char *EVcommand = strndup(commands[i]+1, index);
				EVcommand[index-1] = '\0';
				EV = getenv(EVcommand);
				if (EV == NULL) EV = "";
				
				char *tmp = strdup(commands[i]+index); // create a tmp string to store what was after EV in original command
				int length = strlen(EV) + strlen(commands[i]+index) + 1;
				free(commands[i]);
				commands[i] = malloc(length);
				strcpy(commands[i], ""); // not even sure why I need this but I segfault without having something in commands[i] when I strcat
				
				strcat(commands[i], EV);
				strcat(commands[i], tmp);
				free(tmp);
			}
		}
	}
}

void cd(char *commands[], int count) {
	if (count == 1) { // if just cd, go home
		chdir(getenv("HOME"));
    } else {
		if (strcmp(commands[1], "~") == 0) { // cd ~ same as just cd
			chdir(getenv("HOME"));
		} else {
			if (chdir(commands[1]) == -1) perror(commands[1]);
		}
	}
}

bool redirection(char *commands[], int count) {
	
	bool files = false;
	for (int i = 0;i < count;i++) { // loop through checking if files need redirection
		if (commands[i][0] == '>' && commands[i][1] != '>') { // > case
			int fd = open(commands[i]+1, O_WRONLY | O_CREAT, 0666);
			if (fd < 0) {
				perror("open");
				exit(EXIT_FAILURE);
			}

			dup2(fd, STDOUT_FILENO); // connect either STDOUT or STDIN to file for each case
			
			strcpy(commands[i], commands[i]+1); // remove the > or >> or < from file for ease later
			files = true; // set return bool true for ease later

		} else if (commands[i][0] == '>' && commands[i][1] == '>') { // >> case

			int fd = open(commands[i]+2, O_APPEND | O_WRONLY | O_CREAT, 0666);
			if (fd < 0) {
				perror("open");
				exit(EXIT_FAILURE);
			}

			dup2(fd, STDOUT_FILENO);
		
			strcpy(commands[i], commands[i]+2);
			files = true;

		} else if (commands[i][0] == '<') { // < case

			int fd = open(commands[i]+1, O_RDONLY, 0666);
			if (fd < 0) {
				perror("open");
				exit(EXIT_FAILURE);
			}

			dup2(fd, STDIN_FILENO);
		
			strcpy(commands[i], commands[i]+1);
			files = true;
		}
	}
	return files;
}

void resourceLimits(int argc, char *argv[]) {
	struct rlimit rl;

	// used daemons notes for implementation
	int opt;
	int processes = 256;
	int data = 1 << 30;
	int stack = 1 << 30;
	int fd = 256;
	int file = 1 << 30;
	int time = 1 << 30;
	while ((opt = getopt(argc, argv, "p:d:s:n:f:t:")) != -1) {
		switch(opt) {
			case 'p':
				if (sscanf(optarg, "%d", &processes) != 1) {
					fprintf(stderr, "Error with -p switch '%s' is invalid.\n", optarg);
					return;
				}
				break;
			case 'd':
				if (sscanf(optarg, "%d", &data) != 1) {
					fprintf(stderr, "Error with -d switch '%s' is invalid.\n", optarg);
					return;
				}
				break;
			case 's':
				if (sscanf(optarg, "%d", &stack) != 1) {
					fprintf(stderr, "Error with -s switch '%s' is invalid.\n", optarg);
					return;
				}
				break;
			case 'n':
				if (sscanf(optarg, "%d", &fd) != 1) {
					fprintf(stderr, "Error with -n switch '%s' is invalid.\n", optarg);
					return;
				}
				break;
			case 'f':
				if (sscanf(optarg, "%d", &file) != 1) {
					fprintf(stderr, "Error with -f switch '%s' is invalid.\n", optarg);
					return;
				}
				break;
			case 't':
				if (sscanf(optarg, "%d", &time) != 1) {
					fprintf(stderr, "Error with -t switch '%s' is invalid.\n", optarg);
					return;
				}
				break;
			default:
				break;
		}
	}

	if (setrlimit(RLIMIT_NPROC, &(struct rlimit){processes, processes}) == -1) {
		perror("setrlimit");
		return;
	}
	if (setrlimit(RLIMIT_DATA, &(struct rlimit){data, data}) == -1) {
		perror("setrlimit");
		return;
	}
	if (setrlimit(RLIMIT_STACK, &(struct rlimit){stack, stack}) == -1) {
		perror("setrlimit");
		return;
	}
	if (setrlimit(RLIMIT_NOFILE, &(struct rlimit){fd, fd}) == -1) {
		perror("setrlimit");
		return;
	}
	if (setrlimit(RLIMIT_FSIZE, &(struct rlimit){file, file}) == -1) {
		perror("setrlimit");
		return;
	}
	if (setrlimit(RLIMIT_CPU, &(struct rlimit){time, time}) == -1) {
		perror("setrlimit");
		return;
	}

}

int main(int argc, char *argv[]) {

	// forgot I could use my vector class until nearly finished, so just continued with arrays and pointers
	// also was not sure how large to allow for commands and lines, so juggled between 1024 per command size
	// and 256 commands per line
	pid_t pids[256];
	for (int i = 0;i < 256;i++) pids[i] = -1; // setting all pids to -1 for logic later
	char *jobs[256];
	int pidCount = 0; // current pid count in use
	int maxPidCount = 0; // over course of use, this is max pid count used, need for freeing at end

	while (1) {
		prompt();

		char *commands[256];
		int count = getCommands(commands);
		if (count == 0) continue;

		expandEV(commands, count);

		bool background = false;
		if (strcmp(commands[count-1], "&") == 0) {
			background = true;
			commands[count-1] = NULL;
			count--;
		}
		
		// checking for internal commands
		if (strcmp(commands[0], "exit") == 0) {
			return 0;
		} else if (strcmp(commands[0], "jobs") == 0) {
			printf("%d jobs.\n", pidCount);
			for (int i = 0;i < maxPidCount;i++) {
				if (pids[i] > 0) printf("	%d  -  %s\n", pids[i], jobs[i]);
			}
		} else if (strcmp(commands[0], "cd") == 0) {
			cd(commands, count);
		} else { // not internal command

			pid_t pid = fork(); // create child process
		
			if (pid < 0) {
				perror("fork");
				exit(EXIT_FAILURE);
			} else if (pid == 0) { // child 
	
				// check for redirection and set resource limits
				bool files = redirection(commands, count);
				if (argc > 1) resourceLimits(argc, argv);
				
				int exe;
				if (files == false) {
					exe = execvp(commands[0], commands); // if no redirection, need arguments passed
				} else {
					exe = execvp(commands[0], NULL); // if redirection, all files changed in function already
				}
				
				if (exe == -1) {
					perror(commands[0]);
					exit(EXIT_FAILURE);
				}

			} else { // parent
				
				if (background == false) { // if no &, wait on forked process
					waitpid(pid, NULL, 0);
				} else {

					pids[pidCount] = pid; // store the background process
				
					// this is basically creating the strings for the jobs being done in background
					// get length, allocate memory for string, concatentate job string
					int length = 0;
					for (int i = 0;i < count;i++) {
						length += strlen(commands[i]);
					}
					jobs[pidCount] = malloc(length);
					for (int i = 0;i < count-1;i++) {
						strcat(jobs[pidCount], commands[i]);
						strcat(jobs[pidCount], " ");
					}
					strcat(jobs[pidCount], commands[count-1]);
					pidCount++;
					maxPidCount = maxPidCount > pidCount ? maxPidCount : pidCount; // I use maxPidCount for freeing later, it is probably not most efficient logically, but it works
				}
			}
		}

		// I loop through the maxPidCount because it represents the range of possible active processes
		// I check if any of the pids are not -1, signaling that there is an actual pid being used
		// if so, I use waitpid with WNOHANG to check if the process finished and if so, I set it inactive
		for (int i = 0;i < maxPidCount;i++) {

			if (pids[i] != -1) {

				pid_t child = waitpid(pids[i], NULL, WNOHANG);
				if (child > 0) {
					pids[i] = -1; // not active
					pidCount--;
				}
			}
		}

		// free commands allocated
		for (int i = 0;i < count;i++) {
			free(commands[i]);
		}
	}
	// free jobs allocated
	for (int i = 0;i < maxPidCount;i++) {
		if (jobs[i] != NULL) free(jobs[i]);
	}
}
