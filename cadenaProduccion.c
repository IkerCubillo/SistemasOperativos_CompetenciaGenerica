#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <mqueue.h>
#include <string.h>
#include <fcntl.h>

// Constantes
#define MAX_MSG 10  // Capacidad máxima de mensajes
#define MSG_SIZE 64 // Tamaño máximo de un mensaje
#define COLA_VENTAS "/cola_ventas"

// Declaraciones iniciales
sem_t sem_pintar, sem_empaquetar; // Semáforos
pid_t pid_almacen, pid_fabrica, pid_ventas; // PID de procesos hijo
int unidades_producto = 0; // Contador de unidades de producto en el almacén
mqd_t cola_ventas; // cola de mensajes.

// Hemos usado la documentación Docstring de Google para python ya que desconocemos el formato correcto para C
int tiempo_aleatorio(int min, int max) {
    /*
    Genera un tiempo aleatorio dentro de un rango dado
 
    Args:
        min (int): Tiempo mínimo en segundos
        max (int): Tiempo máximo en segundos
    
    Returns:
        int: Tiempo aleatorio generado
    */
    return rand() % (max - min + 1) + min;
}

void finalizar_recursos(int sig) {
    /*
    Maneja la señal SIGINT para liberar recursos y finalizar el programa
 
    Args:
        sig (int): Señal recibida
    */
    sem_destroy(&sem_pintar);
    sem_destroy(&sem_empaquetar);

    mq_close(cola_ventas);
    mq_unlink(COLA_VENTAS);

    printf("\n[Sistema] Recursos liberados correctamente. Finalizando...\n");
    exit(0);
}

void manejador_senal(int sig) {
    /*   
    Maneja la señal SIGUSR1 para incrementar el stock en el almacén
 
    Args:
      sig (int): Señal recibida
    */

    if (sig == SIGUSR1) {
        unidades_producto++;
        printf("[Almacén] Producto recibido. Stock actual: %d\n", unidades_producto);
    }
}

void* ensamblar(void* args) {
    /*
    Hilo encargado de ensamblar productos
    
    Args:
        args (void): Argumentos del hilo (no utilizados)
    
    Returns:
        void: Siempre retorna NULL
    */

    printf("[Ensamblaje] Comienzo de mi ejecución...\n");
    while (1) {
        printf("[Ensamblaje] Ensamblando producto...\n");
        sleep(tiempo_aleatorio(3, 8));
        printf("[Ensamblaje] Producto ensamblado.\n");
        sem_post(&sem_pintar); 
    }
}

void* pintar(void* args) {
    /*
    Hilo encargado de pintar productos
 
    Args:
        args (void): Argumentos del hilo (no utilizados)
    
    Returns:
        void: Siempre retorna NULL
    */
    printf("[Pintado] Comienzo de mi ejecución...\n");
    while (1) {
        sem_wait(&sem_pintar); 
        printf("[Pintado] Pintando producto...\n");
        sleep(tiempo_aleatorio(2, 4));
        printf("[Pintado] Producto pintado.\n");
        sem_post(&sem_empaquetar); 
    }
}

void* empaquetar(void* args) {
    /*
    Hilo encargado de empaquetar productos y notificar al almacén
    
    Args:
        args (void): Argumentos del hilo (no utilizados)
    
    Returns:
        void: Siempre retorna NULL
    */
    printf("[Empaquetado] Comienzo de mi ejecución...\n");
    while (1) {
        sem_wait(&sem_empaquetar); 
        printf("[Empaquetado] Empaquetando producto...\n");
        sleep(tiempo_aleatorio(2, 5));
        printf("[Empaquetado] Producto empaquetado. Notificando al almacén...\n");

        kill(pid_almacen, SIGUSR1);
    }
}


int main(int argc, char argv[]) {
    /*
    Programa principal que crea procesos e inicializa recursos
    
    Args:
        argc (int): Número de argumentos
        argv (char[]): Lista de argumentos
    
    Returns:
        int: Siempre retorna 0
    */
    srand(time(NULL));

    // Configurar manejadores de señales
    struct sigaction sa_finalizar;
    sa_finalizar.sa_handler = finalizar_recursos;
    sigaction(SIGINT, &sa_finalizar, NULL);

    struct sigaction sa_almacen;
    sa_almacen.sa_handler = manejador_senal;
    sigaction(SIGUSR1, &sa_almacen, NULL);

    // Inicializar semáforos
    sem_init(&sem_pintar, 0, 0);
    sem_init(&sem_empaquetar, 0, 0);

    // Crear cola de mensajes
    struct mq_attr attr = { .mq_flags = O_NONBLOCK, .mq_maxmsg = MAX_MSG, .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };
    cola_ventas = mq_open(COLA_VENTAS, O_CREAT | O_RDWR | O_NONBLOCK, 0644, &attr);

    if (cola_ventas == -1) {
        perror("Error creando cola de mensajes de ventas");
        exit(1);
    }

    pid_almacen = fork();

    if (pid_almacen != 0) {
        pid_fabrica = fork();
        if (pid_fabrica != 0) {
            pid_ventas = fork();
            if (pid_ventas != 0) {
                /* Proceso padre */
                pause(); 

                kill(pid_almacen, SIGINT);
                kill(pid_fabrica, SIGINT);
                kill(pid_ventas, SIGINT);

            } else {
                /* Proceso Ventas */
                char orden[MSG_SIZE];
                int num_orden = 0;
                while (1) {
                    sleep(tiempo_aleatorio(10, 15));
                    snprintf(orden, MSG_SIZE, "Orden %d", num_orden++);
                    mq_send(cola_ventas, orden, strlen(orden) + 1, 0);
                    printf("[Ventas] Orden enviada: %s\n", orden);
                }
            }
        } else {
            /* Inicia hilos, comienza la fábrica */
            pthread_t h1, h2, h3;
            printf("[Fábrica] Comienzo mi ejecución...\n");
            pthread_create(&h1, NULL, ensamblar, NULL);
            pthread_create(&h2, NULL, pintar, NULL);
            pthread_create(&h3, NULL, empaquetar, NULL);

            pthread_join(h1, NULL);
            pthread_join(h2, NULL);
            pthread_join(h3, NULL);
        }
    } else {
        /* Proceso almacén */
        printf("[Almacén] Comienzo mi ejecución...\n");
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR1);
        sigprocmask(SIG_BLOCK, &sigset, NULL);

        char msg_ventas[MSG_SIZE];
        while (1) {
            /* Administra ordenes de ventas por mensaje POSIX */
            if (mq_receive(cola_ventas, msg_ventas, MSG_SIZE, NULL) > 0) {
                printf("[Almacén] Orden de ventas recibida: %s\n", msg_ventas);
                if (unidades_producto > 0) {
                    unidades_producto--;
                    printf("[Almacén] Orden atendida. Stock restante: %d\n", unidades_producto);
                } else {
                    printf("[Almacén] Sin stock para atender la orden.\n");
                }
            }
            int sig;
            /* Administra señal recibida de la fábrica */
            if (sigwait(&sigset, &sig) == 0) {
                if (sig == SIGUSR1) {
                    unidades_producto++;
                    printf("[Almacén] Producto recibido. Stock actual: %d\n", unidades_producto);
                }
            }
        }
    }
    exit(0);
}
