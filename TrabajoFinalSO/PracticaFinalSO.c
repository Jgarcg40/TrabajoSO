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
#define RECEPCIONISTA_NORMAL1     1
#define RECEPCIONISTA_NORMAL2         2
#define RECEPCIONISTA_VIP                3

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
	
	int recepcionistaNm = RECEPCIONISTA_NORMAL1;
	int recepcionistaNm2 = RECEPCIONISTA_NORMAL2;
	int recepcionistaVIP = RECEPCIONISTA_VIP;

	for(i = 0; i < totalRecepcionistas; i++) {

        if(i == 0) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaNm);
        else if(i == 1) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaNm2);
        else if(i == 2) pthread_create((recepcionistas+i), NULL, accionesRecepcionista, &recepcionistaVIP);

    }

	//bucle infinito para funcionamiento continuo del programa
	while(funcionando){
		pause();
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
    	//Identificadores de los hilos recepcionista
	if (tipo == RECEPCIONISTA_NORMAL1) {
        sprintf(titulo, "recepcionista%s", "Normal_1");

        }else if (tipo == RECEPCIONISTA_NORMAL2){
                sprintf(titulo, "recepcionista%s", "Normal_2");
                       
        }else if (tipo == RECEPCIONISTA_VIP){
                sprintf(titulo, "recepcionista%s", "VIP_3");


        }	
    	for(;;) {
    	
		// HAY MAS DE UN CLIENTE EN EL SISTEMA		

		if(contadorClientes > 0) {
		//printf("****************************");
		
			// BUSCA UNA SOLICITUD SEGÚN SU TIPO

			pthread_mutex_lock(&mutexColaClientes); 

			clienteID = buscarSolicitud(tipo);//busco que el cliente sea igual al tipo del recepcionista
			

			pthread_mutex_unlock(&mutexColaClientes); 


			// ATIENDE Al CLIENTE

			if(clienteID != -1) {
			
				clientesAtendidos++;
				pthread_mutex_lock(&mutexColaClientes); 

				(cola+clienteID)->atendido = ATENDIENDO;

				sprintf(msg, "El cliente_%d está siendo atendido", (cola+clienteID)->id);//pongo el flag en atendiendo
    				writeLogMessage(titulo, msg);

				pthread_mutex_unlock(&mutexColaClientes); 

				int estadoCliente = calculaAleatorios(1, 100);

				// ATENCIÓN CORRECTA, PAPELES EN REGLA

				if(estadoCliente >= 0 && estadoCliente <= 80) {
					int tiempo = calculaAleatorios(1, 4);
					sleep(tiempo);

					pthread_mutex_lock(&mutexColaClientes); 

					(cola+clienteID)->atendido = ATENDIDO;
    
					sprintf(msg, "El cliente_%d ha sido atendido correctamente en %d segundos", (cola+clienteID)->id, tiempo);//pongo el flag atendido
    					writeLogMessage(titulo, msg);

					pthread_mutex_unlock(&mutexColaClientes); 
				}

				// ERRORES EN LOS DATOS

				else if(estadoCliente > 80 && estadoCliente <= 90) {
					int tiempo = calculaAleatorios(2, 6);
					sleep(tiempo);

					pthread_mutex_lock(&mutexColaClientes); 

					(cola+clienteID)->atendido = ATENDIDO;

					sprintf(msg, "El cliente_%d ha sido atendido en %d segundos y contenia errores en los datos", (cola+clienteID)->id, tiempo);//pongo el flag atendido
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

					expulsarCliente(clienteID);//pongo todo a 0
					pthread_cancel((cola+clienteID)->hilo);//se expulsa el cliente cancelando el hilo

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
			writeLogMessage(titulo, msg);
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
	(cola+posicion)->serologia = 0;

	// SE DECREMENTA EL TOTAL DE CLIENTES

	contadorClientes--;
}

int buscarSolicitud(int tipo) {
	int posicion = -1;
	int i = 0;
	tipo = (tipo <= 2)? 1:2;
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
					(cola+i)->atendido = ATENDIENDO;
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

	//Creamos un contenedor donde guardar id
	char * id = (char *) malloc(sizeof(char)*100);
	sprintf(id, "cliente_%d", cliente->id);

	//1. Escribimos la hora de entrada 
	writeLogMessage(id, "Ha entrado al hotel");

	//2. Escribimos  el tipo del cliente
	char* log = (char*) malloc(sizeof(char)*6);
	sprintf(log, "Cliente de tipo %s",(cliente->tipo== CLIENTE_VIP)? "VIP":"normal");
	writeLogMessage(id, log);

	//queHacer determina la accion que hara el cliente de la siguiente manera "Si el x% de clientes hace y, este cliente hara y si queHacer <=x" 
	int queHacer;

	queHacer = calculaAleatorios(1,100);

	//El 10% de los clientes se va a maquinas directamente
	if(queHacer<=10) irAMaquinas((cliente), id);
	
	//Guardamos el valor de atendido en una variable a la que se pueda acceder sin concurrencia
	pthread_mutex_lock(&mutexColaClientes);
	int atendido = (cliente)->atendido;
	pthread_mutex_unlock(&mutexColaClientes);

	//4a. comptueba que el cliente no esta siendo atendido
	while(atendido==NO_ATENDIDO){
		
		//Calcula su proximo movimiento
		queHacer = calculaAleatorios(1,100);

		//20% de cambiar de idea e ir a maquinas 10% de cansarse e irse
		if(atendido == NO_ATENDIDO && queHacer <= 20) irAMaquinas(cliente, id);
		else if(queHacer<=30) irseDelHotel(cliente, id);
		else{
			//5% se ira al baño perderan el turno y se iran
			queHacer = calculaAleatorios(1,100);

			if(queHacer<=5) irseDelHotel(cliente, id); //4c. se va del hotel			

		}	
		//4e. espera en la cola
		sleep(3);

		//Actualizamos la variable atendido
                pthread_mutex_lock(&mutexColaClientes);
                atendido = (cliente)->atendido;
                pthread_mutex_unlock(&mutexColaClientes);

	}

	//5. Esta siendo atendido, simula ser atendido y comprueba cada dos segundos si han acabado de atenderle;
	while(cliente-> atendido != ATENDIDO) sleep(2);

	//6. Calculamos si coge los ascensores
	queHacer = calculaAleatorios(1,100);

	//30% de posibilidades de coger ascensores
	if(queHacer <= 30) irAAscensores(cliente, id); //6a. Coge los ascensores
	else irseDelHotel(cliente, id); //6b. Se va del Hotel

	//Ningun hilo llegara a esta linea, pero por si acaso no queremos que haga cosas raras.
	pthread_exit(NULL);
}

void irseDelHotel(struct clientes *cliente, char* id){

	//Escribe en el log que el cliente se ha ido
	writeLogMessage(id, (cliente->atendido = ATENDIDO)? "se ha ido a su habitacion":"se ha ido del hotel");
	
	//Iguala a 0 todas las variables de cliente para dejar sitio para uno nuevo
	cliente->id = 0;
	cliente->tipo = 0;
 	cliente->atendido = 0;
 	cliente->serologia = 0;
 	cliente->ascensor = 0;
 	cliente->hilo = 0;

	//Se resta el contador de clientes para que el nuevo cliente pueda entrar
	contadorClientes--;

	//Se cierra el hilo
	pthread_exit(NULL);

}

void irAMaquinas(struct clientes *cliente, char* id){
	
	cliente->atendido = 3;
	int i;
	int maquinaUsada=-1;

	//RESERVA DE MEMORIA DINAMICA PARA LA VARIABLE PUNTERO msg
	char *msg = (char*)malloc(sizeof(char)*256);

	//ALMACENAMOS EN LA VARIABLE msg EL MENSAJE
	sprintf(msg, "Estamos en maquinas.\n");

	//PASAMOS LAS VARIABLES id Y msg A LA FUNCION writeLogMessage PARA QUE SE MUESTREN LOS LOGS QUE HACEN REFERENCIA A MAQUINAS
	writeLogMessage(id, msg);

	//UTILIZAMOS EL MUTEX PARA BLOQUEAR EL USO DE LAS MAQUINAS
	pthread_mutex_lock(&mutexMaquinas);

	//MIRAMOS TODAS LAS MAQUINAS
	for(i=0; i<totalMaquinasChecking && maquinaUsada==-1; i++){
		//COMPROBAMOS CUAL DE LAS MAQUINAS ESTA DISPONIBLE PARA EL USO
		if(*(maquinasChecking+i)==0){
			//INDICAMOS QUE LA MAQUINA ESTA OCUPADA
			*(maquinasChecking+i)=1;
			//INDICAMOS QUE EL CLIENTE ESTA SIENDO ATENDIDO POR LA MAQUINA
			(cliente)->atendido = ATENDIENDO;
			//ASIGNAMOS UN VALOR PARA SALIR DEL BUCLE
			maquinaUsada=i;
		}
	}

	//UTILIZAMOS EL MUTEX PARA DESBLOQUEAR EL USO DE LAS MAQUINAS
	pthread_mutex_unlock(&mutexMaquinas);

	//EN CASO DE QUE EL CLIENTE ESTE SIENDO ATENDIDO
	if(cliente->atendido==ATENDIENDO){

		//ALMACENAMOS EN LA VARIABLE msg LA MAQUINA QUE ESTE SIENDO USADA
		sprintf(msg, "A ver si funciona la maquina numero %d ...\n", (maquinaUsada+1));

		//PASAMOS COMO PARAMETROS LA VARIABLES id Y msg A LA FUNCION writeLogMessage PARA MOSTRAR LOS  LOGS
		writeLogMessage(id, msg);

		/*DORMIMOS 6 SEGUNDOS*/
		sleep(6);

		//ALMACENAMOS EN LA VARIABLE msg QUE HA ACABADO DE USAR LA MAQUINA
		sprintf(msg, "He acabado con la maquina\n");

		//PASAMOS COMO PARAMETROS LA VARIABLES id Y msg A LA FUNCION writeLogMessage PARA MOSTRAR LOS  LOGS
		writeLogMessage(id, msg);

		//BLOQUEAMOS EL USO DE LAS MAQUINAS CON EL MUTEX
		pthread_mutex_lock(&mutexMaquinas);
		/*ASIGNAMOS 0 A LA MAQUINA CUANDO EL CLIENTE YA HA ACABADO DE USARLA*/
		*(maquinasChecking+maquinaUsada)=0;

		//INDICAMOS QUE EL CLIENTE HA SIDO ATENDIDO POR LA MAQUINA
		(cliente)->atendido = ATENDIDO;

		//DESBLOQUEAMOS EL MUTEX DE MAQUINAS PORQUE YA SE HA INDICADO QUE LA MAQUINA ESTA VACIA Y QUE EL CLIENTE YA ESTA ATENDIDO
		pthread_mutex_unlock(&mutexMaquinas);

	//EN CASO QUE NO HAYA NINGUNA MAQUINA LIBRE
	}else{

		//ALMACENAMOS EN LA VARIABLE msg EL MENSAJE
		sprintf(msg, "Ninguna maquina libre !!\n");

		//INDICAMOS QUE EL CLIENTE TODAVIA NO ESTA ATENDIDO
		cliente->atendido = NO_ATENDIDO;

		//PASAMOS COMO PARAMETROS LA VARIABLES id Y msg A LA FUNCION writeLogMessage PARA MOSTRAR LOS  LOGS
		writeLogMessage(id, msg);
	}
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

	//Si hay menos de 6 y el ascensor no está lleno
	while(1){
		if(clientesAscensor < 6 && ascensorLleno == 0){
			clientesAscensor++;	//Se incrementa el número de clientes en el ascensor

			sprintf(id, "cliente_%d: ", cliente->id);
			sprintf(msg, "El cliente entra al ascensor.\n");
			writeLogMessage(id, msg);

			//El último marca que el ascensor está lleno, duerme y procede a salir dejando que salgan los demás
			if(clientesAscensor == 6) {
				ascensorLleno = 1;
				sleep(3);
				clientesAscensor--;
				sprintf(id, "cliente_%d", cliente->id);
				sprintf(msg, "El cliente deja el ascensor.\n");
				writeLogMessage(id, msg);
				pthread_cond_broadcast(&ascensorFin);
				pthread_mutex_unlock(&mutexAscensor);
				break;
			}

			//Los demás clientes de ascensor esperan a que salga el último que entró para salir
			pthread_cond_wait(&ascensorFin, &mutexAscensor);
			clientesAscensor--;
			sprintf(id, "cliente_%d", cliente->id);
			sprintf(msg, "El cliente deja el ascensor.\n");
			writeLogMessage(id, msg);
			pthread_cond_signal(&ascensorFin);
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
		pthread_mutex_lock(&mutexColaClientes);
	for(i = 0; i < atencionMaxClientes; i++){
	if((cola+i)->ascensor==1){
	expulsarCliente(i);
		pthread_cancel((cola+i)->hilo);
	}
	
	}
	pthread_mutex_unlock(&mutexColaClientes);
	
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

	//RESERVA DE MEMORIA DINAMICA PARA LA VARIABLE PUNTERO msg
	char *msg = (char*)malloc(sizeof(char)*256);

	//INCREMENTA NUMERO DE MAQUINAS
	totalMaquinasChecking++;

	//REALMACENO EL ESPACIO EN MEMORIA DE maquinasChecking CON LA VARIABLE totalMaquinasChecking
	maquinasChecking = (int*)realloc(maquinasChecking, totalMaquinasChecking);

	//ALMACENAMOS EN LA VARIABLE msg EL MENSAJE
	sprintf(msg, "Incrementado el número de maquinas de autochecking, ahora son %d maquinas", totalMaquinasChecking);

	//PASAMOS LAS VARIABLES id Y msg A LA FUNCION writeLogMessage PARA QUE SE MUESTREN LOS LOGS QUE HACEN REFERENCIA A MAQUINAS
	writeLogMessage("Maquinas", msg);
	
}
