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
void irAMaquinas(struc clientes cliente);
void irseDelHotel(struct clientes cliente);
void irAAscensores(struct clientes cliente);
void *accionesRecepcionista(void *ptr);
int buscarSolicitud(int tipo);

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

//Mutex
pthread_mutex_t mutexLog; 
pthread_mutex_t mutexColaClientes;
pthread_mutex_t mutexAscensor;

//Condicionales
pthread_cond_t ascensorLibre;
pthread_cond_t ascensorOcupado;

//Hilos
pthread_t *recepcionistas;
pthread_t *maquinasAutochekin;

typedef struct clientes {
	int id;
	int tipo;
	int atendido;
	int serologia;
	pthread_t hilo;
} clientes;

clientes *cola;

int atencionMaxClientes;
int totalRecepcionistasVIP;
int totalRecepcionistas;

/* Funcion principal del programa. */
int main(int argc, char const *argv[]){
// CANTIDAD DE CLIENTES QUE PUEDE ATENDER EL SISTEMA Y LOS RECEPCIONISTAS	

	if(argc == 1) {
		atencionMaxClientes = 20;
		totalRecepcionistas = 3;
		totalRecepcionistasVIP = 1;
	}
/*
	// CANTIDAD DE CLIENTES QUE PUEDE ATENDER EL SISTEMA VARIABLE

	else if(argc == 2) {
		atencionMaxClientes = atoi(argv[1]);
		totalRecepcionistas = 3;
		totalRecepcionistasVIP = 1;
	}

	// CANTIDAD DE CLIENTES QUE PUEDE ATENDER EL SISTEMA Y RECEPCIONISTAS VARIABLE

	else if(argc == 3) {
		atencionMaxClientes = atoi(argv[1]);
		totalRecepcionistasVIP = atoi(argv[2]);

		totalRecepcionistas = totalRecepcionistasVIP + 2;
	}
*/
	// ARGUMENTOS INVÁLIDOS

	else {
		printf("Número de argumentos inválidos\n");
		exit(-1);
	}
	
	cola = (clientes *) malloc(atencionMaxClientes * sizeof(clientes));	
	recepcionistas = (pthread_t *) malloc(totalRecepcionistas * sizeof(pthread_t));	
	
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

	pthread_cond_init(&ascensorLibre, NULL);
	pthread_cond_init(&ascensorOcupado, NULL);
	
	// MUTEX
	
	pthread_mutex_init(&mutexLog, NULL);
	pthread_mutex_init(&mutexColaClientes, NULL);
	pthread_mutex_init(&mutexAscensor, NULL);


	
	// HILOS RECEPCIONISTAS
	
	int recepcionistaNm = RECEPCIONISTA_NORMAL;
	int recepcionistaVIP = RECEPCIONISTA_VIP;


	for(i = 0; i < totalRecepcionistas; i++) {

		if(i == 0) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaNm);
		else if(i == 1) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaNm);
		else if(i == 2) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaVIP);
    		
	}
	

return 0;
}

void *accionesRecepcionista(void *ptr) {

	int tipo = *(int *)ptr;
	int clienteID = 0;
	//int recepcionistaID = 0;
	int clientesAtendidos = 0;

	srand(time(NULL));
	
    	char titulo[100];
    	char message[200];

    	sprintf(titulo, "recepcionista_%s", tipoRecepcionista[tipo - 1]);
    	
    	for(;;) {

		// HAY MAS DE UN CLIENTE EN EL SISTEMA		

		if(totalClientes > 0) {
		
		
			// BUSCA UNA SOLICITUD SEGÚN SU TIPO

			pthread_mutex_lock(&mutexColaClientes); 

			clienteID = buscarSolicitud(tipo);
			

			pthread_mutex_unlock(&mutexColaClientes); 


			// ATIENDE Al CLIENTE

			if(clienteID != -1) {
				clientesAtendidos++;
				pthread_mutex_lock(&mutexColaClientes); 

				(cola+clienteID)->atendido = ATENDIENDO;
    
    				sprintf(message, "El cliente_%d está siendo atendido", (cola+clienteID)->id);
    				writeLogMessage(titulo, message);

				pthread_mutex_unlock(&mutexColaClientes); 

				int estadoCliente = calculaAleatorios(1, 100);

				// ATENCIÓN CORRECTA, PAPELES EN REGLA

				if(estadoCliente >= 0 && estadoCliente <= 80) {
					int tiempo = calculaAleatorios(1, 4);
					sleep(tiempo);

					pthread_mutex_lock(&mutexColaClientes); 

					(cola+clienteID)->atendido = ATENDIDO;
    
    					sprintf(message, "El cliente_%d ha sido atendido correctamente en %d segundos", (cola+clienteID)->id, tiempo);
    					writeLogMessage(titulo, message);

					pthread_mutex_unlock(&mutexColaClientes); 
				}

				// ERRORES EN LOS DATOS

				else if(estadoCliente > 80 && estadoCliente <= 90) {
					int tiempo = calculaAleatorios(2, 6);
					sleep(tiempo);

					pthread_mutex_lock(&mutexColaClientes); 

					(cola+clienteID)->atendido = ATENDIDO;

    					sprintf(message, "El cliente_%d ha sido atendido en %d segundos y contenia errores en los datos", (cola+clienteID)->id, tiempo);
    					writeLogMessage(titulo, message);

					pthread_mutex_unlock(&mutexColaClientes); 
				}

				// PASAPORTE VACUNAL

				else if(estadoCliente > 90 && estadoCliente <= 100) {
					int tiempo = calculaAleatorios(6, 10);
					sleep(tiempo);

					pthread_mutex_lock(&mutexColaClientes); 
					(cola+clienteID)->serologia = SEROLOGIA_INCORRECTA;
    					sprintf(message, "El cliente_%d ha sido atendido en %d segundos, no tenia el pasaporte vacunal y se le ha expulsado del hotel", (cola+clienteID)->id, tiempo);
    					writeLogMessage(titulo, message);

					pthread_cancel((cola+clienteID)->hilo);
					expulsarCliente(clienteID);

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

	(cola+posicion)->id = 0;
	(cola+posicion)->tipo = 0;
	(cola+posicion)->atendido = 0;
	(cola+posicion)->hilo = 0;

	// SE DECREMENTA EL TOTAL DE CLIENTES

	totalClientes--;
}

int buscarSolicitud(int tipo) {
	int posicion = -1;
	int i = 0;

	// MIENTRAS HAYA SOLCITUDES QUE ATENDER
	
	while(i < atencionMaxClientes) {
		if((cola+i)->id != 0 && (cola+i)->atendido == NO_ATENDIDO) {

			// SI COINCIDE CON EL TIPO O SI ES ATENDEDOR VIP

			if(tipo == RECEPCIONISTA_VIP || (cola+i)->tipo == tipo) {

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

//
void nuevoCliente(int s){

	//1.Comprobamos si hay espacio
	if(contadorClientes < 20){
		//1a. Lo hay

		//1ai. Se anyade el cliente
		struct clientes nuevo;
		
		//1aii. Se aumenta el contador de clientes
		contadorClientes++;

		//1aiii. Se da identidad al cliente
		nuevo.id = contadorClientes;	
	
		//1aiv. Se marca al cliente como NO_ATENDIDO
		nuevo.atentido = NO_ATENDIDO;

		//1av. Se guarda el tipo de cliente
		if(s == SIGUSR1)
			nuevo.tipo = CLIENTE_NORMAL;
		else if(s == SIGUSR2)
			nuevo.tipo = CLIENTE_VIP;
		else exit(-1);

		//1avi. Guardamos serologia
		nuevo.serologia = 0;

		//1avii. Creamos el hilo 
		p_thread_create(nuevo.hilo, NULL, accionesCliente, &nuevo);

	}
	//1bi. No hay espacio, se ignora la llamada 
}


void *acionesCliente(void *ptr){

	struct clientes cliente = *(struct clientes) ptr;

	//Creamos un contenedor donde guardar los logs antes de escribirlos
	char * log = malloc(sizeOf(char)*100);

	//1. Guardamos la hora de entrada 
	time_t now = time(0);
        struct tm *tlocal = localtime(&now);

        char stnow[19];
        strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);

	//2. Guardamos el tipo del cliente;
	printf(log, cliente.tipo);	

	//queHacer determina la accion que hara el cliente de la siguiente manera "Si el x% de clientes hace y, este cliente hara y si queHacer <=x" 
	int queHacer = calculaAleatorios(1,100);

	if(queHacer<=10) //3. irAMaquinas(cliente);
	else {
	
		//4a. comptueba que el cliente no esta siendo atendido
		while(ptr.atendido = NO_ATENDIDO){
			
			//Calcula su proximo movimiento
			queHacer = calculaAleatorios(1,100);

			
			if(queHacer <= 20) //4d. va a  3. irAMaquinas(cliente);
			else if(queHacer(<=30) //4c. se va del hotel irseDelHotel(cliente);
			else{
				//Para calcular el 5% del 70% restante volvemos a calcular el comportamiento
				queHacer = calculaAleatorios(1,100);

				if(queHacer<=5) //4c. se va del hotel irseDelHotel();			

			}		
			//4e. espera en la cola
			sleep(3000);

		}
		//5. Esta siendo atendido, simula ser atendido;
		sleep(2000);
	}

	//6. Calculamos si coge los ascensores
	queHacer = calculaAleatorios(1,100);

	if(queHacer <= 30) //6a. Los coge ascensor(cliente);
	else //6b. Se va irseDelHotel(cliente);

}


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
		if(totalClientes == 0){
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
	fclose(logFile);
	
	pthread_mutex_unlock(&mutexLog); 
}




int calculaAleatorios(int min, int max) {
	return rand() % (max-min+1) + min;
}


