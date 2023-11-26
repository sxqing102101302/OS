#include <stdio.h>
#include "mail.h"

mail_t *mail_a;
mail_t *mail_b;

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
	mail_b = mail_open(getpid());
	pid_t pid_a;
	printf("输入邮箱A进程的PID：");
	scanf("%d", &pid_a);
	mail_a = mail_open(pid_a);
	if (mail_a == NULL) {
		printf("邮箱B打开失败！\n");
		return 1;
	}
	mail_send(mail_a, "This is mail b.", 16);
	printf("已发送字符串：This is mail b.\n");
	mail_recv(mail_b, buf, 16);
	printf("接收到字符串：%s\n", buf);
	printf("1秒钟后关闭邮箱\n");
	sleep(1);
	return 0;
}
