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

// Multi-threading 기반 채팅 프로그램
// 서버
// 컴파일 : gcc chat_serv.c -o chat_serv -lpthread
// 실행 : ./chat_serv <port>

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
int send_whisper_msg(char* msg, int len); //1:1 대화를 구현하는 메소드
void error_handling(char * msg);

int clnt_cnt = 0; //현재 접속 중인 클라이언트의 개수
int clnt_socks[MAX_CLNT]; //현재 접속 중인 클라이언트 배열
char clnt_names[MAX_CLNT][NAME_SIZE]; //지금까지 만들어진 클라이언트의 이름을 저장하는 배열

pthread_mutex_t mutx; //공유 데이터를 관리하기 위한 뮤텍스

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
	
	printf("=====================================\n");
	printf("	서버가 실행되었습니다.\n");
	printf("=====================================\n");

	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz); //클라이언트 소켓 연결
		
		str_len = read(clnt_sock, clnt_name, NAME_SIZE - 1); //클라이언트의 이름을 수신
		clnt_name[str_len] = 0;

		pthread_mutex_lock(&mutx); //임계 영역 시작
		clnt_socks[clnt_cnt] = clnt_sock; //클라이언트 소켓을 소켓 배열에 저장
		strcpy(clnt_names[clnt_cnt++], clnt_name); //클라이언트 이름을 이름 배열에 저장(복사)
		pthread_mutex_unlock(&mutx); //임계 영역 끝
	
		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock); //쓰레드 생성 -> handle_clnt 함수 실행
		pthread_detach(t_id); //Non blocking으로 t_id 쓰레드의 리턴 기다리기
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
	}
	close(serv_sock);
	return 0;
}
	
void * handle_clnt(void * arg) //생성된 쓰레드가 실행하는 메인 함수
{
	int clnt_sock=*((int*)arg);
	int str_len=0, i;
	char msg[NAME_SIZE + BUF_SIZE];
	int check; //1:1 메시지에서 전송 여부를 확인하기 위한 변수
	char* error = "해당 대화명을 사용하는 클라이언트가 존재하지 않습니다.\n";

	while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) { //클라이언트에게서 메시지 수신
		if (msg[0] == '@') { //수신받은 메시지가 1:1 메시지라면 send_whisper_msg 메소드 실행
			check = send_whisper_msg(msg, str_len); //만약 수신자가 존재하지 않는다면 send_whisper_msg가 -1을 반환
			if (check == -1) write(clnt_sock, error, strlen(error)); //해당 클라이언트에게 에러 메시지를 리턴
		}
		else //수신받은 메시지가 전체 메시지라면 send_msg 메소드 실행
			send_msg(msg, str_len);

		memset(msg, 0, sizeof(msg)); //다음 통신을 위해 msg 배열을 0으로 초기화
	}
	
	pthread_mutex_lock(&mutx); //임계 영역 시작
	for(i=0; i<clnt_cnt; i++) //연결이 끊긴 클라이언트 삭제
	{
		if(clnt_sock==clnt_socks[i]) //clnt_socks 돌면서 clnt_sock 찾기
		{
			while(i <clnt_cnt-1)
			{
				clnt_socks[i] = clnt_socks[i+1]; //clnt_sock 뒤에서부터 한 칸씩 앞으로 당기기 (clnt_sock 삭제)
				strcpy(clnt_names[i], clnt_names[i+1]);
				i++;
			}
			break;
		}
	}
	clnt_cnt--;
	pthread_mutex_unlock(&mutx); //임계 영역 끝
	close(clnt_sock);
	return NULL;
}

void send_msg(char * msg, int len) //브로드캐스트 통신
{
	int i;

	pthread_mutex_lock(&mutx); //임계 영역 시작
	for(i=0; i<clnt_cnt; i++) //현재 서버와 연결된 모든 클라이언트에게 전송받은 메시지를 전달 
		write(clnt_socks[i], msg, len);
	pthread_mutex_unlock(&mutx); //임계 영역 끝
}

int send_whisper_msg(char* msg, int len) // 1:1 통신
{
	int i;
	char* receiver, * text;
	char whisper_msg[NAME_SIZE+BUF_SIZE]; //수신자에게 전달된 최종 메시지를 담는 배열

	receiver = strtok(msg, " "); //메시지를 받을 클라이언트, @가 삭제되지 않고 저장
	receiver++; //receiver의 바로 앞 @ 삭제
	text = strtok(NULL, ""); //receiver에게 전송될 메시지의 내용

	pthread_mutex_lock(&mutx); //임계 영역 시작
	if (!strcmp(receiver, "all")) { //메시지를 받을 receiver가 all일 때
		sprintf(whisper_msg, "%s %s", "(all)", text);
		for (i = 0; i < clnt_cnt; i++) //모든 클라이언트에게 전송
			write(clnt_socks[i], whisper_msg, strlen(whisper_msg));
		pthread_mutex_unlock(&mutx); //임계 영역 끝
		return 0; //메소드 정상 종료
	}

	for (i = 0; i < clnt_cnt; i++) //clnt_names 전체를 돌며 receiver 찾기
		if (!strcmp(clnt_names[i], receiver)) { //clnt_names 배열 안에 receiver가 존재한다면
			sprintf(whisper_msg, "%s %s", "(whisper)", text);
			write(clnt_socks[i], whisper_msg, strlen(whisper_msg)); //해당 클라이언트에게 전송
			pthread_mutex_unlock(&mutx); //임계 영역 끝
			return 0; //메소드 정상 종료
		}
	
	pthread_mutex_unlock(&mutx); //임계 영역 끝
	return -1; //해당 receiver가 존재하지 않음. 메소드 비정상 종료
}


void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}