#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#define NROTEADORES 7 // Quantidade de Roteadores 
#define MENSAGEM_SIZE 100 // Quantidade de caracteres em uma mensagem
#define NUM_THR 4

typedef struct{//estrutura que cria um roteador para a rede
	int id, porta;
	char ip[30];
}roteador;

typedef struct{//estrutura que cria uma mensagem enviada/recebida por um roteador
	int origem, destino, tipo;//origem, destino, flag de tipo de msg pedido no enunciado
	char msg[MENSAGEM_SIZE];
	int vetordist[NROTEADORES];
}mensagem;

void die(char *s);//imprime erros, caso tiver né
void criar_roteador(int ID);//cria roteador
void criavetor();
void criatabela();
void * recebe(void * dado);
void * envia_vetor(void * dado);
void envia();
void copia_vetor_para_tabela(int id, int vetdist[NROTEADORES]);
int achaNext(int destiny);
void send_next(int next);
void update_vector_vizinhos();
void update_vector();
void imprime_tabela_roteamento();
void send_next_vetor(int next);


// variavel globais
int ID, op, x, sock;
int tabelaroteamento[NROTEADORES][NROTEADORES];
int vetordistancia[NROTEADORES];
int vetorvizinhos[NROTEADORES];

mensagem temp_msg;
mensagem caixa_msg[100], caixa_env[100]; //filas do roteador
int msg_control = 0;
mensagem caixa_env[100];
int msg_env_control = 0;
mensagem temp_rota; // utilizada para enviar o vetor dist para os vizinhos

roteador roteadores[NROTEADORES]; // vetor de roteadores, usada apenas o ID, mas instancia um vetor para facilitar a leitura do arquivo

struct sockaddr_in si_me, si_other;
pthread_t recebe_id, envia_id;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


void die(char *s){
  perror(s);
  // exit(1);
}

void envia(){ //  funcao para enviar mensagens, faz busca e fornecendo a rota para o pacote

  int dest, next;

  printf("Digite o roteador destino\n");
  scanf("%d", &dest); // destino da mensagem

  if (dest > 5 || dest < 0){
    die("Esse roteador nao existe\n"); // verifica e trata erro na inserção do destino
    op = -1;
  }

  printf("Escreva a mensagem que quer enviar para %d\n", dest);
  getchar();
  fgets(caixa_env[msg_env_control].msg, MENSAGEM_SIZE, stdin); // mensagem no vetor de msgs_env
  caixa_env[msg_env_control].origem = ID;
  caixa_env[msg_env_control].destino = dest;
  caixa_env[msg_env_control].tipo = 0;
  // até aqui percebemos que pega destino, pega msg que sera enviada

  
  printf("enviando para %d atraves de %d\n", dest, achaNext(dest));
  // adicionando função de roteamento aqui, que gera o proximo a ser roteado
  sleep(1);
  send_next(achaNext(dest));

  op = -1;
  msg_env_control++;
}

int achaNext(int destiny) // função de roteamento 
{
  if (destiny > ID)
  {
    int i = 0;
    while(i < NROTEADORES){
      if (tabelaroteamento[i][destiny] + tabelaroteamento[ID][i] == vetordistancia[destiny] && i != ID && i != destiny)
      {
        return i;
      }

      i+=1;
    }
  }
  
  if (ID > destiny){
    int i = NROTEADORES;
    while( i != 0)
    {
      if (tabelaroteamento[i][destiny] + tabelaroteamento[ID][i] == vetordistancia[destiny] && i != ID && i != destiny)
      {
        return i;
      }

      i-=1;
    }
  }

  return destiny;
}

void send_next(int next)
{

  pthread_mutex_lock(&mutex);

  printf("Enviando mensagem para %d\n", next);
  sleep(2);

  si_other.sin_port = htons(roteadores[next].porta);
  if (inet_aton(roteadores[next].ip, &si_other.sin_addr) == 0)
  { // se tiver erro no endereço
    die("Falha ao obter ip do destino\n");
  }
  else
  {
    if (sendto(sock, &caixa_env[msg_env_control], sizeof(caixa_env[msg_env_control]), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1)
    {
      die("Falha no envio da mensagem\n"); // se tiver erro na mensagem
    }
    else
    {
      printf("Roteador %d enviando Mensagem para %d\n", ID, roteadores[next].id);
      printf("______________________________________________\n");
    }
  }

  pthread_mutex_unlock(&mutex);
}



void copia_vetor_para_tabela(int id, int vetdist[NROTEADORES]){ 
// passa o vetor distancia do roteador para a tabela de roteamento

  int i =0;

  while(i < NROTEADORES) {
    tabelaroteamento[id][i] = vetdist[i];
    i +=1;
  }
}

void update_vector_vizinhos(){

  int i=0;

  while(i < NROTEADORES){
    if (vetordistancia[i] >= 0 && vetordistancia[i] < 100){
      vetorvizinhos[i] = i;
    }

    i +=1;
  }
  vetorvizinhos[ID] = 0;
}

void imprime_tabela_roteamento(){

  int  j=0;
  while(j <NROTEADORES) //j+=1;
  {

    for(int i=0 ; i<NROTEADORES; i++){
      printf("%d\t", tabelaroteamento[j][i]);
    }
    printf("\n");
    
    j+=1;
  }

  sleep(10);
  op = -1;
}


void *recebe(void *dado){ 
// recebe mensagens e retransmite se necessário

  int slen = sizeof(si_other); // si_me
  int next, i;

  while (1)
  {
    if ((recvfrom(sock, &temp_msg, sizeof(temp_msg), 0, (struct sockaddr *)&si_me, &slen)) == -1)
    {
      printf("Erro ao receber mensagem\n");
    }

    if (temp_msg.tipo == 0)
    { // zero é tipo mensagem

      if (temp_msg.destino == ID)
      { // ve se é pra mim
        printf("Mensagem recebida de %d\n", temp_msg.origem);
        caixa_msg[msg_control] = temp_msg; // ------ fila
        msg_control++;
        // confirmar aqui
        
        printf("Enviando confirmação para %d!\n", temp_msg.origem);
        caixa_env[msg_env_control] = temp_msg;
        caixa_env[msg_env_control].tipo = 2;
        caixa_env[msg_env_control].origem = ID;
        caixa_env[msg_env_control].destino = temp_msg.origem;
        send_next(achaNext(caixa_env[msg_env_control].destino));
        msg_env_control++;

        
      }
      else
      { 
        
        // reenviar para o proximo sendnext
        caixa_env[msg_env_control] = temp_msg; // ------ fila 
        send_next(achaNext(caixa_env[msg_env_control].destino));
        msg_env_control++;
      }
    }
    if (temp_msg.tipo == 1)
    { 
      // tipo atualização de rota

      copia_vetor_para_tabela(temp_msg.origem, temp_msg.vetordist); // passa o vetor para a tabela de roteamento
      update_vector();
    }
    if (temp_msg.tipo == 2)
    { // tipo confirmação

      if (temp_msg.destino == ID)
      { // ve se a confirmação é pra mim

        printf("Confirmado. %d recebeu a sua mensagem!\n", temp_msg.origem);
        printf("______________________________________________\n");
      }
      else
      { // senao repassa a confirmação
        sleep(2);
        printf("Enviando confirmação!\n");
        caixa_env[msg_env_control] = temp_msg;
        send_next(achaNext(caixa_env[msg_env_control].destino));
        msg_env_control++;
      }
    }
  }
}

void update_vector()
{

  pthread_mutex_lock(&mutex);
  int custo, i, j;
  i = 0;
  while(i<NROTEADORES){

    custo = vetordistancia[i];
    if (i != ID)
    {
      for (j = 0; j < NROTEADORES; j++)
      {
        if (j != ID && (custo > (tabelaroteamento[j][i] + tabelaroteamento[ID][j])))
        {
          custo = (tabelaroteamento[j][i] + tabelaroteamento[ID][j]);
        }
      }
      vetordistancia[i] = custo;
    }
    i +=1;
  }

  copia_vetor_para_tabela(ID, vetordistancia);
  pthread_mutex_unlock(&mutex);
}

void *envia_vetor(void *dado){

  int i;

  while (1){

    sleep(10);

    pthread_mutex_lock(&mutex);

    temp_rota.origem = ID;
    temp_rota.tipo = 1; // tippo att de rota

    i =0;
    while(i < NROTEADORES)
    {
      temp_rota.vetordist[i] = vetordistancia[i];
      i += 1;
    }

    i=0;
    while(i< NROTEADORES)
    {
      if (vetorvizinhos[i] >= 0)
      { // escolhe um vizinho válido
        send_next_vetor(i);
      }
      i+=1;
    }

    pthread_mutex_unlock(&mutex);
  }
}

void send_next_vetor(int next){ // manda o vetor de distancias para os vizinhos

  si_other.sin_port = htons(roteadores[next].porta);
  
  if (inet_aton(roteadores[next].ip, &si_other.sin_addr) == 0)
  { // se tiver erro no endereço
    die("Falha ao obter ip do destino\n");
  }
  else
  {
    if (sendto(sock, &temp_rota, sizeof(temp_rota), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1)
    {
      die("Falha no envio da mensagem\n"); // se tiver erro na mensagem
    }
  }
}


// funcao que cria a primeira vez o vetor distancia do roteador
void criavetor(){
  int x, y, i=0, custo;
  // inicia os vetores dis e de vetorvizinhos
  //i =0;
  while(i<NROTEADORES)
  { // inicializa os vetores com -1
    vetorvizinhos[i] = -1;
    vetordistancia[i] = 1000;
    i +=1;
  }
  // vetorvizinhos[ID]=0;//seta os valores para mim mesmo como zero
  vetordistancia[ID] = 0;

  // atribuindo os custos dos vizinhos par ao vet distancia

  FILE *p = fopen("src/enlaces.config", "r"); // abre o arquivo enlaces em modo leitura
  if (!p)
  {
    die("Erro ao abrir arquivo enlaces.config\n");
  }
  while (fscanf(p, "%d %d %d\n", &x, &y, &custo) != EOF){ 
    // enquanto a leitura do arquivo não chega ao fim
    if (x == ID)
    {
      vetordistancia[y] = custo;
    }
    if (y == ID)
    {
      vetordistancia[x] = custo;
    }
  }
  fclose(p);
}

void criar_roteador(int ID){ 
// insrancia roteador, cria socket e faz bind com a porta

  int i, id;

  id = ID;
  // abre o arquivo e carrega as informações do roteador
  FILE *r = fopen("src/roteador.config", "r");
  if (!r)
    die("Erro ao abrir o arquivo roteadores.config\n");
  
  for (i = 0; fscanf(r, "%d %d %s\n", &roteadores[i].id, &roteadores[i].porta, roteadores[i].ip) != EOF; i++);
  // le arquivo de configuração dos roteadores, e atribui para todos os ids, usa apena o id informado pelo terminal
  printf("%d %d %s\n", roteadores[id].id, roteadores[id].porta, roteadores[id].ip);
  fclose(r);

  // Cria o socket
  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
    die("Erro ao criar Socket\n");
  }
  // Zera a estrutura
  memset((char *)&si_me, 0, sizeof(si_me));

  si_me.sin_family = AF_INET;                   // Familia
  si_me.sin_port = htons(roteadores[id].porta); // Porta em ordem de bytes de rede
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);    // Atribui o ip, qualquer ip aqui, sera substituido

  // bind socket a porta
  if (bind(sock, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
  {
    die("erro no bind");
  }
}

void criatabela(){

  for (int i = 0; i < NROTEADORES; i++)
  { // prenche a tabela com infinito
    for (int j = 0; j < NROTEADORES; j++)
    {
      tabelaroteamento[i][j] = 1000; // value infinito
    }
  }

  for (int i = 0, j = 0; i < NROTEADORES; i++, j++){   

    // zera a diagonal principal
    tabelaroteamento[i][j] = 0; // custos pra vc mesmo é zero
  }

  copia_vetor_para_tabela(ID, vetordistancia); // chama essa função para copiar o vd para a tabela
}

/******________________ initial ________________ ******/
int main(int argc, char const *argv[]){
  ID = atoi(argv[1]); // converte o parametro de entrada em um inteiro e passa para id


  if (ID < 0 || ID >= NROTEADORES) // trata erros no parametro do teclado
  {
    die("Numero de roteador invalido\n"); // garante que o id do roteador seja de 0 a 5
  }
  if (argc < 2)
  {
    die("insira o ID do roteador\n"); // se não for passado o ID
  }
  else if (argc > 2)
  {
    die("insira apenas o ID do roteador 'de 0 a 5'"); // se for passado mais que 1 parametro
  }
  
  memset((char *)&si_other, 0, sizeof(si_other)); // zera a estrutura
  si_other.sin_family = AF_INET;                  // Familia ipv4
  si_other.sin_addr.s_addr = htonl(INADDR_ANY);   // Atribui o ip

  criar_roteador(ID);  
  for(int i = 0; i < NUM_THR; i++)                               // carrega insformações do arquivo,(id, porta ,ip) cria socket e faz bind do socket com a porta
    pthread_create(&recebe_id, NULL, recebe, NULL);     // Cria a thread receptora
    pthread_create(&envia_id, NULL, envia_vetor, NULL); // Cria a thread que envia
    /* sleep(1); */

  //sleep(8);
  criavetor();  // cria o vetor distancia com os dados do arquivo enlaces.config
  criatabela(); // instancia a tabela com valores 0 na digonal e 1000 no resto
  update_vector_vizinhos();

  //sleep(5);
  while (1){ // loop menu, usado pra mandar e ler mensagens

    system("clear");
    printf("______________________________________________\n");
    printf("\t\tROTEADOR %d\n", ID);
    printf("1 - Checar Mensagens\n");
    printf("2 - Enviar Mensagem\n");
    printf("3 - Imprimir tabela\n");
    printf("4 - Limpar\n");
    printf("5 - Sair\n");
    printf("______________________________________________\n");
    scanf("%d", &op);
    
    switch (op){
      
      case 1:
        x = 0;
        while( x <= msg_control)
        {
          if (x < msg_control)
          {
            printf("Mensagem recebida de %d\n", caixa_msg[x].origem);
          }
          printf("%s\n", caixa_msg[x].msg);

          x+=1;
        }
        sleep(10);
        op = -1;
        break;
      
      case 2:
        envia(ID);
        break;
      
      case 3:
        imprime_tabela_roteamento();
        break;
      
      case 4:
        op = -1;
        break;

      case 5:
        exit(1);
        break;
      
      default:
        op = -1;
        break;
    }  
  }

  return 0;
}