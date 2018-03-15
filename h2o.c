/*	IOS- Operacni systemy	*/
/*	2.projekt		*/
/*	autor: xcoufa08		*/

/* P O Z N A M K Y
 * smazat zbytecne pomocne vypisy
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#define ERR -1
// Globalni promenne
int cntA_get, *cntA_ctrl, cntH_get, *cntH_ctrl, cntO_get, *cntO_ctrl, cur_H_hlp, *cur_H, cur_O_hlp, *cur_O, cnt_all_hlp, *cnt_all,
bond3_hlp, *bond3, finished_hlp, *finished, AllBonded_hlp, *AllBonded;
int N, GH, GO, B, ID; //Pocet procesu kyslik/max. doba pro vodik/max. doba pro kyslik/ max. doba bond
pid_t pid_genH, pid_genO, pid_H, pid_O;
sem_t *s_write, *s_allInBond, *s_end, *s_allBonded, *s_bond, *s_alldone, *s_HReady, *s_OReady;

//definice funkci
void printErr(char *msg, int err);
void initSemaphores();
void initSharedMemory();
FILE *openFile();
void closeFile(FILE *fd);
void cleanSharedMemory();
void cleanSemaphores();
int isNumeric(char *str);
void oxygen(FILE* fd);
void hydrogen(FILE* fd);
void bond(char type, FILE* fd);
void printEvent(char jmeno, char* akce, FILE* fd);

/*Vypise chybovou zpravu na stderr a ukonci program s prislusnym cislem. Pokud je err 0,
 vypise chybu, ale program neukoncuje*/
void printErr(char *msg, int err){
  if(err != 0){
    fprintf(stderr, "%s\n", msg);
    exit(err);
  }
  else{
    fprintf(stderr, "%s\n", msg);
  }
}

/*Funkce namapuje a nainicializuje semafory, v pripade chyby korektne ukonci*/
void initSemaphores(){
  /*S e m a f o r y*/
  // Zapis
  s_write = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  if(s_write == MAP_FAILED){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error in mmap()!", 2);
  }
  if(sem_init(s_write, 1, 1) == ERR){	// init
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error inicializing semaphore!", 2);
  }
  // All In Bond
  s_allInBond = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  if(s_allInBond == MAP_FAILED){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error in mmap()!", 2);
  }
  if(sem_init(s_allInBond, 1, 0) == ERR){	// init
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error inicializing semaphore!", 2);
  }
  // Konec hlavniho procesu, ostatni jsou jiz ukonceny
  s_end = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  if(s_end == MAP_FAILED){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error in mmap()!", 2);
  }
  if(sem_init(s_end, 1, 0) == ERR){	// init
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error inicializing semaphore!", 2);
  }
  // AllBonded -> Vsechny 3 procesy dokoncily bonded
  s_allBonded = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  if(s_allBonded == MAP_FAILED){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error in mmap()!", 2);
  }
  if(sem_init(s_allBonded, 1, 1) == ERR){	// init
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error inicializing semaphore!", 2);
  }
  // Bond
  s_bond = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  if(s_bond == MAP_FAILED){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error in mmap()!", 2);
  }
  if(sem_init(s_bond, 1, 1) == ERR){	// init
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error inicializing semaphore!", 2);
  }
  // All DONE
  s_alldone = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  if(s_alldone == MAP_FAILED){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error in mmap()!", 2);
  }
  if(sem_init(s_alldone, 1, 0) == ERR){	// init
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error inicializing semaphore!", 2);
  }
  // H Ready
  s_HReady = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  if(s_HReady == MAP_FAILED){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error in mmap()!", 2);
  }
  if(sem_init(s_HReady, 1, 0) == ERR){	// init
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error inicializing semaphore!", 2);
  }
  // O Ready
  s_OReady = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  if(s_OReady == MAP_FAILED){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error in mmap()!", 2);
  }
  if(sem_init(s_OReady, 1, 0) == ERR){	// init
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error inicializing semaphore!", 2);
  }
}

/*Funkce inicializuje sdilenou pamet, v pripade chyby korektne ukonci program*/
void initSharedMemory(){
  /* S d i l e n a	p a m e t*/
  //poradove cislo 'cntA'
  cntA_get = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(cntA_get == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    cntA_ctrl = (int*) shmat(cntA_get, NULL, 0);
    if(cntA_ctrl == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  (*cntA_ctrl) = 1; //pocatecni hodnota
  //poradove cislo vodiku 'cntH'
  cntH_get = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(cntH_get == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    cntH_ctrl = (int*) shmat(cntH_get, NULL, 0);
    if(cntH_ctrl == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  (*cntH_ctrl) = 0; //pocatecni hodnota
  //poradove cislo kysliku 'cntO'
  cntO_get = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(cntO_get == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    cntO_ctrl = (int*) shmat(cntO_get, NULL, 0);
    if(cntO_ctrl == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  (*cntO_ctrl) = 0; //pocatecni hodnota
  //Soucasny pocet vodiku
  cur_H_hlp = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(cur_H_hlp == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    cur_H = (int*) shmat(cur_H_hlp, NULL, 0);
    if(cur_H == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  (*cur_H) = 0; //pocatecni hodnota
  //Soucasny pocek kysliku
  cur_O_hlp = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(cur_O_hlp == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    cur_O = (int*) shmat(cur_O_hlp, NULL, 0);
    if(cur_O == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  (*cur_O) = 0; //pocatecni hodnota
  //Cnt all -> pocet dobehlych cekajicich procesu
  cnt_all_hlp = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(cnt_all_hlp == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    cnt_all = (int*) shmat(cnt_all_hlp, NULL, 0);
    if(cnt_all == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  (*cnt_all) = 0; //pocatecni hodnota
  //BOND 3 ->cita aktualni pocet prvku ktere vypsaly bond
  bond3_hlp = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(bond3_hlp == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    bond3 = (int*) shmat(bond3_hlp, NULL, 0);
    if(bond3 == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  *bond3 = 0; //pocatecni hodnota
  //FINISHED -> cita dokoncene procesy
  finished_hlp = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(finished_hlp == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    finished = (int*) shmat(finished_hlp, NULL, 0);
    if(finished == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  (*finished) = 0; //pocatecni hodnota
  //ALL Bonded -> citac procesu ktere vypsaly bonded
  AllBonded_hlp = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
  if(AllBonded_hlp == ERR){
    printErr("Error while getting shared memory!",2);
  }
  else{
    AllBonded = (int*) shmat(AllBonded_hlp, NULL, 0);
    if(AllBonded == (void*)ERR){
      cleanSharedMemory();
      cleanSemaphores();
      printErr("Shared memory error!",2);
    }
  }
  (*AllBonded) = 0; //pocatecni hodnota
}

/*Otevre soubor pro vystup. v pripade chyby korektne ukonci program*/
FILE *openFile(){
  /*o t e v r e n i	s o u b o r u*/
  FILE *fd = fopen("h2o.out","a");
  if(fd == NULL){
    printErr("Error opening output file!", 2);
  }
  return fd;
}

/*Funkce zavre soubor popsany deskriptorem fd, v pripade chyby korektne ukonci porgram*/
void closeFile(FILE *fd){
  if(fclose(fd) != 0){
    cleanSharedMemory();
    cleanSemaphores();
    printErr("Error closing output file!", 2);
  }
}

/*Funkce uklidi vsechnu pouzitou sdilenou pamet, v pripade chyby korektne ukonci program*/
void cleanSharedMemory(){
  if(shmctl(cntA_get, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
  if(shmctl(cntH_get, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
  if(shmctl(cntO_get, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
  if(shmctl(cur_H_hlp, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
  if(shmctl(cur_O_hlp, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
  if(shmctl(cnt_all_hlp, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
  if(shmctl(bond3_hlp, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
  if(shmctl(finished_hlp, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
  if(shmctl(AllBonded_hlp, IPC_RMID, NULL) == ERR){
    printErr("Error cleaning shared memory!", 0);
  }
}

/*Funkce uvolni semafory, v pripade chyby korektne ukoni program*/
void cleanSemaphores(){
  if(sem_destroy(s_write) == ERR){
    printErr("Error cleanin semaphores!", 0);
  }
  if(sem_destroy(s_allInBond) == ERR){
    printErr("Error cleanin semaphores!", 0);
  }
  if(sem_destroy(s_end) == ERR){
    printErr("Error cleanin semaphores!", 0);
  }
  if(sem_destroy(s_allBonded) == ERR){
    printErr("Error cleanin semaphores!", 0);
  }
  if(sem_destroy(s_bond) == ERR){
    printErr("Error cleanin semaphores!", 0);
  }
  if(sem_destroy(s_alldone) == ERR){
    printErr("Error cleanin semaphores!", 0);
  }
  if(sem_destroy(s_HReady) == ERR){
    printErr("Error cleanin semaphores!", 0);
  }
  if(sem_destroy(s_OReady) == ERR){
    printErr("Error cleanin semaphores!", 0);
  }
}

/*Funkce urci zda je retezec ciselny, pokud ano, vraci 1, jinak 0*/
int isNumeric(char *str){
  while(*str != '\0')
  {
    if(isdigit(*str) == 0)
      return 0;
    str++;
  }
  return 1;
}

/*Simuluje proces vodiku, vypisuje prislusne radky na vystup*/
void hydrogen(FILE* fd){
  sem_wait(s_write);
  (*cntH_ctrl)++;
  ID = (*cntH_ctrl);
  printEvent('H', "started", fd);
  sem_post(s_write);
  sem_wait(s_bond);
  sem_wait(s_allBonded);
  sem_wait(s_write);
  if((*cur_H) >= 1 && (*cur_O) >= 1){//muze zacit bond
    printEvent('H', "ready", fd);
    (*cur_H)--;;
    (*cur_O)--;
    sem_post(s_write);
    sem_post(s_HReady);
    sem_post(s_OReady);
    bond('H', fd); 
    sem_wait(s_write);
    printEvent('H', "bonded", fd);
    (*AllBonded)++;
    if(*AllBonded == 3){
      sem_post(s_allBonded);
      *AllBonded = 0;
    }
    (*cnt_all)++;
    sem_post(s_write);
    sem_post(s_bond);
    while(*cnt_all < 3*N){
	//cekame na dokonceni vsech molekul
    }
    sem_wait(s_write);
    printEvent('H', "finished", fd);
    (*finished)++;
    sem_post(s_write);
  }
  else{//ceka na ostatni
    //sem_wait(s_write);
    printEvent('H', "waiting", fd);
    (*cur_H)++;
    sem_post(s_write);
    sem_post(s_bond);
    sem_post(s_allBonded);
    sem_wait(s_HReady);
    bond('H', fd);
    sem_wait(s_write);
    printEvent('H', "bonded", fd);
    (*AllBonded)++;
    if(*AllBonded == 3){
      sem_post(s_allBonded);
      *AllBonded = 0;
    }
    (*cnt_all)++;
    sem_post(s_write);
    while(*cnt_all < 3*N){
	//cekame na dokonceni vsech molekul
    }
    sem_wait(s_write);
    printEvent('H', "finished", fd);
    (*finished)++;
    sem_post(s_write);
  }
}

/*Simuluje proces kysliku, vypisuje prislusne radky na vystup*/
void oxygen(FILE* fd){
  sem_wait(s_write);
  (*cntO_ctrl)++;
  ID = (*cntO_ctrl);
  printEvent('O', "started", fd);
  sem_post(s_write);
  sem_wait(s_bond);
  sem_wait(s_allBonded);
  sem_wait(s_write);
  if((*cur_H) >= 2 && (*cur_O) >= 0){//muze zacit bond
    printEvent('O', "ready", fd);
    (*cur_H) = (*cur_H) -2;
    sem_post(s_write);
    sem_post(s_HReady);
    sem_post(s_HReady);
    bond('O', fd); 
    sem_wait(s_write);
    printEvent('O', "bonded", fd);
    (*AllBonded)++;
    if(*AllBonded == 3){
      sem_post(s_allBonded);
      *AllBonded = 0;
    }
    (*cnt_all)++;
    sem_post(s_write);
    sem_post(s_bond);
    while(*cnt_all < 3*N){
	//cekame na dokonceni vsech molekul
    }
    sem_wait(s_write);
    printEvent('O', "finished", fd);
    (*finished)++;
    sem_post(s_write);
  }
  else{//ceka na ostatni
    //sem_wait(s_write);
    printEvent('O', "waiting", fd);
    (*cur_O)++;
    sem_post(s_write);
    sem_post(s_bond);
    sem_post(s_allBonded);
    sem_wait(s_OReady);
    bond('O', fd);
    sem_wait(s_write);
    printEvent('O', "bonded", fd);
    (*AllBonded)++;
    if(*AllBonded == 3){
      sem_post(s_allBonded);
      *AllBonded = 0;
    }
    (*cnt_all)++;
    sem_post(s_write);
    while(*cnt_all < 3*N){
	//cekame na dokonceni vsech molekul
    }
    sem_wait(s_write);
    printEvent('O', "finished", fd);
    (*finished)++;
    sem_post(s_write);
  }
}

/*Simuluje proces spojeni atomu do molekuly, vypisuje prislusne radky na vystup*/
void bond(char type, FILE* fd){
  sem_wait(s_write);
  printEvent(type, "begin bonding", fd);
  (*bond3)++;
  //if(type == 'H') (*cur_H)--;
  //else (*cur_O)--;
  if(*bond3 == 3){//Porovnat se sdilenou promennou bond3
    sem_post(s_allInBond);
    sem_post(s_allInBond);
    sem_post(s_allInBond);
    (*bond3) = 0;
  }
  sem_post(s_write);
  //uspani
  if(B != 0){
    int sleep_time = (random()%B + 1) * 1000;
    usleep(sleep_time);
  }
  sem_wait(s_allInBond);
}

/*Funkce vytiskne na pozadovany vystup korektni radek, dany pocitadly akci*/
void printEvent(char jmeno, char* akce, FILE* fd){
  fd = openFile();
  //kontrolni vypis na stdout
  //printf("%d:\t%c %d:\t%s\n", *cntA_ctrl, jmeno, ID, akce);//SMAZAT, slouzi jako kontrola
  fprintf(fd, "%d:\t%c %d:\t%s\n", *cntA_ctrl, jmeno, ID, akce);
  (*cntA_ctrl)++;
  closeFile(fd);
}

/************************/
/*	M A I N 	*/
/************************/
int main(int argc, char **argv){
  
  int sleep_time, sleep_time2;
  FILE *fd = NULL;
  srand(time(0)); //pro lepsi nahodna cisla
  //Nacteni a kontrola argumentu
  if(argc != 5){
    printErr("Bad number of arguments!", 1);
  }
  N = strtol(argv[1], NULL, 10);
  if(isNumeric(argv[1]) == 0){
    printErr("First argument not even number!", 1);
  }
  GH = strtol(argv[2], NULL, 10);
  if(isNumeric(argv[2]) == 0){
    printErr("Second argument not even number!", 1);
  }
  GO = strtol(argv[3], NULL, 10);
  if(isNumeric(argv[3]) == 0){
    printErr("Third argument not even number!", 1);
  }
  B = strtol(argv[4], NULL, 10);
  if(isNumeric(argv[4]) == 0){
    printErr("Fourth argument not even number!", 1);
  }
  
  if(N <= 0){
    printErr("Invalid number in first argument!", 1);
  }
  else if(GH < 0 || GH >= 5001){
    printErr("Invalid number in second argument!", 1);
  }
  else if(GO < 0 || GO >= 5001){
    printErr("Invalid number in third argument!", 1);
  }
  else if(B < 0 || B >= 5001){
    printErr("Invalid number in fourth argument!", 1);
  }
/****************************************************************/
  //Smazani predchozich dat souboru
  fd = fopen("h2o.out","w");
  if(fd == NULL){
    printErr("Error opening output file!", 2);
  }
  fclose(fd);
  fd = NULL;
  //Inicializace sdilene pameti a semaforu
  initSharedMemory();
  initSemaphores();

  //Spusteni procesu vodiku a kysliku
  pid_genH = fork();
  //generujeme vodik
  if(pid_genH == 0){//v procesu generujicim vodik
    for(int i = 1; i <= 2 * N; i++){
      pid_H = fork();
      if(pid_H == 0){//Proces jednotlivych vodiku
	if(GH == 0){
	  sleep_time = 0;
	}
	else{
	  sleep_time = (random()%GH + 1) * 1000;
	  usleep(sleep_time);
	}
	
	hydrogen(fd);// F U N K C E	H Y D R O G E N
	if(*finished == 3*N){
	  sem_post(s_alldone);
	  sem_post(s_alldone);
	  sem_post(s_alldone);
	}
	
	kill(getpid(), SIGTERM);
	//sem_wait(s_alldone);
      }
      else if(pid_H > 0){//Proces generujici vodik
	//neni treba
      }
      else if(pid_H < 0){
	cleanSharedMemory();
	cleanSemaphores();
	kill(pid_H, SIGTERM);
	kill(getppid(), SIGTERM);
	printErr("Error in fork()!", 2);
      }
    }
    sem_wait(s_alldone);
    sem_post(s_end);
    kill(getpid(), SIGTERM);
  }
  //generujeme kyslik
  else if(pid_genH > 0){//v hlavnim procesu
    pid_genO = fork();
    
    if(pid_genO == 0){//v procesu generujicim kyslik
      for(int i = 1; i <= N; i++){
	pid_O = fork();
	if(pid_O == 0){//v procesu jednotlivych kysliku
	  if(GO == 0){
	    sleep_time2 = 0;
	  }
	  else{
	    sleep_time2 = (random()%GO + 1) * 1000;
	    usleep(sleep_time2);
	  }
	  oxygen(fd); // F U N K C E	O X Y G E N
	  if(*finished == 3*N){
	    sem_post(s_alldone);
	    sem_post(s_alldone);
	    sem_post(s_alldone);
	  }
	  
	  kill(getpid(), SIGTERM);
	  //sem_wait(s_alldone);
	}
	else if(pid_O > 0){//V procesu generujicim kyslik
	  //neni treba
	}
	else if(pid_H < 0){
	  cleanSharedMemory();
	  cleanSemaphores();
	  kill(pid_O, SIGTERM);
	  kill(getppid(), SIGTERM);
	  printErr("Error in fork()!", 2);
	}
      }
      sem_wait(s_alldone);
      sem_post(s_end);
      kill(getpid(), SIGTERM);
    }
    else if(pid_genO > 0){//v hlavnim procesu
      sem_wait(s_alldone);
    }
    else if(pid_genO < 0){//err
      cleanSharedMemory();
      cleanSemaphores();
      kill(pid_genO, SIGTERM);
      printErr("Error in fork()!", 2);
    }
  }
  //ERROR
  else if(pid_genH < 0){
    cleanSharedMemory();
    cleanSemaphores();
    kill(pid_genH, SIGTERM);
    printErr("Error in fork()!", 2);
  }
  //cekam na ukonceni vsech generujicich procesu
  sem_wait(s_end);
  sem_wait(s_end);
  //Uklid sdilene pameti a semaforu
  cleanSharedMemory();
  cleanSemaphores();
  //Zavreni souboru
  //closeFile(fd);
  return 0;
}