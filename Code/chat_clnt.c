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
	
// Multi-threading 기반 채팅 프로그램
// 클라이언트
// 컴파일 : gcc chat_clnt.c -o chat_clnt -lpthread
// 실행 : ./chat_clnt <IP> <port> <name>

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
	
	sprintf(name, "[%s]", argv[3]); //프로그램 실행 시 입력받은 닉네임을 [%s] 포멧에 맞춰 name에 저장
	sock=socket(PF_INET, SOCK_STREAM, 0); //통신 소켓 생성
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_addr.sin_port=htons(atoi(argv[2]));
	  
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) //소켓 연결
		error_handling("connect() error");

	write(sock, argv[3], strlen(argv[3])); //연결된 서버에게 자신의 닉네임을 전달

	printf("=============================================\n");
	printf("		종료 : q\n");
	printf("		귓속말 : @username\n");
	printf("		전체 메시지 : @all\n");
	printf("=============================================\n");

	pthread_create(&snd_thread, NULL, send_msg, (void*)&sock); //write 전용 쓰레드 생성
	pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock); //read 전용 쓰레드 생성
	pthread_join(snd_thread, &thread_return); //blocking 상태로 write 쓰레드의 리턴을 기다리기
	pthread_join(rcv_thread, &thread_return);
	close(sock);  
	return 0;
}
	
void * send_msg(void * arg) //write 쓰레드가 실행하는 main 함수
{
	int sock=*((int*)arg);
	char name_msg[NAME_SIZE+BUF_SIZE]; //서버에게 보낼 최종 메시지를 저장하는 변수

	while(1) 
	{
		fgets(msg, BUF_SIZE, stdin); //키보드로 입력한 문자열을 msg 배열에 저장

		if(!strcmp(msg,"q\n")||!strcmp(msg,"Q\n")) //msg가 Q라면 프로세스 종료
		{
			close(sock);
			exit(0);
		}

		if(msg[0] == '@') //보내려는 메시지가 1:1 메시지인 경우
		{
			char* receiver, * text;

			memset(name_msg, 0, sizeof(name_msg)); //최종 메시지를 담을 name_msg 배열을 0으로 초기화
			receiver = strtok(msg, " "); //메시지를 받을 사람
			text = strtok(NULL, ""); //메시지의 내용

			// @receiver [name] text 형태로 name_msg에 저장
			sprintf(receiver_name, "%s %s", receiver, name);
			sprintf(name_msg, "%s %s", receiver_name, text);
		}
		else //보내려는 메시지가 1:1 메시지가 아닐 경우
			sprintf(name_msg,"%s %s", name, msg); // [name] text 형태로 name_msg에 저장

		write(sock, name_msg, strlen(name_msg)); //name_msg의 내용을 서버로 전송
	}
	return NULL;
}
	
void * recv_msg(void * arg) //read 쓰레드가 실행하는 main 함수
{
	int sock=*((int*)arg);
	char name_msg[NAME_SIZE+BUF_SIZE];
	int str_len;

	while(1)
	{
		str_len=read(sock, name_msg, NAME_SIZE+BUF_SIZE-1); //서버에게서 수신받은 내용을 name_msg에 저장
		if(str_len==-1) 
			return (void*)-1;
		name_msg[str_len]=0;
		fputs(name_msg, stdout); //name_msg의 내용을 모니터에 출력
	}
	return NULL;
}
	
void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
