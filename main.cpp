#include <iostream>
#include <sys/syscall.h>
#include <linux/sched.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <climits>
using namespace std;

pid_t clone3_syscall(struct clone_args *args){
    return (pid_t) syscall(SYS_clone3, args, sizeof(*args));
}

void startContainer(){
    cout << "Starting container..." << endl;

    struct clone_args args{};
    args.flags = CLONE_NEWPID | CLONE_NEWUTS;
    args.exit_signal = SIGCHLD;

    // create new process with new pid namespace

    pid_t pid = clone3_syscall(&args);

    if (pid == -1) {
        cerr << "Failed to create container: " << strerror(errno) << endl;
        return;
    } else if (pid == 0) {
        // child/container process
        const char* hostname = "mycontainer";
        int result = sethostname(hostname, strlen(hostname));

        if (result == 0) {
            std::cout << "Successfully changed hostname to: " << hostname << std::endl;
        } else {
            std::cerr << "Failed to set hostname. Error: " << std::strerror(errno) << std::endl;
            std::cerr << "Are you running this program as root?" << std::endl;
        }
        cout << "Container started with PID: " << getpid() << endl;
    } else {
        // parent process
        cout << "Container created with PID: " << pid << endl;
        
    }


    char hostname[HOST_NAME_MAX + 1]; 
    
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::cout << "GetHostname: " << hostname << std::endl;
    } else {
        std::perror("gethostname failed");
        return;
    }

    if (pid != 0) {
        // wait on container process to finish
        waitpid(pid, NULL, 0);
    }

    

}

int main(){
    startContainer();
    return 0;
}
