#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "svec.h"
#include "tokenize.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>



int
semi_op(svec* toRun);

int
change_directory(svec* change);

int
redirect_left(svec* toLeft);

int
redirect_right(svec* toRight);

int
sleep_operator(svec* sleep_args);

int
or_operator(svec* toOr);

int
and_operator(svec* toAnd);

int
pipe_this(svec* toPipe);

int
execute_ops(svec* input);

int
check_for(svec* toCheck, char* checkFor);

int
fork_and_exec(svec* toExec);

int
semi_op(svec* toRun) {

    int size = toRun->size;

    svec* left = make_svec();
    svec* right = make_svec();

    int ii = 0;
    while (size != ii + 1 && (strcmp(toRun->data[ii], ";")) != 0) {
        svec_push_back(left, svec_get(toRun, ii));
        ++ii;
    }

    ++ii;
    for (; ii < size; ++ii) {
        svec_push_back(right, svec_get(toRun, ii));

    }

    execute_ops(left);
    execute_ops(right);
    free_svec(left);
    free_svec(right);
    return 0;
}

int
change_directory(svec* change) {

    if (check_for(change, "exit") == 0
        || check_for(change, "exit\n") == 0) {
        _Exit(0);
    }

    char strbuff[100];
    getcwd(strbuff, 100);
    strcat(strbuff, "/");
    strcat(strbuff, change->data[1]);
    return chdir(strbuff);


}
int
redirect_left(svec* toLeft) {

    if (check_for(toLeft, "exit") == 0
        || check_for(toLeft, "exit\n") == 0) {
        _Exit(0);
    }

    int size = toLeft->size;

    svec* left = make_svec();
    svec* right = make_svec();

    int ii = 0;
    while (size != ii + 1 && (strcmp(toLeft->data[ii], "<")) != 0) {
        svec_push_back(left, svec_get(toLeft, ii));
        ++ii;
    }

    ++ii;
    for (; ii < size; ++ii) {
        svec_push_back(right, svec_get(toLeft, ii));

    }


    int cpid;
    if ((cpid = fork())) {
        int s;
        waitpid(cpid, &s, 0);
    }
    else {
        close(0);
        FILE* toRead = fopen(right->data[0], "r");
        dup(fileno(toRead));
        execvp(left->data[0], left->data);
    }

    free_svec(left);
    free_svec(right);
    return 0;
}

// you put the result of the execution side, put it in pipe write, then in parent you read from the pipe
// and put it in the file that was on the other side of the redirection operator
int
redirect_right(svec* toRight) {

    if (check_for(toRight, "exit") == 0
        || check_for(toRight, "exit\n") == 0) {
        _Exit(0);
    }

    int size = toRight->size;

    svec* left = make_svec();
    svec* right = make_svec();

    int ii = 0;

    while (size != ii + 1 && (strcmp((svec_get(toRight, ii)), ">")) != 0) {
        svec_push_back(left, svec_get(toRight, ii));
        ++ii;
    }

    ++ii;

    for (; ii < size; ++ii) {
        svec_push_back(right, svec_get(toRight, ii));

    }

    int rv;
    int pipe_fds[2];
    rv = pipe(pipe_fds);
    assert(rv != -1);

    int cpid;

    if ((cpid = fork())) {

        char toFile[256];
        int x = read(pipe_fds[0], toFile, 255);
        toFile[x] = 0; // null terminator
        FILE* fromLeft = fopen(right->data[0], "w");
        fprintf(fromLeft, toFile);
        fclose(fromLeft);

    }
    else {
        //left pipe
        close(pipe_fds[0]);
        close(1);
        dup(pipe_fds[1]);
        close(pipe_fds[1]);

        execvp(left->data[0], left->data);
    }

    free_svec(left);
    free_svec(right);
    return 0;
}


int
sleep_operator(svec* sleep_args) {

    if (check_for(sleep_args, "exit") == 0
        || check_for(sleep_args, "exit\n") == 0) {
        _Exit(0);
    }

    int cpid;

    if ((cpid = fork())) {

    }

    else {
        execvp(sleep_args->data[0], sleep_args->data);
    }
    return 0;
}

int
or_operator(svec* toOr) {



    int size = toOr->size;

    svec* left = make_svec();
    svec* right = make_svec();

    int ii = 0;

    for (; ii < size && strcmp(toOr->data[ii], "||") != 0; ++ii) {
        svec_push_back(left, svec_get(toOr, ii));

    }

    ++ii;

    for (; ii < size; ++ii) {
        svec_push_back(right, svec_get(toOr, ii));

    }

    int rv = fork_and_exec(left);
    if (rv != 0) {
        fork_and_exec(right);
        return 0;
    }


    free_svec(left);
    free_svec(right);

}

// right side of && will only be evaluated if the exit status of the left side is zero
int
and_operator(svec* toAnd) {
    int size = toAnd->size;

    svec* left = make_svec();
    svec* right = make_svec();

    int ii = 0;

    while (size != ii + 1 && (strcmp(toAnd->data[ii], "&&")) != 0) {
        svec_push_back(left, svec_get(toAnd, ii));
        ++ii;

    }

    ++ii;

    for (; ii < size; ++ii) {
        svec_push_back(right, svec_get(toAnd, ii));

    }

    int rv = fork_and_exec(left);
    if (rv == 0) {
        fork_and_exec(right);
        return 0;
    }


    free_svec(left);
    free_svec(right);
}

// Some code and inspiration taken from Nat's lecture notes: 08-processes/sort-pipe/sort-pipe.c
int
pipe_this(svec* toPipe)
{
    if (check_for(toPipe, "exit") == 0
        || check_for(toPipe, "exit\n") == 0) {
        _Exit(0);
    }

    int size = toPipe->size;

    svec* left = make_svec();
    svec* right = make_svec();

    int ii = 0;

    while (size != ii + 1 && (strcmp(toPipe->data[ii], "|")) != 0) {
        svec_push_back(left, svec_get(toPipe, ii));
        ++ii;
    }

    ++ii;

    for (; ii < size; ++ii) {
        svec_push_back(right, svec_get(toPipe, ii));

    }

    int rv;
    int pipe_fds[2];
    rv = pipe(pipe_fds);
    assert(rv != -1);

    int cpid;
    int other_cpid;

    // pipes[0] is for reading
    // pipes[1] is for writing

    if ((cpid = fork())) {
        // parent
        assert(rv != -1);
        int s;
        waitpid(cpid, &s, 0);
    }
    else {
        // child


        if ((other_cpid = fork())) {
            // right pipe

            close(pipe_fds[1]); // only reading in the parent
            char toRead[256];

            int x = read(pipe_fds[0], toRead, 255);
            toRead[x] = 0; // null terminator

            FILE* returnedFromLeft = fopen("/tmp/pipeop.txt", "w");

            fprintf(returnedFromLeft, toRead);
            fclose(returnedFromLeft);

            svec_push_back(right, "/tmp/pipeop.txt");
            execute_ops(right);


        }
        else {
            //left pipe
            close(pipe_fds[0]);
            close(1);
            dup(pipe_fds[1]);
            close(pipe_fds[1]);

            execute_ops(left);

        }

    }


    free_svec(left);
    free_svec(right);
    return 0;


}

int
fork_and_exec(svec* toExec) {

    if (check_for(toExec, "exit") == 0) {
        _Exit(0);
    }

    if (check_for(toExec, "exit\n") == 0) {
        _Exit(0);
    }

    int cpid;
    int toReturn = 0;
    if ((cpid = fork())) {
        int status;
        waitpid(cpid, &status, 0);
        if (status == 0) {
            return toReturn;
        }
        else {
            toReturn = 1;
            return toReturn;
        }
    }

    else {
        execvp(toExec->data[0], toExec->data);
    }


}

// 0 is true, 1 is false
int
check_for(svec* toCheck, char* checkFor) {


    int toReturn = 1;
    int nn = toCheck->size;
    int ii = 0;
    for (; ii < nn; ++ii) {
        if (strcmp(toCheck->data[ii], checkFor) == 0) {
            toReturn = 0;
        }
    }

    return toReturn;
}

int
execute_ops(svec* input) {


    if (check_for(input, "&") == 0) {
        sleep_operator(input);
    }

    else if(check_for(input, ";") == 0) {
        semi_op(input);
    }

    else if (check_for(input, "|") == 0) {
        pipe_this(input);
    }
    else if (check_for(input, "&&") == 0) {
        and_operator(input);
    }

    else if (check_for(input, "||") == 0) {
        or_operator(input);
    }

    else if (check_for(input, ">") == 0) {
        redirect_right(input);
    }
    else if(check_for(input, "<") == 0) {
        redirect_left(input);
    }
    else if(check_for(input, "cd") == 0) {
        change_directory(input);
    }

    else {
        // normal command
        // fork and exec
        fork_and_exec(input);
    }

    return 0;
}
int
main(int argc, char* argv[])
{
    char cmd[256];
    char* eof = "";
    FILE* script;
    svec* input;
    //
    if (argc > 1) {
        script = fopen(argv[1], "r");
        fflush(stdout);
        eof = fgets(cmd, 255, script);
    }
    // script
    while (eof != NULL) {
        if (argc == 1) {
            printf("nush$ ");
            fflush(stdout);
            eof = fgets(cmd, 255, stdin);
            input = tokenize(cmd);
            execute_ops(input);
            free_svec(input);


        }
        else {
            input = tokenize(cmd);
            execute_ops(input);
            free_svec(input);
            eof = fgets(cmd, 255, script);
        }

    }
    if (argc > 1) {
        fclose(script);
    }
    return 0;
}





