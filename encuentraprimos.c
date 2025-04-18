
//TRABAJO REALIZADO POR MARINA GARCIA Y PAULA MÉNDEZ
//  Empezamos el: 12_12_2022

//INCLUDES -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

//DEFINES ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define LONGITUD_MSG 100           // Payload del mensaje
#define LONGITUD_MSG_ERR 200       // Mensajes de error por pantalla

// codigos de exit por error
#define ERR_ENTRADA_ERRONEA 2
#define ERR_SEND 3
#define ERR_RECV 4
#define ERR_FSAL 5

#define NOMBRE_FICH "primos.txt"
#define NOMBRE_FICH_CUENTA "cuentaprimos.txt"
#define CADA_CUANTOS_ESCRIBO 5

// rango de busqueda, desde BASE a BASE+RANGO
#define BASE 800000000
#define RANGO 2000

// Intervalo del temporizador para RAIZ
#define INTERVALO_TIMER 5

// CÃ³digos de mensaje para el campo mesg_type del tipo T_MESG_BUFFER 
#define COD_ESTOY_AQUI 5           // Un calculador indica al SERVER que esta preparado
#define COD_LIMITES 4              // Mensaje del SERVER al calculador indicando los limites de operacion
#define COD_RESULTADOS 6           // Localizado un primo
#define COD_FIN 7                  // Final del procesamiento de un calculador

// Mensaje que se intercambia ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    long mesg_type;
    char mesg_text[LONGITUD_MSG];
} T_MESG_BUFFER;

//PROTOTIPOS DE LAS FUNCIONES ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int Comprobarsiesprimo(long int numero);
void Informar(char *texto, int verboso);
void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos);
int ContarLineas();
static void alarmHandler(int signo);

int cuentasegs;                   // Variable para el computo del tiempo total


//MAIN ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char* argv[]){
	int i,j;
	long int numero;
	long int numprimrec;
    long int nbase;
    int nrango;
    int nfin;
    time_t tstart,tend; 
	
	key_t key;
    int msgid;    
    int pid, pidservidor, pidraiz, parentpid, mypid, pidcalc;
    int *pidhijos;
    int intervalo,inicuenta;
    int verbosity;
    T_MESG_BUFFER message;
    char info[LONGITUD_MSG_ERR];
    FILE *fsal, *fc;
    int numhijos;
	int numprimos = 0; 		//Variable para contar el numero de primos encontrados

    // Control de entrada, depsues del nombre del script debe figurar el numero de hijos y el parámetro verbosity
    //numhijos = 2;     // SOLO para el esqueleto, en el proceso  definitivo vendra por la entrada
	if(argc != 3){

        printf("ERROR, deberia de haber otros parametros");
    }else {
		/* Comprobacion que coge bien el argc y argv
        for(int i=0; i<argc ; i++){
           printf("argv[%d] : %d\n ", i,atoi(argv[i]));
        }
		*/
    }
	numhijos=atoi(argv[1]);
	verbosity=atoi(argv[2]);
	
    pid=fork();       // Creacion del SERVER
    
	// Rama del hijo de RAIZ (SERVER)
    if (pid == 0){     
    
		pid = getpid();
		pidservidor = pid;
		mypid = pidservidor;	   
		
		// peticion de clave para crear la cola
		if ( ( key = ftok( "/tmp", 'C' ) ) == -1 ) {
		  perror( "Fallo al pedir ftok" );
		  exit( 1 );
		}
		
		printf( "Server: System V IPC key = %u\n", key );

        // creacion de la cola de mensajeria
		if ( ( msgid = msgget( key, IPC_CREAT | 0666 ) ) == -1 ) {
		  perror( "Fallo al crear la cola de mensajes" );
		  exit( 2 );
		}
		printf("Server: Message queue id = %u\n", msgid );

        i = 0;
        // Creacion de los procesos CALCuladores
		while(i < numhijos) {
		 if (pid > 0) { // Solo SERVER creara hijos
			 pid=fork(); 
			 if (pid == 0){ 
			      // Rama hijo
				parentpid = getppid();
				mypid = getpid();
			   }
			 }
		 i++;  // n de hijos creados
		}

        // AQUI VA LA LOGICA DE NEGOCIO DE CADA CALCulador. 
		if (mypid != pidservidor){

			message.mesg_type = COD_ESTOY_AQUI;
			//manda el mensaje "COD_ESTOY_AQUI" e imprime su PID
			sprintf(message.mesg_text,"%d",mypid);
			
			//Para enviar o recibir mensajes de una cola de mensajes
		
			msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);
			//RECIBIMOS EL COD LIMITE
			msgrcv(msgid, &message, sizeof(message), COD_LIMITES, 0);
			sscanf(message.mesg_text,"%ld %d",&nbase,&intervalo);//guardamos la base para cada hijo
			printf("Me ha enviado una base el servidor\n");
			sleep(1);
			
			//Calcular numeros primos, y enviar mensaje al server si se encuentra un primo
			for(numero=nbase;numero<nbase+intervalo;numero++){//nrango=1000 nfin= 4000
				
				if (Comprobarsiesprimo(numero)){
					message.mesg_type = COD_RESULTADOS;
					sprintf(message.mesg_text,"%d:  %ld", mypid, numero);
					msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
				}
			}
			
			//Envio del mensaje de fin de busqueda
			message.mesg_type = COD_FIN;
			sprintf(message.mesg_text,"%d", mypid);
			msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);
		
			// Un monton de codigo por escribir
		  //	sleep(60); // Esto es solo para que el esqueleto no muera de inmediato, quitar en el definitivo

			exit(0);
		}
		// SERVER
		else{ 
		  // Pide memoria dinamica para crear la lista de pids de los hijos CALCuladores
		  pidhijos = (int*)malloc(numhijos*sizeof(int));
		  
		  //Recepcion de los mensajes COD_ESTOY_AQUI de los hijos
		  for (j=0; j <numhijos; j++){
			  msgrcv(msgid, &message, sizeof(message), 0, 0);
			  sscanf(message.mesg_text,"%d",&pidhijos[j]); // Tendras que guardar esa pid
			  printf("\nMe ha enviado un mensaje el hijo %d \n",pidhijos[j]);
            }
		  
		  //Imprimir la Jerarquia de procesos
		  pidraiz = getppid();
		  Imprimirjerarquiaproc(pidraiz,pidservidor, pidhijos, numhijos);
		  
		  
		  //Envia a los hijos el intervalo en el que deben de buscar
			inicuenta = BASE;
			intervalo = (int)(RANGO/numhijos);
	
			for(int j = 0; j < numhijos; j++){
				message.mesg_type = COD_LIMITES;
				sprintf(message.mesg_text, "%d %d", inicuenta, intervalo);
				msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
				inicuenta += intervalo;
			}
			//sleep(60); // Esto es solo para que el esqueleto no muera de inmediato, quitar en el definitivo

		  //Crear el fichero de resultados primos.txt
		  
		    fsal = fopen(NOMBRE_FICH, "w");
			nfin = 0;
		  
			if (fsal == NULL){
				perror("Fallo al crear el fichero de salida");
				exit(ERR_FSAL);
			}
			
			nfin = 0;
			while(nfin < numhijos) {
				if(msgrcv(msgid, &message, sizeof(message), 0, 0)) {
					if(message.mesg_type == COD_RESULTADOS) {
						
						Informar(message.mesg_text, verbosity);
						
						sscanf(message.mesg_text,"%d:  %ld", &pid, &numprimrec);
						fprintf(fsal, "%ld\n", numprimrec);
						numprimos++;
						if(numprimos%CADA_CUANTOS_ESCRIBO == 0 && numprimos != 0) {
							fc = fopen(NOMBRE_FICH_CUENTA, "w");
							fprintf(fc, "%d\n", numprimos);
							
							//Cerrar fichero
							fclose(fc);
						}
					}
					else if(message.mesg_type == COD_FIN) {
						nfin++;
						printf("El proceso %s ha terminado\n", message.mesg_text);
					}
				}
				
			}
			//Cierro fichero
			fclose(fsal);
			printf("\nCalculos terminados\n");
			time(&tend);
			printf("Números encontrados: %d.\n", numprimos);
			printf("Tiempo total del calculo: %.2f segundos.\n", difftime(tend, tstart));
			msgctl(msgid, IPC_RMID, NULL); // Borrar la cola de mensajería, muy importante
			
		  
			
		// Mucho codigo con la logica de negocio de SERVER
		  
		  // Borrar la cola de mensajeria, muy importante. No olvides cerrar los ficheros
		  msgctl(msgid,IPC_RMID,NULL);
		  
	   }
    }else{
	  // Rama de RAIZ, proceso primigenio
	  
	  cuentasegs = 0;
	  
      alarm(INTERVALO_TIMER);
      signal(SIGALRM, alarmHandler);
	  
	  wait(NULL); 	//Espera al final del server
	  printf("RESULTADO: Se han encontrado %d primos \n", ContarLineas());
	  
	  //Liberar memoria
	  free(pidhijos);
	
    }
}

// Manejador de la alarma en el RAIZ --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void alarmHandler(int signo){
//...
	FILE* fc;
	
	int nprim;
	cuentasegs = cuentasegs + INTERVALO_TIMER;
	
	fc = fopen(NOMBRE_FICH_CUENTA, "r" );
	
	if (fc != NULL){
		
		fscanf(fc, "%d", &nprim);
		fclose(fc);
		printf("%02d (segs): %d primos encontrados \n", cuentasegs, nprim);
		
	}else{
		printf("%02d (segs) \n", cuentasegs);
		alarm(INTERVALO_TIMER);
	}
}


//COMPROBAR SI ES PRIMO ---------------------------------------------------------------------------------------------------------------------------
//Esta hecha pero no termina de funcionar bien ya que falta implementar la division por intervalos de los hijos
int Comprobarsiesprimo(long int numero){
	
	if (numero < 2){
		return 0; // Por convenio 0 y 1 no son primos ni compuestos
	
		}else{
			for (int x = 2; x <= (numero / 2) ; x++){
				if (numero % x == 0) {
					return 0;
				}
			}
			return 1;
		}
}


//FUNCION CONTAR LINEAS DE PRIMOS.TXT ------------------------------------------------------------------------------------------------------------------------------------------------------------------
int ContarLineas(){
   FILE *fsal;
    int contador = 0;  
    char c;    
  
    // abrir fichero
    fsal = fopen(NOMBRE_FICH, "r"); 
  
    // Comprobar si existe
	while(feof(fsal) == 0){
		c = getc(fsal);
		if(c=='\n'){
			contador++;
		}
	}
	fclose(fsal);

	return contador;
}

//IMPRIMIR LA JERARQUIA --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos){
	
	printf("\nLa Jerarquia es: ");
	printf("\nRAIZ \t\t SERV \t\t CALC\n");
	printf("%d \t\t %d  \t\t %d \n ", pidraiz, pidservidor, pidhijos[0]); // Imprimes por primera vez raiz, serv y el primer hijo (0)

	for(int i=1; i<numhijos; i++){ // Desde 1 (incluido) imprimes los hijos
		printf("\t\t\t\t %d\n", pidhijos[i]);
	}
	
	printf("\n");
}

// INFORMAR -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Informar(char *texto, int verboso){
	FILE *fsal;

	int prim;
	prim=*(int*)texto;
	  
    if(verboso == 1){
        fsal = fopen(NOMBRE_FICH,"r");

        while(feof(fsal) == 0){
            fscanf(fsal, "%d\n", &prim);
            printf("%d", prim);
        }
        fclose(fsal);
    }
}
