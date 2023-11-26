#include <stdio.h>
#include "mail.h"

mail_t *mail_a = NULL;
mail_t *mail_b = NULL;

void cleanup() {
	if (mail_a != NULL)
		mail_close(mail_a);
	if (mail_b != NULL)
		mail_close(mail_b);
}

int main() {
	char buf[100];
	atexit(cleanup);
	printf("我的PID是：%d\n", getpid());
	mail_a = mail_open(getpid());
	pid_t pid_b;
	printf("输入邮箱B进程的PID：");
	scanf("%d", &pid_b);
	mail_b = mail_open(pid_b);
	if (mail_b == NULL) {
		printf("邮箱B打开失败！\n");
		return 1;
	}
	mail_recv(mail_a, buf, 16);
	printf("接收到字符串：%s\n", buf);
	mail_send(mail_b, "This is mail a.", 16);
	printf("已发送字符串：This is mail a.\n");
	printf("1秒钟后关闭邮箱\n");
	sleep(1);
	return 0;
}
