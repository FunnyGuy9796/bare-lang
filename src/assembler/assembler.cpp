#include "assembler.h"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>

int Assembler::run(const vector<string> &args, string &err_output) {
    int pipe_fd[2];

    pipe(pipe_fd);

    pid_t pid = fork();

    if (pid == 0) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);

        vector<const char *> argv;

        for (const auto &a : args)
            argv.push_back(a.c_str());

        argv.push_back(nullptr);

        execvp(argv[0], const_cast<char *const *>(argv.data()));

        cerr << "could not execute '" << args[0] << "': " << strerror(errno) << endl;

        exit(1);
    }
    
    close(pipe_fd[1]);

    char buf[256];
    ssize_t n;

    while ((n = read(pipe_fd[0], buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        err_output += buf;
    }

    close(pipe_fd[0]);

    int status;

    waitpid(pid, &status, 0);

    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

bool Assembler::assemble(const string &asm_file, const string &obj_file, string &err) {
    int ret = run({"nasm", "-f", "elf32", asm_file, "-o", obj_file}, err);

    return ret == 0;
}

bool Assembler::link(const string &obj_file, const string &out_file, string &err) {
    int ret = run({"ld", "-m", "elf_i386", "-o", out_file, obj_file}, err);

    return ret == 0;
}