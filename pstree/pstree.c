#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>

// 进程信息结构体
typedef struct process {
    int pid;
    int ppid;
    char name[128];
    struct process *children;  // 指向第一个子进程
    struct process *sibling;   // 指向下一个兄弟进程
    struct process *next;      // 用于链表管理
} process_t;

// 全局变量
process_t *process_list = NULL;
int show_pids = 0;
int numeric_sort = 0;

// 添加进程到链表
void add_process(int pid, int ppid, const char *name) {
    process_t *proc = malloc(sizeof(process_t));
    if (!proc) return;
    
    proc->pid = pid;
    proc->ppid = ppid;
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->name[sizeof(proc->name) - 1] = '\0';
    proc->children = NULL;
    proc->sibling = NULL;
    proc->next = process_list;
    process_list = proc;
}

// 查找进程
process_t *find_process(int pid) {
    process_t *proc = process_list;
    while (proc) {
        if (proc->pid == pid) {
            return proc;
        }
        proc = proc->next;
    }
    return NULL;
}

// 有序插入子进程
void insert_child_sorted(process_t *parent, process_t *child) {
    if (!numeric_sort || !parent->children || child->pid < parent->children->pid) {
        // 不排序，直接插入到头部
        child->sibling = parent->children;
        parent->children = child;
        return;
    }
    
    // 找到正确的插入位置
    process_t *prev = parent->children;
    while (prev->sibling && prev->sibling->pid < child->pid) {
        prev = prev->sibling;
    }
    
    child->sibling = prev->sibling;
    prev->sibling = child;
}

// 构建进程树
void build_tree() {
    process_t *proc = process_list;
    
    while (proc) {
        if (proc->ppid != 0) {  // 不是根进程
            process_t *parent = find_process(proc->ppid);
            if (parent) {
                // 使用有序插入
                insert_child_sorted(parent, proc);
            }
        }
        proc = proc->next;
    }
}

// 简化的打印函数
void print_tree(process_t *proc, const char *prefix, int is_last) {
    if (!proc) return;
    
    // 打印当前进程
    printf("%s", prefix);
    printf("%s", is_last ? "└── " : "├── ");
    if(show_pids)
        printf("%s[%d]\n", proc->name, proc->pid);
    else
        printf("%s\n", proc->name);
    
    // 为子进程准备新的前缀
    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, is_last ? "    " : "│   ");
    
    // 递归打印所有子进程（已经排序）
    process_t *child = proc->children;
    while (child) {
        process_t *next_sibling = child->sibling;
        print_tree(child, new_prefix, next_sibling == NULL);
        child = next_sibling;
    }
}

// 打印所有进程树
void print_all_trees() {
    process_t *proc = process_list;
    
    // 找到并打印所有根进程（ppid为0或找不到父进程的进程）
    while (proc) {
        if (proc->ppid == 0 || find_process(proc->ppid) == NULL) {
            // 这是一个根进程
            printf("Process tree starting from %s[%d]:\n", proc->name, proc->pid);
            print_tree(proc, "", 1);
            printf("\n");
        }
        proc = proc->next;
    }
}

// 释放内存
void free_processes() {
    process_t *proc = process_list;
    while (proc) {
        process_t *next = proc->next;
        free(proc);
        proc = next;
    }
    process_list = NULL;
}
int is_number(const char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

void traverse_directory(const char *path) {
    DIR *dir;
    struct dirent *entry;
    
    // 打开目录
    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }
    
    // 读取目录项
    while ((entry = readdir(dir)) != NULL) {
        // 只处理名字为数字的entry（进程ID）
        if (is_number(entry->d_name)) {
            char stat_path[512];
            snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);
            FILE *fp = fopen(stat_path, "r");
            if (fp) {
                char line[512];
                if (fgets(line, sizeof(line), fp)) {
                    // 手动解析stat行，因为comm字段可能包含空格
                    int pid, ppid;
                    char state;
                    char *comm_start, *comm_end;
                    
                    // 找到PID
                    if (sscanf(line, "%d", &pid) != 1) {
                        printf("Failed to parse PID from line: %.50s...\n", line);
                        fclose(fp);
                        continue;
                    }
                    
                    // 找到comm字段的开始和结束（被括号包围）
                    comm_start = strchr(line, '(');
                    comm_end = strrchr(line, ')');  // 从右边找，因为可能有嵌套括号
                    
                    if (!comm_start || !comm_end || comm_end <= comm_start) {
                        printf("Failed to parse comm from PID %d\n", pid);
                        fclose(fp);
                        continue;
                    }
                    
                    // 解析comm之后的字段：状态和PPID
                    char *after_comm = comm_end + 1;
                    if (sscanf(after_comm, " %c %d", &state, &ppid) != 2) {
                        printf("Failed to parse state and PPID for PID %d\n", pid);
                        fclose(fp);
                        continue;
                    }
                    
                    // 提取comm字段（包括括号）
                    int comm_len = comm_end - comm_start + 1;
                    char comm[128];
                    strncpy(comm, comm_start, comm_len);
                    comm[comm_len] = '\0';
                    
                    // 添加进程到链表
                    add_process(pid, ppid, comm);
                }
                fclose(fp);
            }
        }
    }
    
    // 关闭目录
    closedir(dir);
}

void printUsage() {
    printf("Usage:\n");
    printf("  pstree [options]\n");
    printf("Options:\n");
    printf("  -p, --show-pids     Show process IDs\n");
    printf("  -n, --numeric-sort  Sort numerically by PID\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
}

int main(int argc, char *argv[]) {
    // Define long options
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"show-pids", no_argument, 0, 'p'},
        {"numeric-sort", no_argument, 0, 'n'},
        {0, 0, 0, 0} // End marker
    };

    int option_index = 0;
    int c;

    // Parse options - 修正参数字符串
    while ((c = getopt_long(argc, argv, "hvpn", long_options, &option_index)) != -1) {
        switch (c) {
        case 'h':
            printUsage();
            return 0;
        case 'v':
            printf("pstree version 1.0\n");
            return 0;
        case 'p':
            show_pids = 1;
            break;
        case 'n':
            numeric_sort = 1;
            break;
        case '?':
            // getopt_long already printed error message
            printUsage();
            return 1;
        default:
            printUsage();
            return 1;
        }
    }

    // Check for extra non-option arguments
    if (optind < argc) {
        printf("Error: Unexpected argument '%s'\n", argv[optind]);
        printUsage();
        return 1;
    }

    // 执行 pstree 功能
    const char *directory = "/proc";
    traverse_directory(directory);
    build_tree();
    print_all_trees();
    // free_processes();

    return 0;
}