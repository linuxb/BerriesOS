#include <iostream>
#include <zconf.h>
#include "include/uv.h"
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>
#include <assert.h>
#include <vector>
#include <set>
#include <map>
#include <sys/msg.h>
#include <sys/sem.h>

//#include <assert.h>
#include <aio.h>

class Observer {
public:
    Observer() : msg_(0) {}

    Observer(int64_t msg_);

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
//mask of the master and slaves
static volatile sig_atomic_t sig_mutex;
static sigset_t old_mask, new_mask, mutex_mask;

void SigMasterSlavesHandler(int signo) {
    if (SIGUSR1 == signo) {
        //sent by master, received by slaves
        printf("PID : %d receive form my master\n", getpid());
    } else if (SIGUSR2 == signo) {
        //sent by slaves, received by master
        printf("PID : %d receive form my slaves\n", getpid());
    }
    sig_mutex = 1;
}

//set barrie of this IPC signal avoiding they occurred before process suspend, which prepared for the signal
void TellWait() {
    if (signal(SIGUSR1, SigMasterSlavesHandler) == SIG_ERR)
        perror("can not catch SIGUSR1");
    if (signal(SIGUSR2, SigMasterSlavesHandler) == SIG_ERR)
        perror("can not catch SIGUSR2");
    sigemptyset(&mutex_mask);
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGUSR1);
    sigaddset(&new_mask, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &new_mask, &old_mask) < 0)
        perror("set mask error");
    sig_mutex = 0;
}

void TellMaster(pid_t who) {
    kill(who, SIGUSR2);
}

void TellSlaves(pid_t who) {
    kill(who, SIGUSR1);
}

void WaitMaster() {
    while (sig_mutex == 0)
        sigsuspend(&mutex_mask);
    sig_mutex = 0;

    //reset mask
    if (sigprocmask(SIG_SETMASK, &old_mask, nullptr) < 0)
        perror("reset mask at slaves error");
}

void WaitSlaves() {
    while (sig_mutex == 0)
        sigsuspend(&mutex_mask);
    sig_mutex = 0;

    //reset mask
    if (sigprocmask(SIG_SETMASK, &old_mask, nullptr) < 0)
        perror("reset mask at master error");
}

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
    TellWait();
    if ((pid = fork()) < 0) {
       perror("process fork error\n");
    } else if (pid == 0) {
        //slave process
        printf("need buffer \n");
        WaitMaster();
        shared_counter++;
        TellMaster(getppid());
        char log_buf[MAX_LOG_SIZE];
        memset(log_buf, 0, sizeof(log_buf));
        sprintf(log_buf, "I'm a slave PID : %ld, parent PID : %ld\n", (long)getpid(), (long)getppid());
        //show a log
        Log(log_buf);
    } else {
        //parent process
        //wait for slave avoiding it make no sense if the master exit
        sleep(2);
        //now command the slaves to modify shared area
        printf("now let my slaves modify shared area\n");
        TellSlaves(pid);
        WaitSlaves();
//        assert(shared_counter == 1);
        printf("received from slaves when counter(but in stack on my slaves, need memory shared to see it) = 1\n");
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
    //process is interrupted without handler of this signal
//    raise(SIGUSR1);
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

class AsyncBase {
public:
    inline AsyncBase();
    virtual ~AsyncBase();
//    void ReadAsync(int);
    class Task {
    public:
        virtual void run() = 0;
    };
};

AsyncBase::AsyncBase() {}

AsyncBase::~AsyncBase() {}

typedef std::map<pid_t, volatile sig_atomic_t> __type_mutex_map;
typedef void (*__type_work_func)(void*);
typedef void (*__type_work_cb)(void*);

static __type_mutex_map pool_mutex;

#define MUTEX_ON 0x00
#define MUTEX_OFF 0x01

#define DEFAULT_POOL_SIZE 5

class ProcPool {
public:
    static ProcPool& Self() {
        static ProcPool self;
        return self;
    }

    void Initialize();

    //no copy
    ProcPool(ProcPool const&) = delete;
    void operator=(ProcPool const&) = delete;

    void Schedule(__type_work_func, __type_work_cb, void*, void*);

    inline size_t GetSize() const { return _size; }
    void SetSize(size_t size) { _size = size; }

    inline ~ProcPool();

    enum Status {
        kRunning,   //working
        kSuspended, //idle
    };

    class Task {
    public:
        inline Task(__type_work_func func, __type_work_cb cb, void* args, void* cb_args)
            : func(func),
              callback(cb),
              args(args),
              cb_args(cb_args)
        {}
        __type_work_func func;
        __type_work_cb callback;
        void* args;
        void* cb_args;
    };

    class ProcInfo {
    public:
        pid_t pid;
        sigset_t old_mask, wait_mask;
        void* context = nullptr;
        Status state = kSuspended;
        std::vector<Task> tasks;

        inline ProcInfo(pid_t);

        static void AtExit(ProcInfo&, int);
        //clean up at a detached thread
        static void WrappedCleanup(int);

        void Await();
        void Run();

    };

    typedef std::vector<ProcInfo> __type_procs_;

private:
    inline ProcPool() {}
    void Spawn();
    static void HandleMessage(int);
    void ResumeSlave(pid_t);
    void Massacre();
    void KillASlave(pid_t);

    inline void AddToPool(ProcInfo& proc) {
//        _procs.push_back(proc);
        const_cast<__type_procs_*>(&_procs)->push_back(proc);
    }


    inline void ClearPool() {
//        _procs.clear();
        const_cast<__type_procs_*>(&_procs)->clear();
    }

    size_t _size = DEFAULT_POOL_SIZE;
    volatile std::vector<ProcInfo> _procs;
    sigset_t mask;
};

ProcPool::ProcInfo::ProcInfo(pid_t id) : pid(id) {
    //after fork
    //mask inherited from master's context
    if (sigprocmask(0, nullptr, &old_mask) < 0)
        perror("get old mask from master error");
//    auto temp_pool_mutex = const_cast<__type_mutex_map*>(&pool_mutex);
    pool_mutex.insert({pid, MUTEX_ON});
    //slave will handle all its tasks before exit by accident
    //reinterpret_cast<void(*)(int)>(WrappedCleanup) in vain ^_^
    if (signal(SIGINT, reinterpret_cast<void(*)(int)>(WrappedCleanup)) == SIG_ERR)
        perror("can not catch SIGINT");
//    if (signal(SIGQUIT, AtExit) == SIG_ERR)
//        perror("can not catch SIGQUIT");
}

void ProcPool::ProcInfo::Await() {
    sigemptyset(&wait_mask);
    //atomic operations
    //barrie exists
//    auto temp_pool_mutex = const_cast<__type_mutex_map*>(&pool_mutex);
    auto self_mutex_ref = &(pool_mutex[pid]);
    //compiler will optimize the following code
    //while (true) {}
    while (*self_mutex_ref == MUTEX_ON)
        sigsuspend(&wait_mask);
    *self_mutex_ref = MUTEX_ON;
    //without reset mask
}

void ProcPool::ProcInfo::Run() {
    assert(!tasks.empty());
    //execute
    for (std::vector<Task>::iterator task = tasks.begin(); task != tasks.end(); task++) {
        printf("Slave PID = %d, I'm working\n", getpid());
        task->func(task->args);
        if (task->callback) {
            task->callback(task->cb_args);
        }
    }
}
//this member function can not be passed to signal handler as a pointer
//as compiler will populate a 'this', which is a constant pointer to caller
//this generated code is like that
//X::f__mangling(register const X* this, args...(int)) {}
//but handler requires pointer like that
//void(f*)(int) instead of 'void(X::*f)(const X*, int)'
//so handler must be a static function pointer
void ProcPool::ProcInfo::AtExit(ProcPool::ProcInfo& self, int signo) {
    if (signo == SIGINT || signo == SIGQUIT) {
        if (!self.tasks.empty()) {
            self.Run();
        }
        sleep(2);
        exit(3);
    }
}

void ProcPool::ProcInfo::WrappedCleanup(int signo) {
    //need to get instance of ProcInfo from which is stored at shared memory
    //or is obtained by semaphore
}

//schedule all slaves processes
void ProcPool::Spawn() {
    pid_t pid;
    if ((pid = fork()) < 0)
        perror("fork at processes pool error");
    else if (pid == 0) {
        //slaves process
        ProcPool::ProcInfo info(getpid());
        //register into pool
        //in a duplicated stack of child process, which has no infect in parent process's stack
        AddToPool(info);
        //await until master schedule a work for it
        while (true) {
            info.Await();
            //turn into running state
            info.state = kRunning;
            info.Run();
            sleep(2);
            info.state = kSuspended;
        }
    } else {
        //master
        printf("zygote a slave process its pid = %d\n", pid);
    }
}

void ProcPool::Schedule(__type_work_func func, __type_work_cb cb, void * args, void * cb_args) {
    auto temp_procs = const_cast<__type_procs_*>(&_procs);
    assert(!temp_procs->empty());
    for (std::vector<ProcInfo>::iterator proc = temp_procs->begin(); proc != temp_procs->end(); proc++) {
        if (proc->state == kSuspended) {
            //push into its tasks queue
            ProcPool::Task t(func, cb, args, cb_args);
            //copy constructor
            proc->tasks.push_back(t);
            //await this slave
            ResumeSlave(proc->pid);
            break;
        }
    }
}

void ProcPool::ResumeSlave(pid_t who) {
    //in master
    kill(who, SIGUSR1);
}

void ProcPool::HandleMessage(int signo) {
    //we need to handle SIGUSR1 and SIGUSR2
    pid_t key = getpid();
    //if volatile map
//    auto temp_pool_mutex = const_cast<__type_mutex_map*>(&pool_mutex);
    if (SIGUSR1 == signo) {
        //sent by master, received by slave
        volatile sig_atomic_t& mutex_ref = pool_mutex[key];
        mutex_ref = MUTEX_OFF;
    } else if (SIGUSR2 == signo) {
        //sent by slave, received by master
    }
}

void ProcPool::Initialize() {
    sigset_t new_mask;
    //register signal handler
    if (signal(SIGUSR1, HandleMessage) == SIG_ERR)
        perror("can not catch SIGUSR1");
    if (signal(SIGUSR2, HandleMessage) == SIG_ERR)
        perror("can not catch SIGUSR2");
    //set barrie for IPC signal, or it get lost when it occurred before slaves suspend
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGUSR1);
    sigaddset(&new_mask, SIGUSR2);
    //save current mask
    if (sigprocmask(SIG_BLOCK, &new_mask, &mask) < 0)
        perror("set mask of master error");
    //pre-fork slaves for users
    for (int i = 0; i < _size; i++) {
        Spawn();
    }
    auto temp_procs = const_cast<__type_procs_*>(&_procs);
    assert(temp_procs->size() == _size);
}

void ProcPool::Massacre() {
    auto temp_procs = const_cast<__type_procs_*>(&_procs);
    if (!temp_procs->empty()) {
        for (std::vector<ProcInfo>::iterator proc = temp_procs->begin(); proc != temp_procs->end(); proc++) {
            KillASlave(proc->pid);
        }
    }
}

void ProcPool::KillASlave(pid_t who) {
    kill(who, SIGINT);
    waitpid(who, NULL, 0);

}

ProcPool::~ProcPool() {
    Massacre();
    ClearPool();
//    (const_cast<__type_mutex_map*>(&pool_mutex))->clear();
    pool_mutex.clear();
}

void ThreadSleepWalking(void* msg) {
    //walking thread

}

void CommunicateWithMQ() {

}

//still requires global resource
volatile static int resource = 0;

void LockResWithSemaphore() {
    key_t sem_key = IPC_PRIVATE;
    pid_t pid;
    int sem_id;
    if ((sem_id = semget(sem_key, 1, IPC_CREAT)) < 0)
        perror("acquire semaphore id error");
//    printf("error status: %d\n", errno);
    //define operations to semaphore set
    sembuf ops[] = { { 0, -1, SEM_UNDO } };
    //parent locks resource before forking
    if (semop(sem_id, ops, 1) < 0)
        perror("set ops to semaphore set error");

    //fork a child for preemption
    if ((pid = fork()) < 0) {
        perror("fork child error");
    } else if (pid == 0) {
        //child
        //test the sem of resource, only run before parent locks it,
        //it won't await until its parent release the resource
        //attempts to access to resource
        printf("child will access to resource\n");
        resource++;
        printf("wake up\n");
    } else {
        //parent
        //wait for child
        //parent block, which may result in dead lock
        printf("wait for child\n");
        wait(nullptr);
    }
}

void RunAllThreads() {
    observer = new Observer;
    uv_loop_t *loop = uv_default_loop();
    uv_run(loop, UV_RUN_ONCE);
    std::string ref = std::string("message");
    observer->update(std::move(ref));
    observer->Pump();
    pthread_t worker = ZygoteWorker();
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
}

void MicroTask(void* args) {
    printf("working at PID : %d", getpid());
}

void RunProcessesPool() {
    ProcPool& pool = ProcPool::Self();
    pool.SetSize(5);
    pool.Initialize();
    for (int i = 0; i < 5; i++) {
        pool.Schedule(MicroTask, nullptr, nullptr, nullptr);
    }
}

//    RunAllThreads();
//process fork experiment
//    ForkSlave();
//    PrintSigMask();
//    RunProcessesPool();
int main() {
    LockResWithSemaphore();
    return 0;
}