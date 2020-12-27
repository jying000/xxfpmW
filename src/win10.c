/*
 *  jying FastCGI Process Manager
 *
 *  Copyright (C) 2020 
 *
 *  Website: https://www.cnblogs.com/jying
 * 
 *  2020/12/06 Created.
 *
 *  Description: This file mainly includes the functions about 
 *
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif //_WIN32_WINNT

#include <windows.h>
#include <winsock.h>
#include <wininet.h>
#include <stdbool.h>
#define SHUT_RDWR SD_BOTH

#ifndef JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
#define JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE (0x2000)
#endif
HANDLE FcpJobObject; // 定义句柄

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
// 新加用于日志文件
#include <stdarg.h>
#include <time.h>

#define MAX_PROCESSES 1024
#define FILE_MAX_SIZE (1024 * 1024) // 定义文件大小，超过该大小，会启动清空删除操作。
static const char version[] = "Revision: 2020.12.06";
static char *prog_name;
int number = 1;
int port = 9000;
char *ip = "127.0.0.1";
char *user = "";
char *root = "";
char *path = "";
char *group = "";
int listen_fd;
struct sockaddr_in listen_addr; // sockaddr_in 数据结构体，允许把port和addr 分开存储在两个变量中
int process_fp[MAX_PROCESSES];
int process_idx = 0;
pthread_t threads[MAX_PROCESSES];

static struct option longopts[] =
	{
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"number", required_argument, NULL, 'n'},
		{"ip", required_argument, NULL, 'i'},
		{"port", required_argument, NULL, 'p'},
		{"user", required_argument, NULL, 'u'},
		{"group", required_argument, NULL, 'g'},
		{"root", required_argument, NULL, 'r'},
		{NULL, 0, NULL, 0}};

static char opts[] = "hvnipugr";

// 输出可用参数
static void usage(FILE *where)
{
	//
	fprintf(where, ""
				   "Usage: %s path [-n number] [-i ip] [-p port]\n"
				   "Manage FastCGI processes.\n"
				   "\n"
				   " -n, --number  number of processes to keep\n"
				   " -i, --ip      ip address to bind\n"
				   " -p, --port    port to bind, default is 8000\n"
				   " -u, --user    start processes using specified linux user\n"
				   " -g, --group   start processes using specified linux group\n"
				   " -r, --root    change root direcotry for the processes\n"
				   " -h, --help    output usage information and exit\n"
				   " -v, --version output version information and exit\n"
				   "",
			prog_name);
	exit(where == stderr ? 1 : 0);
}

static void print_version()
{
	printf("\n %s %s\n FastCGI Process Manager\n Copyright 2020 https://www.cnblogs.com/jying\n Compiled on %s\n", prog_name, version, __DATE__);
	exit(0);
}

/*
获得文件大小
@param filename [in]: 文件名
@return 文件大小
*/
long get_file_size(char *filename)
{
	long length = 0;
	FILE *fp = NULL;
	fp = fopen(filename, "rb");
	if (fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
	}
	if (fp != NULL)
	{
		fclose(fp);
		fp = NULL;
	}
	return length;
}

int write_log(char *filename, const char *format, ...)
{
	if (filename != NULL && format != NULL)
	{
		long length = get_file_size(filename);
		// fprintf(stdout, "file length = %d\n", length);
		if (length > FILE_MAX_SIZE)
		{
			unlink(filename); // 删除文件
		}

		FILE *pFile = fopen(filename, "a+b");
		if (pFile != NULL)
		{

			va_list arg;
			int done;

			va_start(arg, format);

			time_t time_log = time(NULL);
			struct tm *tm_log = localtime(&time_log);
			fprintf(pFile, "%04d-%02d-%02d %02d:%02d:%02d ：", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday, tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec);

			done = vfprintf(pFile, format, arg);
			va_end(arg);

			fflush(pFile);
			fclose(pFile);
			pFile = NULL;
			return done;
		}
	}
	return 0;
}

static int try_to_bind()
{
	listen_addr.sin_family = PF_INET;
	listen_addr.sin_addr.s_addr = inet_addr(ip);
	listen_addr.sin_port = htons(port);
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (-1 == bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(struct sockaddr_in)))
	{
		fprintf(stderr, "failed to bind %s:%d\n", ip, port);
		write_log("error_log.txt", "failed to bind %s:%d\r\n", ip, port);
		return -1;
	}

	listen(listen_fd, MAX_PROCESSES);
	return 0;
}

static void *spawn_process(void *arg)
{
	int idx = process_idx++, ret; // process_idx 初始值0
	while (1)
	{

		STARTUPINFO si = {0}; //
		PROCESS_INFORMATION pi = {0};
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = (HANDLE)listen_fd;
		si.hStdOutput = INVALID_HANDLE_VALUE;
		si.hStdError = INVALID_HANDLE_VALUE;
		ret = CreateProcess( // WIN32API函数CreateProcess用来创建一个新的进程和它的主线程，这个新进程运行指定的可执行文件。如果函数执行成功，返回非零值。
			NULL,			 // 指定可执行模块
			path,			 // 指定要执行的命令行，此处为 php-cgi.exe
			NULL,			 // 决定是否返回的句柄可以被子进程继承。如果lpProcessAttributes参数为空（NULL），那么句柄不能被继承。
			NULL,			 // 默认进程安全性，同上，不过这个参数决定的是线程是否被继承.通常置为NULL.
			TRUE,			 // 指定当前进程内句柄可以被子进程继承
			CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
			// CREATE_NO_WINDOW 为新进程不创建新的控制台窗口， CREATE_SUSPENDED 新进程的 主线程会以暂停的状态被创建，直到调用 ResumeThread函数被调用时才运行。
			// 当一个进程被加入到Job中后，没有特殊的说明，那么该进程的所有子进程都将被纳入到Job里。当然，通过JOB_OBJECT_LIMIT_BREAKAWAY_OK或者JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK。
			// Job内进程的子进程就可以不被自动纳入到Job中，两者区别在于，后者将所有子进程自动的赶出Job，而前者的子进程需要在CreateProcess时指定CREATE_BREAKAWAY_FROM_JOB。
			NULL, // 使用本进程的环境变量。指向一个新进程的环境块。如果此参数为空，新进程使用调用进程的环境。
			NULL, // 使用本进程的驱动器和目录。指定子进程的工作路径。如果这个参数为空，新进程将使用与调用进程相同的驱动器和目录。
			&si,
			&pi);

		if (0 == ret)
		{
			fprintf(stderr, "failed to create process %s, ret=%d\n", path, ret);			// 进程创建失败
			write_log("error_log.txt", "failed to create process %s, ret=%d\r\n", path, ret); // 进程创建失败
			return NULL;
		}
		else
		{
			fprintf(stdout, "successful to create process %s, ret=%d\n", path, ret);	  // 进程创建成功
			write_log("log.txt", "successful to create process %s, ret=%d\r\n", path, ret); // 进程创建成功
		}

		/* Use Job Control System */
		if (!AssignProcessToJobObject(FcpJobObject, pi.hProcess))
		{
			TerminateProcess(pi.hProcess, 1);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			write_log("error_log.txt", "AssignProcessToJobObject：failed\r\n");
			return NULL;
		}

		if (!ResumeThread(pi.hThread))
		{
			TerminateProcess(pi.hProcess, 1);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			write_log("error_log.txt", "ResumeThread：failed\r\n");
			return NULL;
		}

		process_fp[idx] = (int)pi.hProcess;
		WaitForSingleObject(pi.hProcess, INFINITE); // 等待函数可使线程自愿进入等待状态，直到一个特定的内核对象变为已通知状态为止。
		/* 第一个参数hObject标识一个能够支持被通知/未通知的内核对象。
			   第二个参数dwMilliseconds允许该线程指明，为了等待该对象变为已通知状态，它将等待多长时间。（INFINITE为无限时间量，INFINITE已经定义为0xFFFFFFFF（或-1））
			   传递INFINITE有些危险。如果对象永远不变为已通知状态，那么调用线程永远不会被唤醒，它将永远处于死锁状态，不过，它不会浪费宝贵的C P U时间。*/
		process_fp[idx] = 0;
		CloseHandle(pi.hThread);
		write_log("log.txt", "WaitForSingleObject：Ready to create a new process\r\n");
	}
}

static int start_processes()
{
	int i;
	pthread_attr_t attr;						 // 线程属性，线程具有属性，用pthread_attr_t表示，在对该结构进行处理之前必须进行初始化，在使用后需要对其去除初始化。
	pthread_attr_init(&attr);					 // 调用pthread_attr_init之后，pthread_t结构所包含的内容就是操作系统实现支持的线程所有属性的默认值。
	pthread_attr_setstacksize(&attr, 64 * 1024); //64KB ，设置堆栈大小
	for (i = 0; i < number; i++)
	{
		if (pthread_create(&threads[i], &attr, spawn_process, NULL) == -1)
		{ // pthread_create是c++系统函数-创建线程， spawn_process是自定义的函数
			fprintf(stderr, "failed to create thread %d\n", i);
			write_log("error_log.txt", "failed to create thread %d\r\n", i);
		}
	}

	for (i = 0; i < number; i++)
	{
		pthread_join(threads[i], NULL); // 线程回收 pthread_join,阻塞等待线程退出，获取线程退出状态,对应进程中 waitpid() 函数，成功：0；失败：错误号
	}
	return 0;
}

void init_win32()
{
	/* init win32 socket */
	static WSADATA wsa_data;							// WSADATA 存放windows socket初始化信息.
	if (WSAStartup((WORD)(1 << 8 | 1), &wsa_data) != 0) // 使用Socket的程序在使用Socket之前必须调用WSAStartup函数。以后应用程序就可以调用所请求的Socket库中的其它Socket函数了。该函数执行成功后返回0。
		exit(1);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;			// JOBOBJECT_EXTENDED_LIMIT_INFORMATION 扩展后的基本限额所对应的数据结构，作业中的某种类型吧
	FcpJobObject = (HANDLE)CreateJobObject(NULL, NULL); // CreateJobObject 创建作业
	if (FcpJobObject == NULL)
		exit(1);

	/* let all processes assigned to this job object // 将所有进程分配给此作业对象
	 * being killed when the job object closed */
	// 作业对象关闭时被杀死
	if (!QueryInformationJobObject(FcpJobObject, JobObjectExtendedLimitInformation, &limit, sizeof(limit), NULL))
	{
		CloseHandle(FcpJobObject);
		exit(1);
	}

	limit.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

	if (!SetInformationJobObject(FcpJobObject, JobObjectExtendedLimitInformation, &limit, sizeof(limit)))
	{
		CloseHandle(FcpJobObject);
		exit(1);
	}
}

/*
	启动入口，关于main函数的介绍可以访问：https://www.cnblogs.com/bianchengzhuji/p/9783772.html，本例使用第五种方法。
	argc：入参命令行参数个数
	**argv 入参命令行参数数组，通常用于实现需要从命令行获取参数的功能。C 语言*号可表示指针，** 表示复合指针
	举例我们执行了：xffpm php-cgi.exe -n 5 -p 8081
*/
int main(int argc, char **argv)
{
	/*如果要注销或关闭系统返回 1 否则返回 0*/
	prog_name = strrchr(argv[0], '/');
	// strrchr()查找一个字符c在另一个字符串str中末次出现的位置（也就是从str的右侧开始查找字符c首次出现的位置），并返回这个位置的地址。
	// 如果未能找到指定字符，那么函数将返回NULL。使用这个地址返回从最后一个字符c到str末尾的字符串。
	// 比如 char *ptr = strrchr("There are two rings", 'r'); 输出ptr是rings，但是这里要注意：ptr是指针，而不是java，php语言中的变量，输出 ptr+1 结果是 ings

	// 输出第一个参数中的文件路径，此处对应为得到xxfpm
	if (prog_name == NULL)
		prog_name = strrchr(argv[0], '\\');
	if (prog_name == NULL)
		prog_name = argv[0];
	else
		prog_name++; // 本例得到xxfpm

	if (argc == 1)
		usage(stderr); // 如果只有一个参数，输出到您的屏幕

	path = argv[1]; // 得到 php-cgi.exe

	opterr = 0;

	for (;;)
	{ // 相当于 while(1)
		int ch;
		if ((ch = getopt_long(argc, argv, opts, longopts, NULL)) == EOF) // 依次获取参数，直到结束字符，此处为依次检查参数h, v, n, i, p, u, g, r
			// 关于 getopt 和 getopt_long 可参看 https://www.cnblogs.com/chenliyang/p/6633739.html
			// getopt_long(5, php-cgi.exe -n 5 -p 8081, "hvnipugr", [自定义的longopts], null) , EOF 表示 End Of File 的缩写,使用EOF是为了避免因试图在文件结尾处进行输入而产生的错误。
			// 直到到达文件的结尾，EOF函数都返回False。对于为访问Random或Binary而打开的文件，直到最后一次执行的Get语句无法读出完整的记录时，EOF都返回False。
			break;
		char *av = argv[optind]; // optind 为 argv的当前索引值， 此处为 *av 依次获取参数 h, v, n, i, p, u, g, r 对应的值
		switch (ch)
		{ // 获取各参数对应的值
		case 'h':
			usage(stdout); // stdout 输出显示到屏幕
			break;
		case 'v':
			print_version();
			break;
		case 'n':
			number = atoi(av); // 取整
			if (number > MAX_PROCESSES)
			{ // MAX_PROCESSES = 1024
				fprintf(stderr, "exceeds MAX_PROCESSES!\n");
				write_log("error_log.txt", "exceeds MAX_PROCESSES!\r\n");
				number = MAX_PROCESSES;
			}
			break;
		case 'u':
			user = av;
			break;
		case 'r':
			root = av;
			break;
		case 'g':
			group = av;
			break;
		case 'i':
			ip = av;
			break;
		case 'p':
			port = atoi(av);
			break;
		default:
			usage(stderr);
			break;
		}
	}

	init_win32(); // 自定义的init_win32函数， 注册作业监控进程？

	int ret;
	ret = try_to_bind(); // 绑定准备要监听的 ip 和 端口，已经被占用，则为-1，否则为0
	if (ret != 0)
		return ret;
	ret = start_processes(); // 监听进程
	if (ret != 0)
		return ret;

	CloseHandle(FcpJobObject);
	WSACleanup(); // 清理作业

	return 0;
}
