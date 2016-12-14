//
// Created by Nuxes Lin on 16/12/14.
//

#ifndef OS_BERRIES_SIGIPC_H
#define OS_BERRIES_SIGIPC_H

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

void SigMasterSlavesHandler(int signo) {
    if (SIGUSR1 == signo) {
        //sent by master, received by slaves
        printf("PID : %d receive form my master\n", getpid());
    } else if (SIGUSR2 == signo) {
        //sent by slaves, received by master
        printf("PID : %d receive form my slaves\n", getpid());
    }
}

#ifdef __SIG_IPC
#endif


#endif //OS_BERRIES_SIGIPC_H
