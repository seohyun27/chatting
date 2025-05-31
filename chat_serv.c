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

// ��Ƽ ������ ���α׷���
// ����
// ������ : gcc chat_serv.c -o chat_serv -lpthread
// ���� : ./chat_serv 8080

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
void error_handling(char * msg);
int send_whisper_msg(char* msg, int len);

int clnt_cnt=0; //Ŭ���̾�Ʈ ����
int clnt_socks[MAX_CLNT]; //���ݱ��� ������� Ŭ���̾�Ʈ ����
char clnt_names[MAX_CLNT][NAME_SIZE]; //���ݱ��� ������� Ŭ���̾�Ʈ�� �̸��� ����

pthread_mutex_t mutx; //���� �����͸� �����ϱ� ���� ���ؽ�(������ key)

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
	
	printf("������ ����Ǿ����ϴ�.\n");

	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz); //Ŭ���̾�Ʈ ���� ����
		
		str_len = read(clnt_sock, clnt_name, NAME_SIZE - 1);
		clnt_name[str_len] = 0; //C ���ڿ��� ���� �ݵ�� null�� ������ ��

		pthread_mutex_lock(&mutx); //�Ӱ� ���� ���� (blocking)
		clnt_socks[clnt_cnt] = clnt_sock; //Ŭ���̾�Ʈ ������ ���� �迭�� ����
		strcpy(clnt_names[clnt_cnt++], clnt_name); //Ŭ���̾�Ʈ �̸��� �̸� �迭�� ����
		pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
	
		//������ ���� -> handle_clnt �Լ� ����
		//������� Ŭ����̾�Ʈ ����(clnt_sock ����) ���� �� �Ѱ��ֱ� 
		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
		pthread_detach(t_id); //Non blocking���� t_id �������� ���� ��ٸ���
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
	}
	close(serv_sock);
	return 0;
}
	
void * handle_clnt(void * arg) //�����尡 �����ϴ� �Լ�
{
	int clnt_sock=*((int*)arg); //������ ���� �� �Ѱ� ���� clnt_sock ���� ������ ����
	int str_len=0, i;
	char msg[NAME_SIZE + BUF_SIZE];
	int check; //1:1 �޽������� ���� ���θ� Ȯ���ϱ� ���� ����
	char* error = "�ش� ��ȭ���� ����ϴ� Ŭ���̾�Ʈ�� �������� �ʽ��ϴ�.\n";

	while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) { //blocking �Լ�. Ŭ���̾�Ʈ���Լ� ����� �� ������ ��ٸ���
		if (msg[0] == '@') { //Ŭ���̾�Ʈ���Լ� ���� �޽����� 1:1 �޽������
			check = send_whisper_msg(msg, str_len);
			if (check == 0) write(clnt_sock, error, strlen(error)); //�����ڰ� �������� �ʴ´ٸ� ���� �޽����� ��ȯ
		}
		else //Ŭ���̾�Ʈ���Լ� ���� �޽����� ��ü �޽������
			send_msg(msg, str_len);
	}
	//��� ���� Ŭ���̾�Ʈ�� ������ ��� read()�� 0�� ���� -> while���� break
	
	pthread_mutex_lock(&mutx); //�Ӱ� ���� ����
	//������ ���� Ŭ���̾�Ʈ(����) �����
	for(i=0; i<clnt_cnt; i++) 
	{
		if(clnt_sock==clnt_socks[i]) //clnt_socks ���鼭 ���� ã��
		{
			while(i <clnt_cnt-1)
			{
				clnt_socks[i] = clnt_socks[i+1];
				strcpy(clnt_names[i], clnt_names[i+1]); //�� �ڷ� �� ĭ�� ������ ����
				i++;
			}
			break;
		}
	}
	clnt_cnt--; //Ŭ���̾�Ʈ ���� -1
	pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
	close(clnt_sock);
	return NULL; //main �Լ��� main �����尡 �ްԵǴ� ���� ��
}

void send_msg(char * msg, int len)   //���� ������ ����� ��� Ŭ���̾�Ʈ���� ���۹��� ���� ����
{
	int i;

	pthread_mutex_lock(&mutx); //�Ӱ� ���� ����
	for(i=0; i<clnt_cnt; i++)
		write(clnt_socks[i], msg, len); //Ŭ���̾�Ʈ �迭�� ���� ��� Ŭ���̾�Ʈ���� �޾Ҵ� �޽����� �ٽ� ����
	pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��
}

int send_whisper_msg(char* msg, int len)   //���� ������ ����� ��� Ŭ���̾�Ʈ���� ���۹��� ���� ����
{
	int i;
	char* receiver, * text;
	char whisper_msg[NAME_SIZE+BUF_SIZE];
	char clnt_name[NAME_SIZE]; //Ŭ���̾�Ʈ�� �̸��� �޾ƿ��� ���� ����

	receiver = strtok(msg, " "); //�޽����� ���� ���
	receiver++; //receiver�� �ٷ� �� @ ����
	text = strtok(NULL, " "); //�޽����� ����

	pthread_mutex_lock(&mutx); //�Ӱ� ���� ����
	if (strcmp(receiver, "all") == 0) { //��� Ŭ���̾�Ʈ���� ������ ��
		sprintf(whisper_msg, "%s %s", "(all)", text);
		for (i = 0; i < clnt_cnt; i++)
			write(clnt_socks[i], whisper_msg, strlen(whisper_msg));
		pthread_mutex_unlock(&mutx);
		return 1;
	}

	for (i = 0; i < clnt_cnt; i++) //clnt_names ��ü�� ���� ���ù� ã��
		if (strcmp(clnt_names[i], receiver) == 0) {
			sprintf(whisper_msg, "%s %s\n", "(whisper)", text);
			write(clnt_socks[i], whisper_msg, strlen(whisper_msg));
			pthread_mutex_unlock(&mutx);
			return 1;
		}
	
	pthread_mutex_unlock(&mutx); //�Ӱ� ���� ��

	//���ù��� �������� �ʴ´ٸ� 0�� ��ȯ
	return 0;

}


void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}