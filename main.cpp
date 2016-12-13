#include <iostream>
#include <zconf.h>
#include "include/uv.h"
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>

class Observer {
public:
    Observer() : msg_(0) {}

    virtual ~Observer() {}

    inline void update(std::string &&msg) {
        std::cout << msg << std::endl;
    }

    inline void Pump() {
        msg_++;
    }

    inline int64_t msg() const { return msg_; }

private:
    int64_t msg_;
};


struct Wrapper {
    Wrapper(int64_t id) : id(id) {}
    int64_t id;
};

Observer *observer;

void CleanupHandler(void *args) {
    printf("clean up handler work \n");
}

void MainSigHandler(int signal) {
    if (SIGUSR1 == signal) {
        printf("handle this user defined signal \n");
    }
}

//#define SIG_IS_IN_MASK(set, sig) \
//    return *set & (1 << (sig - 1) != 0);

void PrintSigMask() {
    sigset_t sig_set;
    //get current mask
    if (sigprocmask(0, nullptr, &sig_set) == -1)
        perror("get process signal mask error");
    if (sigaddset(&sig_set, SIGUSR1) == -1)
        perror("add signal SIGUSR1 to mask error");
    if (sigismember(&sig_set, SIGUSR1))
        printf("SIGUSR1 in mask");
    if (sigismember(&sig_set, SIGKILL))
        printf("SIGKILL in mask");
    if (sigismember(&sig_set, SIGTERM))
        printf("SIGTERM in mask");

    printf("\n");
}

void ReportToMaster(int status) {
    if (WIFEXITED(status))
        printf("normal termination, exit with the status = %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("terminated by signal = %d%s\n", WTERMSIG(status),
#ifdef WCOREDUMP
               WCOREDUMP(status) ? "(core dump file generated)" : "");
#else
        "");
#endif
    else if (WIFSTOPPED(status))
        printf("stopped by signal = %d\n", WSTOPSIG(status));
}

#define LOG_PATH "./log/berries.log"

void FlushLog() {
    FILE* log_fp;
    if ((log_fp = fopen(LOG_PATH, "a")) == nullptr)
        perror("open lof file error");
    //set offset of log file stream at SEEK_SET
    rewind(log_fp);
    if (fputs("", log_fp) == EOF)
        perror("flush log error");
    if (fclose(log_fp) == EOF)
        perror("close log file stream error");
}

void Log(char* buf) {
    //bad pointer
    FILE* log_fp;
    if ((log_fp = fopen("./log/berries.log", "a")) == nullptr)
        perror("open log file error");
    //set this offset for nothing
    if (lseek(STDOUT_FILENO, 0, SEEK_CUR) == -1)
        perror("lseek error when logging");
    //without buffer
//    if (write(log_fd, buf, sizeof(buf) -1) != sizeof(buf) - 1)
//        perror("write log error");
//    setbuf(log_fp, nullptr);
    if (fputs(buf, log_fp) == EOF)
        perror("write log error");
    if (fclose(log_fp) == EOF)
        perror("close log file stream error");
}

#define MAX_LOG_SIZE 1024

void ForkSlave() {
    int shared_counter = 0;
    int status;
    pid_t pid;
    char buffer[] = "data to be written to buffer\n";
    //line buffer if we print out to terminal
    //but if we redirect to a file it all exist in buffer
    if (write(STDOUT_FILENO, buffer, sizeof(buffer) - 1) != sizeof(buffer) - 1)
        perror("write failed !\n");
    printf("start to fork a slave process\n");
    if ((pid = fork()) < 0) {
       perror("process fork error\n");
    } else if (pid == 0) {
        //slave process
        printf("need buffer \n");
        shared_counter++;
        char log_buf[MAX_LOG_SIZE];
        memset(log_buf, 0, sizeof(log_buf));
        sprintf(log_buf, "I'm a slave PID : %ld, parent PID : %ld\n", (long)getpid(), (long)getppid());
        //show a log
        Log(log_buf);
    } else {
        //parent process
        //wait for slave avoiding it make no sense if the master exit
        sleep(2);
        waitpid(pid, &status, 0);
        ReportToMaster(status);
    }

    printf("in fork experiment, show the result ==> pid = %ld, counter = %d\n", (long)getpid(), shared_counter);
}


void *DoWork(void *args) {
    observer->Pump();
    printf("current state of the counter of worker thread ==> %lld \n", observer->msg());
    if (args) {
        auto wpr = static_cast<Wrapper *>(args);
        printf("wrapper id ==> %lld \nprocess id ==> %d \nparent process id ==> %d \n", wpr->id, getpid(), getppid());
        delete wpr;
    }
    //located at stack to which the main thread access result in core dump
    Wrapper dying_resource(6);
    //allocate from heap so it wont be reclaimed when thread local area released as it exit
    auto heap_dyr = (Wrapper *) malloc(sizeof(Wrapper));
    heap_dyr->id = 5;
    int64_t duplicate = observer->msg();
    //worker thread might be aborted cause main process exit so all its threads will be reclaimed
    //However, the shared pointer is deleted so we can not read correct value from it
    printf("duplicate value ==> %lld \nobserve the heap pointer ==> %x \n", duplicate, heap_dyr);
    pthread_t th_worker = pthread_self();
    printf("thread id ==> %ld \n", th_worker->__sig);
    pthread_cleanup_push(CleanupHandler, nullptr);
//        if (args) return (heap_dyr)
//        if (args) pthread_exit(heap_dyr);
    pthread_cleanup_pop(0);
    //send a signal to main thread when dying
    raise(SIGUSR1);
    pthread_exit(heap_dyr);
}

pthread_t ZygoteWorker() {
    pthread_t worker;
    int err = pthread_create(&worker, nullptr, DoWork, new Wrapper(0));
    if (err) std::cerr << "something wrong with the worker thread constructor" << std::endl;
    return worker;
}

void OnTimeout(int signo) {
    if (SIGALRM == signo)
        printf("timeout at PID : %ld\n", (long)getpid());
}

typedef void SigFunc(int);
typedef void SigFuncInfo(int, siginfo_t*, void*);

void OnTimeoutInfo(int signo, siginfo_t* info, void* context) {
    auto resumed_ctx = static_cast<ucontext_t*>(context);
    printf("catch the crash signal for exception stack \n");
}

//implement of signal function using sigaction
//obtain crash info(sig_info) from crash process and its context

//using extra parameters

//template <typename __SF_type>
//__SF_type* SigLinkToDead(int signo, __SF_type* handler, bool need_info) {
//    struct sigaction act, oact;
//    if (need_info) {
//        act.__sigaction_u.__sa_sigaction = handler;
//        act.sa_flags |= SA_SIGINFO;
//    }
//    sigemptyset(&act.sa_mask);
//    if (signo == SIGALRM) {
//        //for Linux
//#ifdef SA_INTERRUPT
//        act.sa_flags |= SA_INTERRUPT;
//#endif
//    } else {
//        act.sa_flags |= SA_RESTART;
//    }
//    if (sigaction(signo, &act, &oact) < 0)
//        return reinterpret_cast<__SF_type*>(SIG_ERR);
//    return (handler);
//}

//using template meta-programing

template <typename __SF_type>
__SF_type* SigLinkToDead(int signo, __SF_type* handler) {}

//trait
template <>
SigFuncInfo* SigLinkToDead(int signo, SigFuncInfo* handler) {
    struct sigaction act, oact;
    act.__sigaction_u.__sa_sigaction = handler;
    act.sa_flags |= SA_SIGINFO;
    sigemptyset(&act.sa_mask);
    if (signo == SIGALRM) {
        //for Linux
#ifdef SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT;
#endif
    } else {
        act.sa_flags |= SA_RESTART;
    }
    if (sigaction(signo, &act, &oact) < 0)
        return reinterpret_cast<SigFuncInfo*>(SIG_ERR);
    return (handler);
}

template <>
SigFunc* SigLinkToDead(int signo, SigFunc* handler) {
    struct sigaction act, oact;
    act.__sigaction_u.__sa_handler = handler;
    sigemptyset(&act.sa_mask);
    if (signo == SIGALRM) {
        //for Linux
#ifdef SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT;
#endif
    } else {
        act.sa_flags |= SA_RESTART;
    }
    if (sigaction(signo, &act, &oact) < 0)
        return (SIG_ERR);
    return (handler);
}

int main() {
    observer = new Observer;
    uv_loop_t *loop = uv_default_loop();
    uv_run(loop, UV_RUN_ONCE);
    std::string ref = std::string("message");
    observer->update(std::move(ref));
    observer->Pump();
    pthread_t worker = ZygoteWorker();
    if (signal(SIGUSR1, MainSigHandler) == SIG_ERR)
        perror("can not catch SIGUSR1");
//    if (signal(SIGALRM, OnTimeout) == SIG_ERR)
//        perror("can not catch SIGALRM");
    if (SigLinkToDead<SigFuncInfo>(SIGALRM, OnTimeoutInfo) == reinterpret_cast<SigFuncInfo *>(SIG_ERR))
        perror("can not catch SIGALRM");
    alarm(2);
    int counter = 0;
    //block main thread waiting the worker which will be cut down if process exit
    while (counter < 1e4) { counter++; }
    printf("current state of counter of main thread ==> %lld \n", observer->msg());
    void* ptr;
    pthread_join(worker, &ptr);
    printf("see the remaining pointer ==> %x \n", ptr);
    auto wrapper = reinterpret_cast<Wrapper*>(ptr);
    /*
     * will access to bad memory when worker thread has been aborted
     */
    //this slot which the following pointer reference to has been released
    printf("real left value ==>  %lld \n", wrapper->id);
//    pthread_cancel(worker);
    //core dump (access to bad memory)
    delete observer;
    //process fork experiment
    ForkSlave();
    PrintSigMask();
    return 0;
}