#include "process.hpp"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <sstream>
#include <stdexcept>

// POSIX
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace simforge::cli::utils
{
    std::string which(const std::string &name)
    {
        const char *path_env = std::getenv("PATH");
        if (!path_env)
            return {};

        std::istringstream iss(path_env);
        std::string token;
        while (std::getline(iss, token, ':'))
        {
            std::filesystem::path candidate = std::filesystem::path(token) / name;
            if (std::filesystem::exists(candidate) && access(candidate.c_str(), X_OK) == 0)
                return candidate.string();
        }
        return {};
    }

    ProcessResult run(const std::string &cmd, const std::vector<std::string> &args, const std::filesystem::path &cwd)
    {
        // build argv
        std::vector<const char *> argv;
        argv.push_back(cmd.c_str());
        for (const auto &a : args)
            argv.push_back(a.c_str());
        argv.push_back(nullptr);

        // pipe pairs
        int stdout_pipe[2], stderr_pipe[2];
        if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0)
            throw std::runtime_error("Failed to create pipes for subprocess");

        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);

        posix_spawn_file_actions_addclose(&fa, stdout_pipe[0]);
        posix_spawn_file_actions_addclose(&fa, stderr_pipe[0]);
        posix_spawn_file_actions_adddup2(&fa, stdout_pipe[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&fa, stderr_pipe[1], STDERR_FILENO);
        posix_spawn_file_actions_addclose(&fa, stdout_pipe[1]);
        posix_spawn_file_actions_addclose(&fa, stderr_pipe[1]);

        posix_spawnattr_t attr;
        posix_spawnattr_init(&attr);

        pid_t pid;
        std::string resolved = which(cmd);
        const char *exe = resolved.empty() ? cmd.c_str() : resolved.c_str();

        pid = fork();
        if (pid < 0)
            throw std::runtime_error("fork() failed");

        if (pid == 0)
        {
            // Child
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stdout_pipe[1]);
            close(stderr_pipe[1]);

            if (chdir(cwd.c_str()) != 0)
                _exit(127);

            execvp(exe, const_cast<char *const *>(argv.data()));
            _exit(127);
        }

        // parent: close write ends
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        posix_spawn_file_actions_destroy(&fa);
        posix_spawnattr_destroy(&attr);

        // read stdout
        std::string out_str, err_str;
        std::array<char, 4096> buf;

        FILE *out_f = fdopen(stdout_pipe[0], "r");
        FILE *err_f = fdopen(stderr_pipe[0], "r");

        while (fgets(buf.data(), buf.size(), out_f))
            out_str += buf.data();

        while (fgets(buf.data(), buf.size(), err_f))
            err_str += buf.data();

        fclose(out_f);
        fclose(err_f);

        int status;
        waitpid(pid, &status, 0);

        return ProcessResult{WIFEXITED(status) ? WEXITSTATUS(status) : -1, std::move(out_str), std::move(err_str)};
    }

    int run_interactive(const std::string &cmd, const std::vector<std::string> &args, const std::filesystem::path &cwd)
    {
        std::vector<const char *> argv;
        argv.push_back(cmd.c_str());
        for (const auto &a : args)
            argv.push_back(a.c_str());
        argv.push_back(nullptr);

        std::string resolved = which(cmd);
        const char *exe = resolved.empty() ? cmd.c_str() : resolved.c_str();

        pid_t pid = fork();
        if (pid < 0)
            throw std::runtime_error("fork() failed");

        if (pid == 0)
        {
            if (chdir(cwd.c_str()) != 0)
                _exit(127);
            execvp(exe, const_cast<char *const *>(argv.data()));
            _exit(127);
        }

        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    bool pid_alive(pid_t pid)
    {
        if (pid <= 0)
            return false;
        if (kill(pid, 0) == 0)
            return true;
        return errno == EPERM;
    }

    pid_t run_detached(const std::string &cmd, const std::vector<std::string> &args, const std::filesystem::path &cwd, const std::filesystem::path &log_file)
    {
        std::vector<const char *> argv;
        argv.push_back(cmd.c_str());
        for (const auto &a : args)
            argv.push_back(a.c_str());
        argv.push_back(nullptr);

        std::string resolved = which(cmd);
        const char *exe = resolved.empty() ? cmd.c_str() : resolved.c_str();

        int pfd[2];
        if (pipe(pfd) != 0)
            throw std::runtime_error("Failed to create pipe for detached process");

        pid_t intermediate = fork();
        if (intermediate < 0)
        {
            close(pfd[0]);
            close(pfd[1]);
            throw std::runtime_error("fork() failed");
        }

        if (intermediate == 0)
        {
            close(pfd[0]);
            setsid();

            pid_t grandchild = fork();
            if (grandchild < 0)
                _exit(127);

            if (grandchild > 0)
            {
                ssize_t ignored = write(pfd[1], &grandchild, sizeof(grandchild));
                (void)ignored;
                close(pfd[1]);
                _exit(0);
            }

            close(pfd[1]);

            if (chdir(cwd.c_str()) != 0)
                _exit(127);

            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0)
            {
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }

            int out = log_file.empty() ? open("/dev/null", O_WRONLY) : open(log_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out >= 0)
            {
                dup2(out, STDOUT_FILENO);
                dup2(out, STDERR_FILENO);
                close(out);
            }

            execvp(exe, const_cast<char *const *>(argv.data()));
            _exit(127);
        }

        close(pfd[1]);
        pid_t launched = -1;
        ssize_t got = read(pfd[0], &launched, sizeof(launched));
        close(pfd[0]);

        int status;
        waitpid(intermediate, &status, 0); // reap the intermediate

        return got == sizeof(launched) ? launched : -1;
    }
} // namespace simforge::cli::utils
