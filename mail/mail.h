
#ifndef MAIL_H_INCLUDED
#define MAIL_H_INCLUDED

#include <stdio.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <sys/semaphore.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

//邮箱共享内存结构体
typedef struct {
	//数据接收的位置
	int pos_recv;
	//数据发送的位置
	int pos_send;
	//邮箱数据缓冲区
	unsigned char buf[1024];
} mail_shm_t;

//邮箱元数据结构体
typedef struct {
	//邮箱持有者PID
	pid_t owner_pid;
	//发送信号量
	sem_t *s_send;
	//接收信号量
	sem_t *s_recv;
	//发送互斥锁
	sem_t *m_send;
	//接收互斥锁
	sem_t *m_recv;
	//共享内存ID
	int shmid;
	//共享内存
	mail_shm_t *shm;
} mail_t;


//打开自己或其他进程的邮箱
mail_t* mail_open(pid_t pid) {
	int shmid;
	char sem_name[50] = {0};
	mail_t* mail = NULL;
	bool is_self = pid == getpid();
	if (is_self) {
		shmid = shmget(pid, sizeof(mail_shm_t), IPC_CREAT | IPC_EXCL | 0666);
	}
	else {
		shmid = shmget(pid, sizeof(mail_shm_t), 0);
	}
	if (shmid == -1) return NULL;
	mail = (mail_t*)malloc(sizeof(mail_t));
	if (mail == NULL) return NULL;
	//设置pid
	mail->owner_pid = pid;
	mail->shmid = shmid;
	mail->shm = (mail_shm_t*)shmat(shmid, NULL, 0);
	if (is_self) {
		//创建新邮箱
		memset(mail->shm->buf, 0, sizeof(mail->shm->buf));
		mail->shm->pos_recv = mail->shm->pos_send = 0;
		//创建信号量和互斥锁
		snprintf(sem_name, sizeof(sem_name), "mail_s:%d", pid);
		mail->s_send = sem_open(sem_name, O_CREAT | O_EXCL, 0666, sizeof(mail->shm->buf));
		snprintf(sem_name, sizeof(sem_name), "mail_r:%d", pid);
		mail->s_recv = sem_open(sem_name, O_CREAT | O_EXCL, 0666, 0);

		snprintf(sem_name, sizeof(sem_name), "mail_s_mtx:%d", pid);
		mail->m_send = sem_open(sem_name, O_CREAT | O_EXCL, 0666, 1);
		snprintf(sem_name, sizeof(sem_name), "mail_r_mtx:%d", pid);
		mail->m_recv = sem_open(sem_name, O_CREAT | O_EXCL, 0666, 1);
	}
	else {
		//打开信号量和互斥锁
		snprintf(sem_name, sizeof(sem_name), "mail_s:%d", pid);
		mail->s_send = sem_open(sem_name, 0);
		snprintf(sem_name, sizeof(sem_name), "mail_r:%d", pid);
		mail->s_recv = sem_open(sem_name, 0);

		snprintf(sem_name, sizeof(sem_name), "mail_s_mtx:%d", pid);
		mail->m_send = sem_open(sem_name, 0);
		snprintf(sem_name, sizeof(sem_name), "mail_r_mtx:%d", pid);
		mail->m_recv = sem_open(sem_name, 0);
	}
	return mail;
}

//关闭打开的邮箱
void mail_close(mail_t* mail) {
	if (mail == NULL) return;

	int shmid = mail->shmid;
	bool is_self = getpid() == mail->owner_pid;
	shmdt(mail->shm);
	//关闭信号量和互斥锁
	sem_close(mail->s_send);
	sem_close(mail->s_recv);
	sem_close(mail->m_send);
	sem_close(mail->m_recv);
	if (is_self) {
		shmctl(shmid, IPC_RMID, NULL);
		//删除信号量和互斥锁
		char sem_name[50] = {0};
		snprintf(sem_name, sizeof(sem_name), "mail_s:%d", getpid());
		sem_unlink(sem_name);
		snprintf(sem_name, sizeof(sem_name), "mail_r:%d", getpid());
		sem_unlink(sem_name);

		snprintf(sem_name, sizeof(sem_name), "mail_s_mtx:%d", getpid());
		sem_unlink(sem_name);
		snprintf(sem_name, sizeof(sem_name), "mail_r_mtx:%d", getpid());
		sem_unlink(sem_name);
	}
	free(mail);
}

//从邮箱中接收一个字节
int mail_recv_byte(mail_t* mail) {
	if (mail == NULL || mail->owner_pid != getpid()) return -1;
	unsigned char data;
	//等待有数据到来
	sem_wait(mail->s_recv);
	//尝试锁上接收互斥锁，保持线程同步
	sem_wait(mail->m_recv);
	data = mail->shm->buf[mail->shm->pos_recv];
	mail->shm->pos_recv = (mail->shm->pos_recv + 1) % sizeof(mail->shm->buf);
	sem_post(mail->m_recv);
	sem_post(mail->s_send);
	return data;
}

//发送一个字节到邮箱
void mail_send_byte(mail_t* mail, const unsigned char data) {
	if (mail == NULL || mail->owner_pid == getpid()) return;
	//等待缓冲区有空间
	sem_wait(mail->s_send);
	//尝试锁上发送互斥锁，保持线程同步
	sem_wait(mail->m_send);
	mail->shm->buf[mail->shm->pos_send] = data;
	mail->shm->pos_send = (mail->shm->pos_send + 1) % sizeof(mail->shm->buf);
	sem_post(mail->m_send);
	sem_post(mail->s_recv);
}

//从邮箱中接收一系列数据
void mail_recv(mail_t* mail, void* dest, unsigned size) {
	if (dest == NULL || mail == NULL || mail->owner_pid != getpid()) return;
	unsigned char *conv_dest = (unsigned char*)dest;
	while (size--)
		*(conv_dest++) = mail_recv_byte(mail);
}

//发送一系列数据到邮箱
void mail_send(mail_t* mail, void* src, unsigned size) {
	if (src == NULL || mail == NULL || mail->owner_pid == getpid()) return;
	unsigned char *conv_dest = (unsigned char*)src;
	while (size--)
		mail_send_byte(mail, *(conv_dest++));
}

#endif
