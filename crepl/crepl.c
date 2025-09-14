#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>
#include <signal.h>

// 常量定义
#define MAX_FILENAME_SIZE 256
#define MAX_FUNCTION_NAME_SIZE 64
#define MAX_FUNCTION_DEF_SIZE 256
#define EXPR_WRAPPER_PREFIX "__expr_wrapper_"
#define EXPR_WRAPPER_PREFIX_LEN sizeof(EXPR_WRAPPER_PREFIX) - 1
#define FUNCTION_PREFIX "int "
#define FUNCTION_PREFIX_LEN 4
#define SUCCESS_FLAG 1

// 在子进程中执行表达式函数
void execute_expression_in_child(int write_fd, void* func_ptr) {
    // 重定向stderr到/dev/null以避免污染输出
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }
    
    // 尝试执行函数
    int result;
    char success = SUCCESS_FLAG;
    
    // 这里可能因为undefined symbol而失败
    int (*expr_func)(void) = (int (*)(void))func_ptr;
    result = expr_func();
    
    // 如果执行到这里，说明函数调用成功
    write(write_fd, &success, 1);  // 写入成功标志
    write(write_fd, &result, sizeof(int));  // 写入结果
    close(write_fd);
    exit(0);
}

// 在父进程中处理表达式执行结果
bool handle_expression_result(int read_fd, int* expr_result, void* handle) {
    // 尝试从管道读取结果
    char success;
    ssize_t bytes_read = read(read_fd, &success, 1);
    
    if (bytes_read == 1 && success == SUCCESS_FLAG) {
        // 子进程成功执行，读取结果
        int result;
        if (read(read_fd, &result, sizeof(int)) == sizeof(int)) {
            *expr_result = result;
            printf("Expression result: %d\n", *expr_result);
            close(read_fd);
            return true;
        } else {
            printf("Error: Failed to read expression result\n");
            close(read_fd);
            dlclose(handle);
            return false;
        }
    } else {
        // 子进程失败（可能是undefined symbol）
        printf("Error: Expression evaluation failed (likely undefined symbol or runtime error)\n");
        close(read_fd);
        dlclose(handle);
        return false;
    }
}
int create_temp_c_file(const char* c_code, char* filename_out) {
    char temp_filename[] = "/tmp/crepl_XXXXXX.c";
    
    // 注意：mkstemp 需要修改template，所以创建副本
    char template[] = "/tmp/crepl_XXXXXX";
    int fd = mkstemp(template);
    if (fd == -1) {
        return -1;
    }
    
    // 重命名为.c文件
    snprintf(temp_filename, sizeof(temp_filename), "%s.c", template);
    if (rename(template, temp_filename) != 0) {
        close(fd);
        unlink(template);
        return -1;
    }
    
    // 重新打开.c文件
    close(fd);
    FILE* file = fopen(temp_filename, "w");
    if (!file) {
        unlink(temp_filename);
        return -1;
    }

    fprintf(file, "%s\n", c_code);
    fclose(file);
    
    // 返回文件名
    strcpy(filename_out, temp_filename);
    return 0;
}

bool get_so_filename(const char* c_filename, char* so_filename) {
    // 生成共享库文件名
    strcpy(so_filename, c_filename);
    char *dot = strrchr(so_filename, '.');
    if (dot) {
        strcpy(dot, ".so");  // 替换.c为.so
        return true;
    } else {
        return false;
    }
}

// Compile a function definition and load it
bool compile_and_load_function_and_alter(const char* function_def, int *expr_result) {
    char c_filename[MAX_FILENAME_SIZE];
    char so_filename[MAX_FILENAME_SIZE];
    
    // 创建临时C文件
    if (create_temp_c_file(function_def, c_filename) != 0) {
        printf("Error creating temporary C file\n");
        return false;
    }
    printf("Created C file: %s\n", c_filename);
    
    get_so_filename(c_filename, so_filename);
    
    // 使用fork + exec编译成共享库
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程：编译成共享库
        execlp("gcc", "gcc", "-shared", "-fPIC", "-Wno-implicit-function-declaration", "-o", so_filename, c_filename, (char*)NULL);
        perror("exec gcc failed");
        exit(1);
    } else if (pid > 0) {
        // 父进程：等待编译完成
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Compilation successful! Shared library: %s\n", so_filename);
            
            // 动态加载共享库
            void* handle = dlopen(so_filename, RTLD_LAZY | RTLD_GLOBAL);
            if (!handle) {
                printf("Cannot load shared library: %s\n", dlerror());
                unlink(c_filename);
                unlink(so_filename);
                return false;
            }
            
            // 提取函数名
            char func_name[MAX_FUNCTION_NAME_SIZE] = "";
            sscanf(function_def, "%*s %63[^(]", func_name);
            
            if (strlen(func_name) > 0) {
                // 获取函数指针
                void* func_ptr = dlsym(handle, func_name);
                if (func_ptr) {
                    printf("Function '%s' loaded successfully at address %p\n", func_name, func_ptr);
                    
                    if(strncmp(func_name, EXPR_WRAPPER_PREFIX, EXPR_WRAPPER_PREFIX_LEN) == 0) {
                        // 创建管道用于父子进程通信
                        int pipefd[2];
                        if (pipe(pipefd) == -1) {
                            perror("pipe failed");
                            dlclose(handle);
                            return false;
                        }

                        // 在子进程中调用表达式函数，避免主进程崩溃
                        pid_t eval_pid = fork();
                        if (eval_pid == 0) {
                            // 子进程：关闭读端
                            close(pipefd[0]);
                            execute_expression_in_child(pipefd[1], func_ptr);
                        } else if (eval_pid > 0) {
                            // 父进程：关闭写端
                            close(pipefd[1]);
                            
                            // 等待子进程完成
                            int status;
                            waitpid(eval_pid, &status, 0);
                            
                            // 处理执行结果
                            return handle_expression_result(pipefd[0], expr_result, handle);
                        } else {
                            close(pipefd[0]);
                            close(pipefd[1]);
                            perror("fork failed for expression evaluation");
                            dlclose(handle);
                            return false;
                        }
                    }
                    // 注意：这里我们没有关闭库，函数在程序运行期间保持可用
                    
                } else {
                    printf("Cannot find function '%s': %s\n", func_name, dlerror());
                    dlclose(handle);
                }
            }
            
            // 清理临时文件
            unlink(c_filename);
            unlink(so_filename);
            
            return true;
        } else {
            printf("Compilation failed with exit code: %d\n", WEXITSTATUS(status));
            unlink(c_filename);
            return false;
        }
    } else {
        perror("fork failed");
        unlink(c_filename);
        return false;
    }
}

bool compile_and_load_function(const char* function_def) {
    return compile_and_load_function_and_alter(function_def, NULL); 
}

// Evaluate an expression
bool evaluate_expression(const char* expression, int* result) {
    static int wrapper_counter = 0;  // 静态计数器
    char function_def_from_expression[MAX_FUNCTION_DEF_SIZE];
    
    // 生成带数字后缀的函数名
    snprintf(function_def_from_expression, sizeof(function_def_from_expression), 
             "int " EXPR_WRAPPER_PREFIX "%d() { return %s;}", 
             wrapper_counter++, expression);
    
    return compile_and_load_function_and_alter(function_def_from_expression, result);
}

int main() {
    // 如果设置了测试环境变量，直接退出让 TestKit 运行测试
    if (getenv("TK_RUN")) {
        return 0;  // 正常退出，让 TestKit 接管
    }
    
    // 启用历史记录
    using_history();
    read_history(".tmp_history");
    
    printf("Enhanced readline demo. Type 'exit' to quit.\n");
    printf("Features: Use ↑↓ for history, Tab for completion, Ctrl+R for search\n\n");
    
    while (1) {
        char *line = readline("crepl> ");
        if (!line) break;
        
        // 如果输入为空，跳过
        if (strlen(line) == 0) {
            free(line);
            continue;
        }
        
        // 添加到历史记录
        add_history(line);
        
        // 处理特殊命令
        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        } else if (strcmp(line, "help") == 0) {
            printf("Available commands: read, write, help, exit\n");
        } else {
            // 检查是否以特定前缀开头
            if (strncmp(line, FUNCTION_PREFIX, FUNCTION_PREFIX_LEN) == 0) {
                // 处理函数定义
                if (compile_and_load_function(line)) {
                    printf("Function compiled successfully!\n");
                } else {
                    printf("Error compiling function\n");
                }
            } else {
                // 尝试作为表达式求值
                int result;
                if (evaluate_expression(line, &result)) {
                    printf("Result: %d\n", result);
                } else {
                    printf("Error evaluating expression\n");
                }
            }
        }
        
        free(line);
    }
    
    // 保存历史记录
    write_history(".tmp_history");
    
    return 0;
}
