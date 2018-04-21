#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

extern char **environ;
int pipe_num;
int redirection_out;
int redirection_in;
char **printEnviron()
{
     return environ;
}
char *printWorkingDirectory()
{
    char wd[4096];
    return getcwd(wd,4096);
}

void redirectionCommand(char **args)
{
    char *name,*name1;
    char *new_args[128];
    int i;
    for(i = 0;strcmp(args[i],">") != 0 && strcmp(args[i],">>") != 0
        && strcmp(args[i],"<") && strcmp(args[0],"<<"); i++)
        new_args[i] = args[i];
    new_args[i + 1] = NULL;
    name = args[i + 1];
    if(redirection_in)name1 = args[i + 3];
    if(redirection_in == 0)
    {
        pid_t pid;
        int fd,fd_temp;
        pid = fork();
        if(pid == 0)
        {
            close(fileno(stdout));
            if(redirection_out == 1)fd = open(name,O_WRONLY |O_TRUNC | O_CREAT,0644);
            else fd = open(name,O_WRONLY | O_APPEND | O_CREAT,0644);
            fd_temp= dup2(fileno(stdout),fd);
            execvp(new_args[0],new_args);
            close(fileno(stdout));
            dup2(fd_temp,fileno(stdout));
        }
        wait(NULL);
    }
}

void pipeCommand(char **args)						//处理管道命令
{
    char *new_args[pipe_num + 1][128];
    int position[pipe_num];

    for(int i = 0,j = 0;; i++)
    {
        if(!args[i])break;
        if(strcmp(args[i],"|") == 0)
        {
            position[j] = i;
            j++;
        }
    }
    int i = 0;

    for(int j = 0; j < pipe_num + 1; j++)				//分割字符串，将命令以|为界限分开
    {
        if(j == 0)
        {
            for(; i < position[0]; i++)new_args[0][i] = args[i];
            new_args[0][i] = NULL;
        }
        else if(j == pipe_num)
        {
            for(; args[i]; i++)new_args[j][i - position[j - 1] - 1] = args[i];
            new_args[j][i] = NULL;
        }
        else
        {
            for(; i < position[j]; i++)new_args[j][i - position[j - 1] - 1] = args[i];
            new_args[j][i] = NULL;
        }
    }
	
    //以下执行管道命令的部分
    //原本想用循环来处理任意多个管道，但总是有错误，所以先交一个比较粗暴的版本（根据管道个数分类）
    //似乎用递归挺简单的

    if(pipe_num == 2)
    {
        pid_t pid1,pid2;
        pid_t pid;
        int pipefd1[2];
        int pipefd2[2];
        pid = fork();
        if(pid == 0)
        {
            pipe(pipefd1);
            pid1 = fork();
            if(pid1 == 0)
            {
               // printf("child process1\n");
                dup2(pipefd1[1],fileno(stdout));
                close(pipefd1[0]);
                close(pipefd1[1]);
               /* if(strcmp(new_args[0][0],"env") == 1)printEnviron();
                else if(strcmp(new_args[0][0],"pwd") == 1)printWorkingDirectory();
                else execvp(new_args[0][0],new_args[0]);*/

                pipe(pipefd2);
                pid2 = fork();
                if(pid2 == 0)								//处理命令的第一部分
                {
                    dup2(pipefd2[1],fileno(stdout));
                    close(pipefd2[0]);
                    close(pipefd2[1]);
                    if(strcmp(new_args[0][0],"env") == 1)printEnviron();
                    else if(strcmp(new_args[0][0],"pwd") == 1)printWorkingDirectory();
                    else execvp(new_args[0][0],new_args[0]);
                }
                else									//处理命令的第二部分
                {
                    waitpid(pid2,NULL,0);
                    dup2(pipefd2[0],fileno(stdin));
                    close(pipefd2[0]);
                    close(pipefd2[1]);
                    execvp(new_args[1][0],new_args[1]);
                }
            }
            else									//处理命令的第三部分
            {
                waitpid(pid1,NULL,0);
                dup2(pipefd1[0],fileno(stdin));
                close(pipefd1[0]);
                close(pipefd1[1]);
                execvp(new_args[2][0],new_args[2]);
            }
        }
        else waitpid(pid,NULL,0);
    }
    if(pipe_num == 1)
    {
        pid_t pid1;
        pid_t pid;
        int pipefd[2];
        pid = fork();
        if(pid == 0)
        {
            pipe(pipefd);
            pid1 = fork();
            if(pid1 == 0)
            {
                dup2(pipefd[1],fileno(stdout));
                close(pipefd[0]);
                close(pipefd[1]);
                if(strcmp(new_args[0][0],"env") == 1)printEnviron();
                else if(strcmp(new_args[0][0],"pwd") == 1)printWorkingDirectory();
                else execvp(new_args[0][0],new_args[0]);
            }
            else
            {
                waitpid(pid1,NULL,0);
                dup2(pipefd[0],fileno(stdin));
                close(pipefd[0]);
                close(pipefd[1]);
                execvp(new_args[1][0],new_args[1]);
            }
        }
        else waitpid(pid,NULL,0);
    }
}


int main()
{
    char cmd[256];
    char *args[128];
    while (1)
    {
        printf("# ");
        fflush(stdin);
        fgets(cmd, 256, stdin);
        int i;
        for (i = 0; cmd[i] != '\n'; i++);
        cmd[i] = '\0';
        args[0] = cmd;
        redirection_out = 0;
        redirection_in = 0;
        pipe_num = 0;
        for(int j = 0; cmd[j]; j++)
            if(cmd[j] == '|')pipe_num++;
        for (i = 0; *args[i]; i++)
            for (args[i + 1] = args[i] + 1; *args[i + 1]; args[i + 1]++)
            {
                int whether_break = 0;
                if (*args[i + 1] == ' ')
                {
                    *args[i + 1] = '\0';
                    args[i + 1]++;
                    for(;*args[i + 1] == ' ';)args[i + 1]++;
                    whether_break = 1;
                }
                if(*args[i + 1] == '>')
                {
                    *args[i + 1] = '\0';
                    if(*(args[i + 1] + 1) == '>')
                    {
                        args[i + 1] += 2;
                        args[i + 2] = args[i + 1];
                        for(;*args[i + 2] == ' ';)args[i + 2]++;
                        args[i + 1] = ">>";
                        i++;
                        redirection_out = 2;
                        whether_break = 1;
                    }
                    else
                    {
                        args[i + 1]++;
                        args[i + 2] = args[i + 1];
                        for(;*args[i + 2] == ' ';)args[i + 2]++;
                        args[i + 1] = ">";
                        i++;
                        redirection_out = 1;
                        whether_break = 1;
                    }

                }
                if(*args[i + 1] == '<')
                {
                    *args[i + 1] = '\0';
                    if(*(args[i + 1] + 1) == '<')
                    {
                        args[i + 1] += 2;
                        args[i + 2] = args[i + 1];
                        for(;*args[i + 2] == ' ';)args[i + 2]++;
                        args[i + 1] = "<<";
                        i++;
                        redirection_in = 2;
                        whether_break = 1;
                    }
                    else
                    {
                        args[i + 1]++;
                        args[i + 2] = args[i + 1];
                        for(;*args[i + 2] == ' ';)args[i + 2]++;
                        args[i + 1] = "<";
                        i++;
                        redirection_in = 1;
                        whether_break = 1;
                    }
                }

                if(*args[i + 1] == '|')
                {
                    *args[i + 1] = '\0';
                    args[i + 1]++;
                    args[i + 2] = args[i + 1];
                    for(;*args[i + 2] == ' ';)args[i + 2]++;
                    args[i + 1] = "|";
                    i++;
                    break;
                }
                if(whether_break)break;
            }
        args[i] = NULL;
        if (!args[0])continue;
        if(pipe_num == 0)
        {
            if(redirection_in == 0 && redirection_out == 0)
            {
                if (strcmp(args[0], "cd") == 0)
                {
                    if (args[1])chdir(args[1]);
                    continue;
                }
                if(strcmp(args[0],"env") == 0)
                {
                    char **result = printEnviron();
                    while(*result)
                    {
                        puts(*result);
                        result++;
                    }
                    continue;
                }
                if(strcmp(args[0],"pwd") == 0)
                {
                    puts(printWorkingDirectory());
                    continue;
                }
                if(strcmp(args[0],"export") == 0)
                {
                    char *name = args[1],*value;
                    for(int j = 0; *args[1]; args[1]++,j++)
                        if(*args[1] == '=')
                        {
                            name[j] = '\0';
                            value = ++args[1];
                        }
                    setenv(name,value,1);
                    continue;
                }
                if (strcmp(args[0], "exit") == 0)return 0;
                pid_t pid = fork();
                if (pid == 0)
                {
                    execvp(args[0], args);

                    return 255;
                }
                wait(NULL);
            }
            else
            {
                redirectionCommand(args);
                continue;
            }
        }
        else
        {
            pipeCommand(args);
            continue;
        }
    }
}

