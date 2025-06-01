#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_CLNT 256
#define BUF_SIZE 100
#define NAME_SIZE 20

// Multi-threading ��� ä�� ���α׷�
// ����
// ������ : gcc chat_serv.c -o chat_serv -lpthread
// ���� : ./chat_serv <port>

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
int send_whisper_msg(char* msg, int len); //1:1 ��ȭ�� �����ϴ� �޼ҵ�
void error_handling(char * msg);

int clnt_cnt = 0; //���� ���� ���� Ŭ���̾�Ʈ�� ����
int clnt_socks[MAX_CLNT]; //���� ���� ���� Ŭ���̾�Ʈ �迭
char clnt_names[MAX_CLNT][NAME_SIZE]; //���ݱ��� ������� Ŭ���̾�Ʈ�� �̸��� �����ϴ� �迭

pthread_mutex_t mutx; //���� �����͸� �����ϱ� ���� ���ؽ�

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	char clnt_name[NAME_SIZE]; //Ŭ���̾�Ʈ�� �̸��� �޾ƿ��� ���� ����
	int str_len;
	pthread_t t_id;

	if(argc!=2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
  
	pthread_mutex_init(&mutx, NULL); //���ؽ� ����
	serv_sock=socket(PF_INET, SOCK_STREAM, 0); //���� ���� ����

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET; 
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1]));
	
	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1) //��Ʈ, IP �Ҵ�
		error_handling("bind() error");
	if(listen(serv_sock, 5)==-1) //ť �Ҵ�
		error_handling("listen() error");
	
	printf("=====================================\n");
	printf("	������ ����Ǿ����ϴ�.\n");
	printf("=====================================\n");

	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz); //Ŭ���̾�Ʈ ���� ����
		
		str_len = read(clnt_sock, clnt_name, NAME_SIZE - 1); //Ŭ���̾�Ʈ�� �̸��� ����
		clnt_name[str_len] = 0;

		pthread_mutex_lock(&mutx); //�Ӱ� ���� ����
		clnt_socks[clnt_cnt] = clnt_sock; //Ŭ���̾�Ʈ ������ ���� �迭�� ����
		strcpy(clnt_names[clnt_cnt++], clnt_name); //Ŭ���̾�Ʈ �̸��� �̸� �迭�� ����(����)
		pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
	
		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock); //������ ���� -> handle_clnt �Լ� ����
		pthread_detach(t_id); //Non blocking���� t_id �������� ���� ��ٸ���
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
	}
	close(serv_sock);
	return 0;
}
	
void * handle_clnt(void * arg) //������ �����尡 �����ϴ� ���� �Լ�
{
	int clnt_sock=*((int*)arg);
	int str_len=0, i;
	char msg[NAME_SIZE + BUF_SIZE];
	int check; //1:1 �޽������� ���� ���θ� Ȯ���ϱ� ���� ����
	char* error = "�ش� ��ȭ���� ����ϴ� Ŭ���̾�Ʈ�� �������� �ʽ��ϴ�.\n";

	while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) { //Ŭ���̾�Ʈ���Լ� �޽��� ����
		if (msg[0] == '@') { //���Ź��� �޽����� 1:1 �޽������ send_whisper_msg �޼ҵ� ����
			check = send_whisper_msg(msg, str_len); //���� �����ڰ� �������� �ʴ´ٸ� send_whisper_msg�� -1�� ��ȯ
			if (check == -1) write(clnt_sock, error, strlen(error)); //�ش� Ŭ���̾�Ʈ���� ���� �޽����� ����
		}
		else //���Ź��� �޽����� ��ü �޽������ send_msg �޼ҵ� ����
			send_msg(msg, str_len);

		memset(msg, 0, sizeof(msg)); //���� ����� ���� msg �迭�� 0���� �ʱ�ȭ
	}
	
	pthread_mutex_lock(&mutx); //�Ӱ� ���� ����
	for(i=0; i<clnt_cnt; i++) //������ ���� Ŭ���̾�Ʈ ����
	{
		if(clnt_sock==clnt_socks[i]) //clnt_socks ���鼭 clnt_sock ã��
		{
			while(i <clnt_cnt-1)
			{
				clnt_socks[i] = clnt_socks[i+1]; //clnt_sock �ڿ������� �� ĭ�� ������ ���� (clnt_sock ����)
				strcpy(clnt_names[i], clnt_names[i+1]);
				i++;
			}
			break;
		}
	}
	clnt_cnt--;
	pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
	close(clnt_sock);
	return NULL;
}

void send_msg(char * msg, int len) //��ε�ĳ��Ʈ ���
{
	int i;

	pthread_mutex_lock(&mutx); //�Ӱ� ���� ����
	for(i=0; i<clnt_cnt; i++) //���� ������ ����� ��� Ŭ���̾�Ʈ���� ���۹��� �޽����� ���� 
		write(clnt_socks[i], msg, len);
	pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
}

int send_whisper_msg(char* msg, int len) // 1:1 ���
{
	int i;
	char* receiver, * text;
	char whisper_msg[NAME_SIZE+BUF_SIZE];

	receiver = strtok(msg, " "); //�޽����� ���� Ŭ���̾�Ʈ, @�� �������� �ʰ� ����
	receiver++; //receiver�� �ٷ� �� @ ����
	text = strtok(NULL, ""); //receiver���� ���۵� �޽����� ����

	pthread_mutex_lock(&mutx); //�Ӱ� ���� ����
	if (!strcmp(receiver, "all")) { //�޽����� ���� receiver�� all�� ��
		sprintf(whisper_msg, "%s %s", "(all)", text);
		for (i = 0; i < clnt_cnt; i++) //��� Ŭ���̾�Ʈ���� ����
			write(clnt_socks[i], whisper_msg, strlen(whisper_msg));
		pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
		return 0; //�޼ҵ� ���� ����
	}

	for (i = 0; i < clnt_cnt; i++) //clnt_names ��ü�� ���� receiver ã��
		if (!strcmp(clnt_names[i], receiver)) { //clnt_names �迭 �ȿ� receiver�� �����Ѵٸ�
			sprintf(whisper_msg, "%s %s", "(whisper)", text);
			write(clnt_socks[i], whisper_msg, strlen(whisper_msg)); //�ش� Ŭ���̾�Ʈ���� ����
			pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
			return 0; //�޼ҵ� ���� ����
		}
	
	pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
	return -1; //�ش� receiver�� �������� ����. ���α׷� ������ ����
}


void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}