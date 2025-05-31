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

// 멀티 쓰레드 프로그래밍
// 서버
// 컴파일 : gcc chat_serv.c -o chat_serv -lpthread
// 실행 : ./chat_serv 8080

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
void error_handling(char * msg);
int send_whisper_msg(char* msg, int len);

int clnt_cnt=0; //클라이언트 개수
int clnt_socks[MAX_CLNT]; //지금까지 만들어진 클라이언트 저장
char clnt_names[MAX_CLNT][NAME_SIZE]; //지금까지 만들어진 클라이언트의 이름을 저장

pthread_mutex_t mutx; //공유 데이터를 관리하기 위한 뮤텍스(일종의 key)

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	char clnt_name[NAME_SIZE]; //클라이언트의 이름을 받아오기 위한 변수
	int str_len;
	pthread_t t_id;
	if(argc!=2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
  
	pthread_mutex_init(&mutx, NULL); //뮤텍스 생성
	serv_sock=socket(PF_INET, SOCK_STREAM, 0); //서버 소켓 생성

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET; 
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1]));
	
	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1) //포트, IP 할당
		error_handling("bind() error");
	if(listen(serv_sock, 5)==-1) //큐 할당
		error_handling("listen() error");
	
	printf("서버가 실행되었습니다.\n");

	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz); //클라이언트 소켓 연결
		
		str_len = read(clnt_sock, clnt_name, NAME_SIZE - 1);
		clnt_name[str_len] = 0; //C 문자열의 끝은 반드시 null로 끝나야 함

		pthread_mutex_lock(&mutx); //임계 영역 시작 (blocking)
		clnt_socks[clnt_cnt] = clnt_sock; //클라이언트 소켓을 소켓 배열에 저장
		strcpy(clnt_names[clnt_cnt++], clnt_name); //클라이언트 이름을 이름 배열에 복사
		pthread_mutex_unlock(&mutx); //임계 영역 끝
	
		//쓰레드 생성 -> handle_clnt 함수 실행
		//만들어진 클라라이언트 소켓(clnt_sock 변수) 안의 값 넘겨주기 
		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
		pthread_detach(t_id); //Non blocking으로 t_id 쓰레드의 리턴 기다리기
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
	}
	close(serv_sock);
	return 0;
}
	
void * handle_clnt(void * arg) //쓰레드가 실행하는 함수
{
	int clnt_sock=*((int*)arg); //쓰레드 생성 시 넘겨 받은 clnt_sock 값을 변수에 저장
	int str_len=0, i;
	char msg[NAME_SIZE + BUF_SIZE];
	int check; //1:1 메시지에서 전송 여부를 확인하기 위한 변수
	char* error = "해당 대화명을 사용하는 클라이언트가 존재하지 않습니다.\n";

	while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) { //blocking 함수. 클라이언트에게서 통신이 올 때까지 기다리기
		if (msg[0] == '@') { //클라이언트에게서 받은 메시지가 1:1 메시지라면
			check = send_whisper_msg(msg, str_len);
			if (check == 0) write(clnt_sock, error, strlen(error)); //수신자가 존재하지 않는다면 에러 메시지를 반환
		}
		else //클라이언트에게서 받은 메시지가 전체 메시지라면
			send_msg(msg, str_len);
	}
	//통신 중인 클라이언트가 연결을 끊어서 read()가 0을 리턴 -> while문이 break
	
	pthread_mutex_lock(&mutx); //임계 영역 시작
	//연결이 끊긴 클라이언트(본인) 지우기
	for(i=0; i<clnt_cnt; i++) 
	{
		if(clnt_sock==clnt_socks[i]) //clnt_socks 돌면서 본인 찾기
		{
			while(i <clnt_cnt-1)
			{
				clnt_socks[i] = clnt_socks[i+1];
				strcpy(clnt_names[i], clnt_names[i+1]); //내 뒤로 한 칸씩 앞으로 당기기
				i++;
			}
			break;
		}
	}
	clnt_cnt--; //클라이언트 개수 -1
	pthread_mutex_unlock(&mutx); //임계 영역 끝
	close(clnt_sock);
	return NULL; //main 함수의 main 쓰레드가 받게되는 리턴 값
}

void send_msg(char * msg, int len)   //현재 서버와 연결된 모든 클라이언트에게 전송받은 값을 전달
{
	int i;

	pthread_mutex_lock(&mutx); //임계 영역 시작
	for(i=0; i<clnt_cnt; i++)
		write(clnt_socks[i], msg, len); //클라이언트 배열을 돌며 모든 클라이언트에게 받았던 메시지를 다시 전송
	pthread_mutex_unlock(&mutx); //임계 영역 끝
}

int send_whisper_msg(char* msg, int len)   //현재 서버와 연결된 모든 클라이언트에게 전송받은 값을 전달
{
	int i;
	char* receiver, * text;
	char whisper_msg[NAME_SIZE+BUF_SIZE];
	char clnt_name[NAME_SIZE]; //클라이언트의 이름을 받아오기 위한 변수

	receiver = strtok(msg, " "); //메시지를 받을 사람
	receiver++; //receiver의 바로 앞 @ 삭제
	text = strtok(NULL, " "); //메시지의 내용

	pthread_mutex_lock(&mutx); //임계 영역 시작
	if (strcmp(receiver, "all") == 0) { //모든 클라이언트에게 전송할 때
		sprintf(whisper_msg, "%s %s", "(all)", text);
		for (i = 0; i < clnt_cnt; i++)
			write(clnt_socks[i], whisper_msg, strlen(whisper_msg));
		pthread_mutex_unlock(&mutx);
		return 1;
	}

	for (i = 0; i < clnt_cnt; i++) //clnt_names 전체를 돌며 리시버 찾기
		if (strcmp(clnt_names[i], receiver) == 0) {
			sprintf(whisper_msg, "%s %s\n", "(whisper)", text);
			write(clnt_socks[i], whisper_msg, strlen(whisper_msg));
			pthread_mutex_unlock(&mutx);
			return 1;
		}
	
	pthread_mutex_unlock(&mutx); //임계 영역 끝

	//리시버가 존재하지 않는다면 0을 반환
	return 0;

}


void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}