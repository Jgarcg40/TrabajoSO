/* Librerias necesarias */
# include <stdio.h>
# include <stdlib.h>
# include <ctype.h>
# include <time.h>
# include <pthread.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>
# include <errno.h>

//Flags para determinar el estado de ejecución del programa
#define PROGRAMA_EJECUCCION 	0
#define PROGRAMA_FINALIZA		1

//Flags para determinar el tipo de recepcionista
#define RECEPCIONISTA_NORMAL 	1
#define RECEPCIONISTA_VIP 		2
#define AUTOCHEKIN				3

//Flags para determinar el tipo de cliente
#define CLIENTE_NORMAL	1
#define CLIENTE_VIP		2

//Flags para determinar el estado de atención del cliente
#define NO_ATENDIDO 			0	
#define ATENDIENDO 				1		
#define ATENDIDO				2
//Flags para determinar la serologia del cliente			
#define SEROLOGIA_CORRECTA 1
#define SEROLOGIA_INCORRECTA		2


//Funciones
void writeLogMessage(char *id, char *msg);
int calculaAleatorios(int min, int max);
void nuevoCliente(int s);
void expulsarCliente(int posicion);
void finalizarAplicacion(int s);
void *accionesCliente(void *ptr);
void *accionesRecepcionista(void *ptr);
int buscarSolicitud(int tipo);
void incrementaMaquinasChecking(int s);

//Declaración del archivo de log
FILE *logFile;
char logFileName[] = "registroTiempos.log";

//Arrays auxiliares para la escritura del log
char tipoCliente[][20] = {"Normal", "VIP"};
char tipoRecepcionista[][20] = {"Normal", "VIP"};

int contadorClientes;	//Contador de clientes actuales en la recepción
int totalClientes;	//Contador de clientes totales que han pasado por el hotel
int clientesAscensor;	//Contador de clientes en el ascensor
int finPrograma;	//Variable para denotar la finalización del programa. Se utilizan los define anteriores.
int ascensorLleno;	//Variable para denotar si el ascensor está lleno. 1 = lleno / 0 = libre.
int funcionando = 1; //variable para indicar que el programa está funcionando

//Mutex
pthread_mutex_t mutexLog; 
pthread_mutex_t mutexColaClientes;
pthread_mutex_t mutexAscensor;
pthread_mutex_t mutexMaquinas;

//Condicionales
pthread_cond_t ascensorFin;

//Hilos
pthread_t *recepcionistas;
pthread_t *maquinasAutochekin;

typedef struct clientes {
	int id;
	int tipo;
	int atendido;
	int serologia;
	int ascensor;
	pthread_t hilo;
} clientes;

clientes *cola;

int atencionMaxClientes;
int totalRecepcionistasVIP;
int totalRecepcionistas;
int totalMaquinasChecking;
int *maquinasChecking;

/* Funcion principal del programa. */
int main(int argc, char const *argv[]){
// CANTIDAD DE CLIENTES QUE PUEDE ATENDER EL SISTEMA Y LOS RECEPCIONISTAS	

	if(argc == 1) {
		atencionMaxClientes = 20;
		totalRecepcionistas = 3;
		totalRecepcionistasVIP = 1;
		totalMaquinasChecking=5;
	}
/*
	// CANTIDAD DE CLIENTES QUE PUEDE ATENDER EL SISTEMA VARIABLE
	else if(argc == 2) {
		atencionMaxClientes = atoi(argv[1]);
		totalRecepcionistas = 3;
		totalRecepcionistasVIP = 1;
	}*/
	// CANTIDAD DE MAQUINAS DE AUTOCHEKING ENTRA POR LÍNEA DE COMANDOS
	else if(argc==3){
		totalMaquinasChecking = atoi(argv[2]);
	}
	// ARGUMENTOS INVÁLIDOS

	else {
		printf("Número de argumentos inválidos\n");
		exit(-1);
	}
	
	maquinasChecking = (int *)malloc(sizeof(int)*totalMaquinasChecking);
	cola = (clientes *) malloc(atencionMaxClientes * sizeof(clientes));	
	recepcionistas = (pthread_t *) malloc(totalRecepcionistas * sizeof(pthread_t));	
	
//INFO
	printf("Escriba SIGUSR1 %d para introducir un cliente de los de andar por casa.\n", getpid());
	printf("Escriba SIGUSR2 %d para introducir un cliente con aires de grandeza.\n", getpid());
	printf("Escriba SIGINT  %d cuando quiebre el hotel.\n", getpid());
	printf("Escriba SIGPIPE %d para incrementar el número de máquinas expendedoras.\n", getpid());
	
	
// ENMASCARAR SEÑALES
	
	struct sigaction ss;

	ss.sa_handler = nuevoCliente;
	sigemptyset(&ss.sa_mask);
	ss.sa_flags = 0;

	if(sigaction(SIGUSR1, &ss, NULL) == -1) {
		perror("Error al enmascarar la señal\n");
		exit(-1);
	}
	
	struct sigaction ss2;

	ss2.sa_handler = nuevoCliente;
	sigemptyset(&ss2.sa_mask);
	ss2.sa_flags = 0;

	if(sigaction(SIGUSR2, &ss2, NULL) == -1) {
		perror("Error al enmascarar la señal\n");
		exit(-1);
	}
	
	struct sigaction ss3;

	ss3.sa_handler = finalizarAplicacion;
	sigemptyset(&ss3.sa_mask);
	ss3.sa_flags = 0;

	if(sigaction(SIGINT, &ss3, NULL) == -1) {
		perror("Error al enmascarar la señal\n");
		exit(-1);
	}
	
	//incremento del número de máquinas de autochecking
	struct sigaction ss4;

	ss4.sa_handler = incrementaMaquinasChecking;
	sigemptyset(&ss4.sa_mask);
	ss4.sa_flags = 0;

	if(sigaction(SIGPIPE, &ss4, NULL) == -1) {
		perror("Error al enmascarar la señal\n");
		exit(-1);
	}

	// COLA CLIENTES

	int i;

	for(i = 0; i < atencionMaxClientes; i++) {
		(cola+i)->id = 0;
		(cola+i)->tipo = 0;
		(cola+i)->atendido = 0;
		(cola+i)->serologia = 0;
		(cola+i)->hilo = 0;
	}
	
        // RESETEAR LOG

	logFile = fopen(logFileName, "w");
	fclose(logFile);
	
	
	// CONTADOR A 0
	
	contadorClientes = 0;
	totalClientes = 0;
	clientesAscensor = 0;
	finPrograma = PROGRAMA_EJECUCCION;
	ascensorLleno = 0;
	
	//VARIABLES CONDICION

	pthread_cond_init(&ascensorFin, NULL);
	
	// MUTEX
	
	pthread_mutex_init(&mutexLog, NULL);
	pthread_mutex_init(&mutexColaClientes, NULL);
	pthread_mutex_init(&mutexAscensor, NULL);
	pthread_mutex_init(&mutexMaquinas, NULL);


	//INICIALIZACION DE LAS MAQUINAS
	for(i=0; i<totalMaquinasChecking; i++){
		*(maquinasChecking+i)=0;
	}
	
	// HILOS RECEPCIONISTAS
	
	int recepcionistaNm = RECEPCIONISTA_NORMAL;
	int recepcionistaVIP = RECEPCIONISTA_VIP;


	for(i = 0; i < totalRecepcionistas; i++) {

		if(i == 0) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaNm);
		else if(i == 1) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaNm);
		else if(i == 2) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaVIP);
    		
	}
	
	//bucle infinito para funcionamiento continuo del programa
	while(funcionando){
		sleep(1);
	}
	
	
	return 0;
}

void *accionesRecepcionista(void *ptr) {

	int tipo = *(int *)ptr;
	int clienteID = 0;
	int clientesAtendidos = 0;

	srand(time(NULL));
	
	char *msg = (char*)malloc(sizeof(char)*256);
	char *titulo = (char*)malloc(sizeof(char)*20);
    	//char titulo[100];
    	//char message[200];

    	sprintf(titulo, "recepcionista_%s_%d", tipoRecepcionista[tipo - 1], tipo);
    	
    	for(;;) {
    	
		// HAY MAS DE UN CLIENTE EN EL SISTEMA		

		if(contadorClientes > 0) {
		//printf("****************************");
		
			// BUSCA UNA SOLICITUD SEGÚN SU TIPO

			pthread_mutex_lock(&mutexColaClientes); 

			clienteID = buscarSolicitud(tipo);
			

			pthread_mutex_unlock(&mutexColaClientes); 


			// ATIENDE Al CLIENTE

			if(clienteID != -1) {
			
				clientesAtendidos++;
				pthread_mutex_lock(&mutexColaClientes); 

				(cola+clienteID)->atendido = ATENDIENDO;
    
    				sprintf(msg, "El cliente_%d está siendo atendido", (cola+clienteID)->id);
    				writeLogMessage(titulo, msg);

				pthread_mutex_unlock(&mutexColaClientes); 

				int estadoCliente = calculaAleatorios(1, 100);

				// ATENCIÓN CORRECTA, PAPELES EN REGLA

				if(estadoCliente >= 0 && estadoCliente <= 80) {
					int tiempo = calculaAleatorios(1, 4);
					sleep(tiempo);

					pthread_mutex_lock(&mutexColaClientes); 

					(cola+clienteID)->atendido = ATENDIDO;
    
    					sprintf(msg, "El cliente_%d ha sido atendido correctamente en %d segundos", (cola+clienteID)->id, tiempo);
    					writeLogMessage(titulo, msg);

					pthread_mutex_unlock(&mutexColaClientes); 
				}

				// ERRORES EN LOS DATOS

				else if(estadoCliente > 80 && estadoCliente <= 90) {
					int tiempo = calculaAleatorios(2, 6);
					sleep(tiempo);

					pthread_mutex_lock(&mutexColaClientes); 

					(cola+clienteID)->atendido = ATENDIDO;

    					sprintf(msg, "El cliente_%d ha sido atendido en %d segundos y contenia errores en los datos", (cola+clienteID)->id, tiempo);
    					writeLogMessage(titulo, msg);

					pthread_mutex_unlock(&mutexColaClientes); 
				}

				// PASAPORTE VACUNAL

				else if(estadoCliente > 90 && estadoCliente <= 100) {
					int tiempo = calculaAleatorios(6, 10);
					sleep(tiempo);

					pthread_mutex_lock(&mutexColaClientes); 
    					sprintf(msg, "El cliente_%d ha sido atendido en %d segundos, no tenia el pasaporte vacunal y se le ha expulsado del hotel", (cola+clienteID)->id, tiempo);
    					writeLogMessage(titulo, msg);

					expulsarCliente(clienteID);
					pthread_cancel((cola+clienteID)->hilo);

					pthread_mutex_unlock(&mutexColaClientes); 
				}
			}
		} else {
			sleep(1);
		}

		// DESPUÉS DE 5 CLIENTES EL RECEPCIONISTA SE TOMA UN DESCANSO DE 5 SEGUNDOS, excepto si es recepcionista VIP

		if(clientesAtendidos == 5 && tipo != RECEPCIONISTA_VIP) {
			clientesAtendidos = 0;
			
    			writeLogMessage(titulo, "Va a empezar su descanso");
			
			sleep(5);
			
    			writeLogMessage(titulo, "Termina su descanso");
		}
	}
	pthread_exit(NULL);

}

void expulsarCliente(int posicion) {

	// SE RESETEAN VARIABLES
	pthread_mutex_lock(&mutexColaClientes);
	(cola+posicion)->id = 0;
	(cola+posicion)->tipo = 0;
	(cola+posicion)->atendido = 0;
	(cola+posicion)->hilo = 0;
	pthread_mutex_unlock(&mutexColaClientes);
	// SE DECREMENTA EL TOTAL DE CLIENTES

	contadorClientes--;
}

int buscarSolicitud(int tipo) {
	int posicion = -1;
	int i = 0;

	// MIENTRAS HAYA SOLCITUDES QUE ATENDER
	
	while(i < atencionMaxClientes) {
		if((cola+i)->id != 0 && (cola+i)->atendido == NO_ATENDIDO) {

			// SI COINCIDE CON EL TIPO O SI ES ATENDEDOR VIP

			if((cola+i)->tipo == tipo) {

				if(posicion != -1) {

					// SI EL TIEMPO DE ESPERA ES MAYOR

					if((cola+posicion)->id > (cola+i)->id) {
						posicion = i;
					}
				} else {
					posicion = i;
				}
			}
		}
		i ++;
	}

	// DEVOLVEMOS -1 SI NO HA ENCONTRADO NINGUNO DE SU TIPO

	return posicion;
}

// IMPLEMENTACION DE LAS FUNCIONES RESPECTIVAS DE LOS CLIENTES

//funciones utilizadas unicamente por clientes

void irAMaquinas(struct clientes *cliente, char* logMessage);
void irseDelHotel(struct clientes *cliente, char* logMessage);
void irAAscensores(struct clientes *cliente, char* logMessage);

void nuevoCliente(int s){

	for(int i = 0; i < contadorClientes; i++){

		printf("Cliente %d, de tipo %d. Atendio = %d. Serologia = %d y ascensor = %d", (cola+i)->id, (cola+i)->tipo, (cola+i)->atendido, (cola+i)->serologia, (cola+i)->ascensor);

	}
	//1.Comprobamos si hay espacio
	pthread_mutex_lock(&mutexColaClientes);
	if(contadorClientes < atencionMaxClientes){
		//1a. Lo hay

		//1ai. Se anyade el cliente
		int i = 0;
		while(i < atencionMaxClientes && (cola+i)->id != 0)i++;		
		//1aii. Se aumenta el contador de clientes
		totalClientes++;

		//1aiii. Se da identidad al cliente	
		(cola+i)->id = totalClientes;
	
		//1aiv. Se marca al cliente como NO_ATENDIDO
		(cola+i)->atendido = NO_ATENDIDO;

		//1av. Se guarda el tipo de cliente
		if(s == SIGUSR1)
			(cola+i)->tipo = CLIENTE_NORMAL;
		else if(s == SIGUSR2)
			(cola+i)->tipo = CLIENTE_VIP;
		else exit(-1);

		//1avi. Guardamos ascensor
		(cola+i)->ascensor = 0;

		//nuevo.serologia = 0;

		//1avii. Creamos el hilo 
		pthread_create(&(cola+i)->hilo, NULL, accionesCliente, (cola+i));

		contadorClientes++;

	}
	pthread_mutex_unlock(&mutexColaClientes);
	//1bi. No hay espacio, se ignora la llamada 
}


void *accionesCliente(void *ptr){

	struct clientes* cliente = ptr;

	//Creamos un contenedor donde guardar los logs antes de escribirlos
	char * log = (char *) malloc(sizeof(char)*100);

	//1. Guardamos la hora de entrada 
	time_t now = time(0);
        struct tm *tlocal = localtime(&now);

        char stnow[19];
        strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);

	//2. Guardamos el tipo del cliente;
	sprintf(log, "Cliente de tipo %d",cliente->tipo);

	//queHacer determina la accion que hara el cliente de la siguiente manera "Si el x% de clientes hace y, este cliente hara y si queHacer <=x" 
	int queHacer;

	queHacer = calculaAleatorios(1,100);

	if(queHacer<=10) irAMaquinas((cliente), log);
	
	int atendido = (cliente)->atendido;

	//4a. comptueba que el cliente no esta siendo atendido
	while(atendido==NO_ATENDIDO){
		
		//Calcula su proximo movimiento
		queHacer = calculaAleatorios(1,100);

		//Es posible que aparezcan clientes indecisos que acaben de venir de las maquinas y decidan inmediatamente volver a ellas, si esto es un problema, separar la estructura de control.
		if(atendido == NO_ATENDIDO && queHacer <= 20) irAMaquinas(cliente, log);
		else if(queHacer<=30) irseDelHotel(cliente, log);
		else{
			//Para calcular el 5% del 70% restante volvemos a calcular el comportamiento
			queHacer = calculaAleatorios(1,100);

			if(queHacer<=5) irseDelHotel(cliente, log); //4c. se va del hotel			

		}	
		//4e. espera en la cola
		sleep(3);
                pthread_mutex_lock(&mutexColaClientes);
                atendido = (cliente)->atendido;
                pthread_mutex_unlock(&mutexColaClientes);

	}

	//5. Esta siendo atendido, simula ser atendido;
	while(cliente-> atendido != ATENDIDO) sleep(2);

	//6. Calculamos si coge los ascensores
	queHacer = calculaAleatorios(1,100);

	if(queHacer <= 30) irAAscensores(cliente, log); //6a. Coge los ascensores
	else irseDelHotel(cliente, log); //6b. Se va del Hotel

	pthread_exit(NULL);

}

void irAMaquinas(struct clientes *cliente, char* logMessage){
	
	cliente->atendido = 3;
	int i;
	int maquinaUsada=-1; 
	char *msg = (char*)malloc(sizeof(char)*256);
	char *id = (char*)malloc(sizeof(char)*20);
	
	sprintf(id, "cliente_%d", cliente->id);
	sprintf(msg, "A ver como va esto de las maquinitas, halaaaa mira que bonicas ellas.\n");
	writeLogMessage(id, msg);
	pthread_mutex_lock(&mutexMaquinas);
	for(i=0; i<totalMaquinasChecking && maquinaUsada==-1; i++){
		if(*(maquinasChecking+i)==0){
			*(maquinasChecking+i)=1;
			(cliente)->atendido = ATENDIENDO; 
			maquinaUsada=i;
		}
	}
	pthread_mutex_unlock(&mutexMaquinas);
	
	if(cliente->atendido==ATENDIENDO){
		
		sprintf(msg, "A ver si funciona la maquina numero %d ...\n", (maquinaUsada+1));
		writeLogMessage(id, msg);
		
		sleep(6);
		
		sprintf(msg, "Pueg ya he acabao con la maquina, no fue tan difícil ... (dijo rascándose debajo de la boina).\n");
		writeLogMessage(id, msg);
		
		pthread_mutex_lock(&mutexMaquinas);
		*(maquinasChecking+maquinaUsada)=0;
		(cliente)->atendido = ATENDIDO;
		pthread_mutex_unlock(&mutexMaquinas);
	}else{
		sprintf(msg, "Pero que es esto ?!?!, %d máquinas y ninguna libre !!\n", totalMaquinasChecking);
		cliente->atendido = NO_ATENDIDO;
		writeLogMessage(id, msg);
	}
}

void irseDelHotel(struct clientes *cliente, char* logMessage){

	char* id = (char*) malloc(sizeof(char)*2);

	sprintf(id, "cliente_%d",cliente->id);  

	writeLogMessage(id, logMessage);
	expulsarCliente(cliente->id);

	pthread_exit(NULL);

}

void irAAscensores(struct clientes *cliente, char* logMessage){

	char *id = (char*)malloc(sizeof(char)*20);
	char *msg = (char*)malloc(sizeof(char)*256);

	//Cambiamos la variable "ascensor" del cliente
	pthread_mutex_lock(&mutexColaClientes);
	cliente->ascensor = 1;
	pthread_mutex_unlock(&mutexColaClientes);

	//Intenta acceder al ascensor
	pthread_mutex_lock(&mutexAscensor);

	while(1){
		if(clientesAscensor < 6 && ascensorLleno == 0){
			clientesAscensor++;

			sprintf(id, "cliente_%d: ", cliente->id);
			sprintf(msg, "El cliente entra al ascensor.\n");
			writeLogMessage(id, msg);

			if(clientesAscensor == 6) {
				ascensorLleno = 1;
				sleep(3);
				clientesAscensor--;
				sprintf(id, "cliente_%d", cliente->id);
				sprintf(msg, "El cliente deja el ascensor.\n");
				writeLogMessage(id, msg);
				pthread_cond_signal(&ascensorFin);
				pthread_mutex_unlock(&mutexAscensor);
				break;
			}

			pthread_cond_wait(&ascensorFin, &mutexAscensor);
			clientesAscensor--;
			sprintf(id, "cliente_%d", cliente->id);
			sprintf(msg, "El cliente deja el ascensor.\n");
			writeLogMessage(id, msg);
			if(clientesAscensor == 0) ascensorLleno = 0;	//Si al irse deja el ascensor vacío cambia el flag
			break;
		}
		else{
			sprintf(id, "cliente_%d: ", cliente->id);
			sprintf(msg, "El cliente espera por el ascensor.\n");
			writeLogMessage(id, msg);
			sleep(3);
		}
	}

}

// FIN DE LA IMPLEMENTACION DE LAS FUNCIONES RESPECTIVAS A LOS CLIENTES

void finalizarAplicacion(int s) {

	printf("Petición para finalizar la aplicación\n");

	// IGNORAMOS LAS SEÑALES

	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	pthread_mutex_lock(&mutexColaClientes);

	// AVISAMOS AL RESTO DE HILOS QUE EL PROGRAMA TIENE QUE TEMINAR
	
	finPrograma = PROGRAMA_FINALIZA;

	pthread_mutex_unlock(&mutexColaClientes);
	
	int i;
	
	// SE ATIENDEN TODAS LAS SOLICITUDES PENDIENTES
	
	while(1){
		pthread_mutex_lock(&mutexColaClientes);
		if(contadorClientes == 0){
			for(i = 0; i < totalRecepcionistas; i++) {
				pthread_cancel(*(recepcionistas+i));
			}
			pthread_mutex_unlock(&mutexColaClientes);
			break;
		}else{
			pthread_mutex_unlock(&mutexColaClientes);
			sleep(1);
		}
	}
	
	
	printf("Aplicación finalizada\n");

	free(recepcionistas);
	free(cola);
	free(maquinasChecking);

	exit(-1);
}

void writeLogMessage(char *id, char *msg) {
	
	// CALCULAMOS LA HORA ACTUAL
	
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal); 
	
	// ESCRIBIMOS EN LOG

	pthread_mutex_lock(&mutexLog); 

	logFile = fopen(logFileName, "a");
	
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	printf("[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
	
	pthread_mutex_unlock(&mutexLog); 
}


int calculaAleatorios(int min, int max) {
	return rand() % (max-min+1) + min;
}

void incrementaMaquinasChecking(int s){
	char *msg = (char*)malloc(sizeof(char)*256);
	totalMaquinasChecking++;
	maquinasChecking = (int*)realloc(maquinasChecking, totalMaquinasChecking);
	sprintf(msg, "Incrementado el número de maquinas de autochecking, ahora son %d maquinas", totalMaquinasChecking);
	writeLogMessage("Maquinas", msg);
	
}
