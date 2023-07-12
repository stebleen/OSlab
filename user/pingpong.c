#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char **argv) {

    int ptoc[2], ctop[2];   // 对应两个管道，0代表读操作，1代表写操作

    pipe(ptoc); // 创建用于 父进程 -> 子进程 的管道
	pipe(ctop); // 创建用于 子进程 -> 父进程 的管道
	
	if(fork() != 0) { // 父进程
		write(ptoc[1], "!", 1); // 1. 父进程首先向发出该字节
		char buf;
		read(ctop[0], &buf, 1); // 2. 父进程发送完成后，开始等待子进程的回复
		printf("%d: received pong\n", getpid()); // 5. 子进程收到数据，read 返回，输出 pong
	} else { // 子进程
		char buf;
		read(ptoc[0], &buf, 1); // 3. 子进程读取管道，收到父进程发送的字节数据
		printf("%d: received ping\n", getpid());
		write(ctop[1], &buf, 1); // 4. 子进程通过 子->父 管道，将字节送回父进程
	}
	exit(0);
}
