#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
	
#define BUF_SIZE 100
#define NAME_SIZE 20
#define RECEIVER_SIZE 20
	
// ��Ƽ ������ ���α׷���
// Ŭ���̾�Ʈ
// ������ : gcc chat_clnt.c -o chat_clnt -lpthread
// ���� : ./chat_clnt 127.0.0.1 8080 user1

void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(char * msg);
	
char name[NAME_SIZE]="[DEFAULT]";
char msg[BUF_SIZE];
char receiver_name[NAME_SIZE + RECEIVER_SIZE];

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void * thread_return;
	if(argc!=4) {
		printf("Usage : %s <IP> <port> <name>\n", argv[0]);
		exit(1);
	 }
	
	sprintf(name, "[%s]", argv[3]); //���α׷� ���� �� �Է¹��� �г����� [%s] ���信 ���� name�̶�� ������ ����(print)
	sock=socket(PF_INET, SOCK_STREAM, 0); //��� ���� ����
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_addr.sin_port=htons(atoi(argv[2]));
	  
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) //blocking �Լ�
		error_handling("connect() error");

	write(sock, argv[3], strlen(argv[3]));

	printf("=============================================\n");
	printf("		���� : q\n");
	printf("		�ӼӸ� : @username\n");
	printf("=============================================\n");

	pthread_create(&snd_thread, NULL, send_msg, (void*)&sock); //write ������ ����
	pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock); //read ������ ����
	pthread_join(snd_thread, &thread_return); //blocking ���·� ���� �������� ������ ��ٸ���
	pthread_join(rcv_thread, &thread_return);
	close(sock);  
	return 0;
}
	
void * send_msg(void * arg) //write �����尡 �����ϴ� main �Լ�
{
	int sock=*((int*)arg); //main �����忡�� �Ѱܹ��� ��� ����
	char name_msg[NAME_SIZE+BUF_SIZE]; //�������� ���� ���� �޽����� �����ϴ� ����

	while(1) 
	{
		fgets(msg, BUF_SIZE, stdin); //Ű����� �Է��� ���ڿ��� msg �迭�� ����

		if(!strcmp(msg,"q\n")||!strcmp(msg,"Q\n")) //msg�� Q��� ���μ��� ����
		{
			close(sock);
			exit(0);
		}

		if(msg[0] == '@') //�������� �޽����� 1:1�� ���
		{
			char* receiver, * text;

			memset(name_msg, 0, sizeof(name_msg)); //���� �޽����� ���� name_msg �迭�� 0���� �ʱ�ȭ
			receiver = strtok(msg, " "); //�޽����� ���� ���
			text = strtok(NULL, ""); //�޽����� ����

			sprintf(receiver_name, "%s %s", receiver, name); //���ù��� �̸��� ������ ����� �̸��� ��ģ��
			sprintf(name_msg, "%s %s", receiver_name, text); //���� ����� text�� ���� name_msg�� ����
		}
		else //�������� �޽����� 1:1�� �ƴ� ���
			sprintf(name_msg,"%s %s", name, msg);

		write(sock, name_msg, strlen(name_msg)); //name_msg �迭�� ������ ������ ����
	}
	return NULL;
}
	
void * recv_msg(void * arg) //read �����尡 �����ϴ� main �Լ�
{
	int sock=*((int*)arg); //main �����忡�� �޾ƿ� ����
	char name_msg[NAME_SIZE+BUF_SIZE];
	int str_len;
	while(1) //���� �ݺ�
	{
		str_len=read(sock, name_msg, NAME_SIZE+BUF_SIZE-1); //blocking �Լ�. sock���κ��� ���� ���޹���
		if(str_len==-1) 
			return (void*)-1;
		name_msg[str_len]=0; //C ���ڿ��� ���� �ݵ�� null�� ������ ��
		fputs(name_msg, stdout); //������� ���ڿ��� ����Ϳ� ���
	}
	return NULL;
}
	
void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
