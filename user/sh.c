#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

#define HISTFILE ".mosh_history"
int history_changed = 1;

void list_history() {
	long n;
	char buf[2048];
	int fdnum = open(HISTFILE, O_RDONLY);
	while ((n = read(fdnum, buf, (long)sizeof buf)) > 0) {
		if (write(1, buf, n) != n)
			user_panic("write error copying");
	}
}

void save_history(char* cmd) {
	int r;
    int fd = open(HISTFILE, O_CREAT | O_WRONLY | O_APPEND);
    if ((r=fd)< 0) {
        debugf("open .mosh_history failed!\n");
        return;
    }
    write(fd, cmd, strlen(cmd));
    write(fd, "\n", 1);
    history_changed = 1; //历史更新
}

int get_history(int back, char *buf) {
	static struct Fd *fd;
    int fd_num = 0;
    static char * va;
    static int offset = 0;
    static int beginva;
    static int endva;
    char* p;
    
    if (history_changed) {
        fd_num = open(HISTFILE, O_RDONLY);
        fd = num2fd(fd_num);
        beginva = fd2data(fd); //文件的起始地址
        endva = beginva + ((struct Filefd*)fd)->f_file.f_size ; //结束地址
        va = endva - 1; //初始化va为指向文件的最后一个字节
        offset = 0;
		history_changed = 0;
    }

    if(back) {
        while(*va == 0 || *va == '\n') 
			va--;
        while(*va != '\n' && va > beginva) 
			va--;
        int i = 0;
        p = va + ((*va == '\n' ? 1 : 0));
        for(; *p != '\n' && *p != '\0';p++) {
            buf[i++] = *p;
        }
        buf[i] = 0;
        offset--;
        return i;
    }else {
        while(*va == '\n' && va < endva) 
			va++;
        while(*va != '\n' && va < endva) 
			va++;
        p = va + 1;
        if(p >= endva){
            *buf = 0;
            return 0;
        }
        else{
            int i = 0;
            while(*p != '\n') {
                buf[i++] = *(p++);
            }
            buf[i] = 0;
            return i;
        }
    }
}

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */

//读端是p[0]，写端是p[1]
void run_backquoted_command(char *cmd) {
    int p[2];
    pipe(p);
	int child=fork();
    if (child == 0) {
        dup(1, p[1]);  // 将标准输出重定向到管道的写端
		close(p[0]);  
		close(p[1]);
		runcmd(cmd);  
        exit(0);
    } else {
        close(p[1]);
        int n;
        char output[1024] = {0};  // 假设输出不超过1024字节
        n = read(p[0], output, sizeof(output));  // 从管道读取输出
        output[n] = '\0';
        wait(child);  // 等待子进程结束
        close(p[0]);
        // 替换原命令行中的反引号部分
        strcpy(cmd, output);
    }
}

int _gettoken(char *s, char **p1, char **p2) {
	*p1 = 0;  // 初始化指向令牌开始的指针为 0
	*p2 = 0;  // 初始化指向令牌结束的指针为 0
	if (s == 0) {
		return 0;  // 如果输入字符串为空，返回 0
	}
	// 跳过所有空白符并将它们设为 0
	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		return 0;  // 如果到达字符串结尾，返回 0
	}

	if (*s == '\"') {
		*s = 0;
		*p1 = ++s;
		while (*s && (*s != '\"')) {
			s++;
		}
		*s++ = 0;
		*p2 = s;
		return 'w';
    }
	
	// 检查是否为特殊符号并处理
	if (strchr(SYMBOLS, *s)) {
		 if (*s == '&' && *(s + 1) == '&') {  // 检查是否为 &&
            *p1 = s;
            *s = 0;  // 将 & 置为 0
            s += 2;  // 跳过 &&
            *p2 = s;
            return 'a';  // 返回 'a' 表示 &&
        } else if (*s == '|' && *(s + 1) == '|') {  // 检查是否为 ||
            *p1 = s;
            *s = 0;  // 将 | 置为 0
            s += 2;  // 跳过 ||
            *p2 = s;
            return 'o';  // 返回 'o' 表示 ||
        }else{
			int t = *s;  // 保存特殊符号
			*p1 = s; 
			*s++ = 0; 
			*p2 = s;  
			return t;  // 返回特殊符号作为令牌类型
		}
	}
	// 处理普通单词
	*p1 = s;  // 设置 p1 指向单词开始位置
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
		s++;  // 移动到单词结尾或遇到下一个空白符/特殊符号
	}
	*p2 = s;  // 设置 p2 指向单词结尾
	return 'w';  // 返回 'w' 表示普通单词
}

int gettoken(char *s, char **p1) {
	static int c, nc;  // 保存当前和下一个令牌类型
	static char *np1, *np2;  // 保存当前和下一个令牌的开始和结束位置

	if (s) {
		nc = _gettoken(s, &np1, &np2);  // 如果提供新字符串，解析第一个令牌
		return 0;  // 初始化完成，返回 0
	}
	c = nc;  // 设置当前令牌类型
	*p1 = np1;  // 设置 p1 指向当前令牌的开始位置
	nc = _gettoken(np2, &np1, &np2);  // 解析下一个令牌并更新静态变量
	return c;  // 返回当前令牌类型
}

#define MAXARGS 128

int execute = 1;  // 决定是否执行命令的标志
int last_return_code = 0;  // 跟踪上一个命令的返回代码

//文件描述符0是标注输入，1是标准输出
// 解析命令
int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;
	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		int re_alloc=0;
		if(c==0) return argc;
		if(execute==0){
			if(c=='a'&&last_return_code==0) execute=1;
			if(c=='o'&&last_return_code!=0) execute=1;
			continue;
		}
		
		switch (c) {
		case 0:
			return argc;
		case 'w':
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit(1);
			}
			argv[argc++] = t;
			break;
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit(1);
			}
			// Open 't' for reading, dup it onto fd 0, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (1/3) */
			if((r = open(t, O_RDONLY))<0){
				debugf("open error %d\n",r);
				exit(1);
			}
			fd=r;
			if ((r = dup(fd, 0)) < 0) {
				debugf("dup error: %d\n", r);
				close(fd);
			}
			
			//user_panic("< redirection not implemented");

			break;
		case '>':
				r= gettoken(0, &t);
			if (r == '>') {
				if (gettoken(0, &t) != 'w') {
					debugf("syntax error: > not followed by word\n");
					exit(0);
				}
				
				fd = open(t, O_WRONLY | O_APPEND | O_CREAT);
				if (fd < 0) {
					user_panic(">> redirection not implemented");
				}
				if ((r = dup(fd, 1)) < 0) {
					debugf("dup error: %d\n", r);
					close(fd);
				}
				re_alloc=1;
				break;
			}  
			if (r != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit(1);
			}
			// Open 't' for writing, create it if not exist and trunc it if exist, dup
			// it onto fd 1, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (2/3) */
			if ((r = open(t, O_WRONLY | O_CREAT | O_TRUNC)) < 0) {
				debugf("open error: %d\n", r);
				exit(1);
			}
			int fd = r;
			if ((r = dup(fd, 1)) < 0) {
				debugf("dup error: %d\n", r);
				close(fd);
			}
			re_alloc=1;
			break;
		case '|':;
			
			/*
			 * First, allocate a pipe.
			 * Then fork, set '*rightpipe' to the returned child envid or zero.
			 * The child runs the right side of the pipe:
			 * - dup the read end of the pipe onto 0
			 * - close the read end of the pipe
			 * - close the write end of the pipe
			 * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest of the
			 *   command line.
			 * The parent runs the left side of the pipe:
			 * - dup the write end of the pipe onto 1
			 * - close the write end of the pipe
			 * - close the read end of the pipe
			 * - and 'return argc', to execute the left of the pipeline.
			 */
			int p[2];
			/* Exercise 6.5: Your code here. (3/3) */
			pipe(p);//分配一个管道
			*rightpipe=fork();
			if(*rightpipe==0){//代表子进程
				dup(p[0],0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe);//返回|右侧命令行
			}else if(*rightpipe>0){
				dup(p[1],1);
				close(p[1]);
				close(p[0]);
				return argc;//代表父进程，返回|左侧命令行
			}
			
			//user_panic("| not implemented");
			break;
		case ';':;
			int child = fork(); // fork出两个进程
			if(child) { // 父进程等待子进程执行完再执行';'右边的命令
				// 如果前一条命令出现了重定向，那么再重定向回来
				if(re_alloc == 1){ 
					dup(1, 0);
					re_alloc=0;
				} else if(re_alloc == 0) {
					dup(0, 1);
				}		
				wait(child);
				return parsecmd(argv, rightpipe);
			} else { // 子进程执行';'左边的命令
				return argc;
			}
			break;
        case 'a': // AND
			debugf("read &&\n");
            r=fork();
			if(r){
				last_return_code=wait(r);
				debugf("last_return_code:%d\n",last_return_code);
				if(last_return_code!=0){
					execute=0;
				}
				return parsecmd(argv,rightpipe);
			}else{
				return argc;
			}
			break;
        case 'o': // OR
			r=fork();

			if(r){//如果是父进程
				last_return_code=wait(r);
				if(last_return_code==0){
					execute=0;
				}
				return parsecmd(argv,rightpipe);
			}else {
				return argc;
			}
			break;
		}
		
	}

	return argc;
}


int str_to_int(const char *str) {
 
    int result = 0;
    int sign = 1;  // 用于处理负号
    const char *p = str;

    // 跳过前导空格
    while (*p == ' ') {
        p++;
    }

    // 处理负号
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    // 遍历每个字符
    while (*p != '\0') {
        // 累加结果
        result = result * 10 + (*p - '0');
        p++;
    }
    return sign * result;
}

// 执行命令函数
void runcmd(char *s) {
	char *bq_start, *bq_end, *dq_start, *dq_end;
    char *cursor = s;
    int in_double_quotes = 0;
    // 解析整个命令字符串
    while (*cursor) {
        if (*cursor == '\"') {
            if (!in_double_quotes) {
                // 进入双引号区域
                dq_start = cursor;
                in_double_quotes = 1;
            } else {
                // 离开双引号区域
                dq_end = cursor;
                in_double_quotes = 0;
            }
        } else if (*cursor == '`' && !in_double_quotes) {
            // 只在不在双引号内时处理反引号
            bq_start = cursor;
            bq_end = strchr(bq_start + 1, '`');
            if (!bq_end) {
                debugf("Syntax error: Unmatched `.\n");
                return;
            }
            *bq_end = '\0';  // 结束反引号内命令的字符串
            char temp_cmd[2048];
            strcpy(temp_cmd, bq_start + 1);  // 复制反引号内的命令
            run_backquoted_command(temp_cmd);  // 执行反引号内的命令并替换
            *bq_start = '\0';  // 结束原命令的字符串
            strcat(s, temp_cmd);  // 将替换后的输出添加回原命令字符串
            strcat(s, bq_end + 1);  // 将反引号后的部分添加回来
            cursor = bq_end + 1;  // 更新cursor位置
            continue;
        }
        cursor++;
    }

    // 初始化解析
    gettoken(s, 0); // 初始化 np1 和 np2
    // 定义命令行参数数组

	char *argv[MAXARGS];
	int rightpipe = 0; // 用于标记是否有管道
	int argc = parsecmd(argv, &rightpipe); // 解析命令行参数，填充 argv，判断是否有管道
	if (argc == 0) {
		return; // 如果没有解析到任何命令行参数，直接返回
	}
	argv[argc] = 0; // 在参数数组的末尾添加一个空指针，标记参数结束
	if(strcmp(argv[0],"history") == 0){
		list_history();
		exit(0);
	}else if (strcmp(argv[0], "kill") == 0) {
	
		syscall_killjob(str_to_int(argv[1]));
		return;
	} else if (strcmp(argv[0], "jobs") == 0) {
		syscall_printjob();
		return;
	}else if (strcmp(argv[0], "fg") == 0) {
		int jobid=str_to_int(argv[1]);
		int envid = syscall_fgjob(jobid);
		if (envid > 0) {
			wait(envid);
		}
		return;
	}
	// 创建子进程来执行命令

	int child = spawn(argv[0], argv); // 调用 spawn 函数，使用 argv[0] 作为命令，argv 作为参数
	debugf("%x spawn %x\n",env->env_id,child); // 打印调试信息，显示当前环境 ID 和子进程 ID

	// 关闭所有文件描述符，防止资源泄露
	close_all();
	if (child >= 0) {
		last_return_code=wait(child); // 等待命令执行完成
	} else {
		debugf("spawn %s: %d\n", argv[0], child); // 如果失败
	}
	// 如果存在管道，等待右侧管道的进程完成
	if (rightpipe) {
		wait(rightpipe); // 等待右侧管道的进程
	}
	exit(last_return_code); // 退出当前进程
}

//0和1号文件描述符（fd），是进程的标准输入和输出

void readline(char *buf, u_int n) {
	int r;
	int maxlen=n;
	for (int i = 0; i < n; i++) {
		if ((r = read(0, buf + i, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit(1);
		}
		if (buf[i] == '\b' || buf[i] == 0x7f) {
			if (i > 0) {
				i -= 2;
			} else {
				i = -1;
			}
			if (buf[i] != '\b') {
				printf("\b");
			}
		}
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;
			return;
		}
		if (buf[i] == '#') {
			buf[i] = 0;
			for(int j=i+1;(r = read(0, buf+j, 1)) == 1 && buf[j] != '\r' && buf[j] != '\n';j++);
			printf("this is buf:%s\n",buf);
			return;
		}
		if(buf[i] == '\x1b') {
    		read(0, buf + i, 1); 
            read(0, buf + i, 1);
			//向上箭头
    		if (buf[i] == 'A') {
        		if (i)
            		printf("\x1b[1B\x1b[%dD\x1b[K", i);
        		else
            		printf("\x1b[1B");
       		 	int history_len = get_history(1, buf);
        		printf("%s", buf);
				n = maxlen;
				i = history_len - 1;
   			}else if (buf[i] == 'B'){ //向下
        		if (i)printf("\x1b[%dD\x1b[K", i);

        		int history_len = get_history(0, buf);
        		printf("%s", buf);
				n = maxlen;
				i = history_len - 1;
    		}
		}
	}

	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;

}

char buf[1024];


void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit(0);
}

int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2024                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}
	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
		readline(buf, sizeof buf);//读取用户输入的命令并存在buf中
		save_history(buf);
		
		if (buf[0] == '#') {
			continue;
		}//如果是注释，直接跳过

		int len = strlen(buf);
		if (buf[len - 1] == '&') {
			buf[len] = '\0'; 
		}
		
		if (echocmds) {
			printf("# %s\n", buf);
		}//如果回显，则直接在命令行上回显输出命令
		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}//创建一个子进程
		if (r == 0) {
			//子进程执行命令
			runcmd(buf);
			exit(0);
		} else {
			if (buf[len - 1] == '&') {
				syscall_addjob(r,buf);
				continue;
			}
			wait(r);
		}//在子进程中运行命令，在父进程中等待子进程运行解释
	}
	return 0;
}
