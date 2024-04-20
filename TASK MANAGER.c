#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>

#define MAX_PID_LENGTH 16
#define MAX_NAME_LENGTH 256

int get_process_name(const char* pid, char* name, int name_length) {
    char comm_file_path[256];
    snprintf(comm_file_path, sizeof(comm_file_path), "/proc/%s/comm", pid);

    FILE* comm_file = fopen(comm_file_path, "r");
    if (!comm_file) {
        return 0;
    }

    fgets(name, name_length, comm_file);

    fclose(comm_file);

    name[strcspn(name, "\n")] = 0;

    return 1;
}

int get_process_arguments(const char* pid, char* arguments, int arguments_length) {
    char cmdline_file_path[256];
    snprintf(cmdline_file_path, sizeof(cmdline_file_path), "/proc/%s/cmdline", pid);

    FILE* cmdline_file = fopen(cmdline_file_path, "r");
    if (!cmdline_file) {
        return 0;
    }

    fgets(arguments, arguments_length, cmdline_file);

    fclose(cmdline_file);

    for (int i = 0; i < arguments_length; i++) {
        if (arguments[i] == '\0') {
            arguments[i] = ' ';
        }
    }

    return 1;
}

int get_process_status(const char* pid, char* status, int status_length) {
    char status_file_path[256];
    snprintf(status_file_path, sizeof(status_file_path), "/proc/%s/status", pid);

    FILE* status_file = fopen(status_file_path, "r");
    if (!status_file) {
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), status_file)) {
        if (strncmp(line, "State:", 6) == 0) {
            strncpy(status, line + 7, status_length);
            break;
        }
    }

    fclose(status_file);

    status[strcspn(status, "\n")] = 0;

    return 1;
}

int get_process_memory(const char* pid, unsigned long* rss_kb, unsigned long* vsz_kb) {
    char statm_file_path[256];
    snprintf(statm_file_path, sizeof(statm_file_path), "/proc/%s/statm", pid);

    FILE* statm_file = fopen(statm_file_path, "r");
    if (!statm_file) {
        return 0;
    }

    fscanf(statm_file, "%lu %lu", rss_kb, vsz_kb);

    fclose(statm_file);

    *rss_kb *= getpagesize() / 1024;
    *vsz_kb *= getpagesize() / 1024;

    return 1;
}

void list_processes() {
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        printf("Error: %s\n", strerror(errno));
        return;
    }

    printf("%-s \t\t%-10s \t\t%-10s \t\t%-10s \t%-10s\n", "PID", "STATUS", "RSS (KB)", "VSZ (KB)", "NAME");
    
    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != NULL) {
    if (entry->d_type != DT_DIR) {
        continue;
    }

    const char* pid_str = entry->d_name;

    if (strspn(pid_str, "0123456789") != strlen(pid_str)) {
        continue;
    }

    char name[MAX_NAME_LENGTH];
    if (!get_process_name(pid_str, name, sizeof(name))) {
        continue;
    }

    char status[256];
    if (!get_process_status(pid_str, status, sizeof(status))) {
        continue;
    }

    unsigned long rss_kb, vsz_kb;
    if (!get_process_memory(pid_str, &rss_kb, &vsz_kb)) {
        continue;
    }

    printf("%-10s \t%-10s \t\t%-10lu \t\t%-10lu \t%s\n", pid_str, status, rss_kb, vsz_kb, name);
}
    closedir(proc_dir);
}

void kill_process(const char* pid) {
    if (kill(atoi(pid), SIGKILL) == -1) {
    printf("Error: %s\n", strerror(errno));
    }
}

float get_elapsed_time() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        printf("Error: %s\n", strerror(errno));
        return -1;
    }
    return (float)info.uptime;
}


float get_cpu_usage(int pid) {
    char stat_path[MAX_NAME_LENGTH];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
    FILE* stat_file = fopen(stat_path, "r");
    if (stat_file == NULL) {
        printf("Error: %s\n", strerror(errno));
        return -1;
    }

    char line[MAX_NAME_LENGTH];
    fgets(line, sizeof(line), stat_file);

    fclose(stat_file);

    unsigned long utime, stime;
    sscanf(line, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %lu %lu", &utime, &stime);

    long clock_ticks = sysconf(_SC_CLK_TCK);

    float total_time = (float)(utime + stime) / clock_ticks;

    float elapsed_time = get_elapsed_time();

    int num_cpus = get_num_cpus();

    float cpu_usage = (total_time / (elapsed_time * num_cpus)) * 100;

    return cpu_usage;
}

int get_num_cpus() {
    FILE* cpuinfo_file = fopen("/proc/cpuinfo", "r");
    if (cpuinfo_file == NULL) {
    printf("Error: %s\n", strerror(errno));
    return -1;
    }
    int count = 0;
    char line[MAX_NAME_LENGTH];
    while (fgets(line, sizeof(line), cpuinfo_file) != NULL) {
    if (strncmp(line, "processor", 9) == 0) {
        count++;
    }
}
    fclose(cpuinfo_file);
    return count;
}

void set_process_priority(int pid, int priority) {
    int result = nice(priority);
    if (result == -1) {
        perror("nice");
        return;
    }
    
    result = setpriority(PRIO_PROCESS, pid, priority);
    if (result == -1) {
        perror("setpriority");
        return;
    }
}

int get_process_priority(int pid) {
    int priority = getpriority(PRIO_PROCESS, pid);
    if (priority == -1) {
        perror("getpriority");
        exit(EXIT_FAILURE);
    }
    return priority;
}

int main() {
    int opt;
    printf("\n\n\n\t\t\t\t\t\tTASK MANAGER");
    while (1) {
    printf("\n1. List processes\n");
    printf("2. Kill process\n");
    printf("3. CPU Usage: \n");
    printf("4. Number Of CPUs: \n");
    printf("5. Set Process Priority: \n");
    printf("6. Get Process Priority: \n");
    printf("7. Quit: \n");
    
    printf("Choose an option: ");
    scanf("%d",&opt);

    if (opt == 1) {
        list_processes();
        printf("\n");
    }
    else if (opt == 2) {
        printf("Enter the PID of the process to kill: ");
        char pid[MAX_PID_LENGTH];
        fgets(pid, sizeof(pid), stdin);
        kill_process(pid);
        printf("\n");
    }
    else if (opt == 3){
   	int pid;
    	printf("Enter PID to check CPU Usage: ");
    	scanf("%d",&pid);
	float cpu_usage = get_cpu_usage(pid);
    	printf("CPU Usage for PID %d: %.2f%%\n", pid, cpu_usage);
    	printf("\n");
    }
    else if (opt == 4) {
        int co = get_num_cpus();
        printf("No. of CPUs: %d\n",co);
        printf("\n");
    } 
    else if (opt == 5) {
    	int pid, pri;
    	printf("Enter PID to set its priority: ");
    	scanf("%d",&pid);
    	printf("Enter Priority: ");
    	scanf("%d",&pri);
    	set_process_priority(pid, pri);
    	printf("\n");
    } 
    else if (opt == 6) {
    	int pid;
    	printf("Enter PID to check its priority: ");
    	scanf("%d",&pid);
    	int pri = get_process_priority(pid);
    	printf("Process Priority: %d\n",pri);
    	printf("\n");
    } 
    else if (opt == 7) {
    	printf("Thank You for using Task Manager");
    	printf("\n");
        break;
    }
    else{
    	printf("Invalid Choice Re-try"); 
    	printf("\n");
    }
    }
    return 0;
}

