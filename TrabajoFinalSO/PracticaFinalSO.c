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
#define ATENDIDO 				2

//Funciones
void writeLogMessage(char *id, char *msg);
int calculaAleatorios(int min, int max);
void clienteNormal(int s);
void clienteVIP(int s);
void nuevoCliente(int t);
void expulsarCliente(int posicion);
void finalizarAplicacion(int s);
void *accionesCliente(void *ptr);
void *accionesRecepcionista(void *ptr);

//Declaración del archivo de log
FILE *logFile;
char logFileName[] = "registroTiempos.log";

//Arrays auxiliares para la escritura del log
char tipoCliente[][15] = {"Normal", "VIP"};
char tipoAtendedor[][15] = {"Normal", "VIP", "Autochekin"};

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
	pthread_t hilo;
} clientes;

clientes *cola;


/* Funcion principal del programa. */
int main(int argc, char const *argv[]){





return 0;
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

