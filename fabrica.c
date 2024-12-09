#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <mqueue.h>
#include <string.h>

// Definir la cola de mensajes
#define QUEUE_NAME "/cola_ventas"
#define MAX_MSG_SIZE 256
#define MSG_BUFFER_SIZE (MAX_MSG_SIZE + 10)

mqd_t mq;
sem_t sem_almacen;          // Semáforo para notificar al almacén
sem_t sem_ventas;           // Semáforo para notificar a ventas
pthread_mutex_t mutex_almacen = PTHREAD_MUTEX_INITIALIZER;
int unidades_producto = 0;  // Cantidad de productos en el almacén

// Atributos de la cola de mensajes
struct mq_attr attr;

// Prototipos de las funciones
void* iniciar_fabrica(void* args);
void* iniciar_almacen(void* args);
void* iniciar_ventas(void* args);
void empaquetar();
void ensamblar();
void pintar();
void producir();

// Función de ensamblaje
void ensamblar() {
    printf("[Ensamblaje] Ensamblando producto...\n");
    sleep(1); // Simular el tiempo de ensamblaje
    printf("[Ensamblaje] Producto ensamblado.\n");
}

// Función de pintado
void pintar() {
    printf("[Pintado] Pintando producto...\n");
    sleep(1); // Simular el tiempo de pintado
    printf("[Pintado] Producto pintado.\n");
}

// Función de empaquetado
void empaquetar() {
    printf("[Empaquetado] Empaquetando producto...\n");
    sleep(1); // Simular el tiempo de empaquetado
    printf("[Empaquetado] Producto empaquetado. Notificando al almacén...\n");
    
    // Incrementamos el stock del almacén
    pthread_mutex_lock(&mutex_almacen);
    unidades_producto++;
    printf("[Empaquetado] Producto enviado al almacén. Unidades en stock: %d\n", unidades_producto);
    pthread_mutex_unlock(&mutex_almacen);

    // Notificar al almacén que hay un producto disponible
    sem_post(&sem_almacen);  // Notificar que hay un producto disponible en el almacén
}

// Función de producción (simula ensamblaje, pintado y empaquetado)
void producir() {
    ensamblar();
    pintar();
    empaquetar();
}

// Función del proceso de fábrica
void* iniciar_fabrica(void* args) {
    while (1) {
        producir();  // Producción de un solo producto
        sleep(2);    // Simula el tiempo entre producciones
    }
    return NULL;
}

// Función del proceso de almacén
void* iniciar_almacen(void* args) {
    while (1) {
        // Esperar a que haya productos disponibles en el almacén
        sem_wait(&sem_almacen);  // Esperar que se notifique que hay un producto disponible

        // Simular la recepción de productos
        pthread_mutex_lock(&mutex_almacen);
        if (unidades_producto > 0) {
            printf("[Almacén] Producto recibido. Unidades en stock: %d\n", unidades_producto);
        } else {
            printf("[Almacén] No hay productos disponibles.\n");
        }
        pthread_mutex_unlock(&mutex_almacen);

        sleep(1);  // Simular el proceso de recepción
    }
    return NULL;
}

// Función del proceso de ventas
void* iniciar_ventas(void* args) {
    while (1) {
        printf("[Ventas] Esperando productos...\n");

        // Verificar si hay productos disponibles en el almacén
        sem_wait(&sem_almacen);  // Esperar que haya un producto disponible en el almacén

        pthread_mutex_lock(&mutex_almacen);
        if (unidades_producto > 0) {
            // Realizar una venta
            unidades_producto--;
            printf("[Ventas] Producto vendido. Unidades en stock: %d\n", unidades_producto);

            // Notificar al almacén que el producto ha sido vendido
            printf("[Ventas] Enviando confirmación al almacén sobre la venta.\n");

            // Aquí podrías agregar la lógica para enviar un mensaje al almacén, si es necesario
        } else {
            printf("[Ventas] No hay productos disponibles para vender.\n");
        }
        pthread_mutex_unlock(&mutex_almacen);

        sleep(1);  // Simula el tiempo entre ventas
    }
    return NULL;
}

// Función de limpieza al capturar SIGINT
void cleanup(int signum) {
    printf("\n[PADRE] Recibiendo SIGINT... Limpiando recursos...\n");

    // Cerrar la cola de mensajes y destruir semáforos
    mq_close(mq);
    mq_unlink(QUEUE_NAME);
    sem_destroy(&sem_almacen);
    sem_destroy(&sem_ventas);

    // Finalizar los hilos
    printf("[PADRE] Finalizando los hilos...\n");
    pthread_exit(NULL);
}

int main() {
    pthread_t hilo_fabrica, hilo_almacen, hilo_ventas;

    // Configurar el manejador para SIGINT
    signal(SIGINT, cleanup);

    // Inicializar los atributos de la cola de mensajes
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;           // Número máximo de mensajes
    attr.mq_msgsize = MAX_MSG_SIZE; // Tamaño máximo del mensaje en bytes
    attr.mq_curmsgs = 0;            // Número de mensajes actuales

    // Crear y abrir la cola de mensajes con los atributos
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (mq == -1) {
        perror("Error abriendo la cola de mensajes");
        exit(1);
    }

    // Inicializar los semáforos
    if (sem_init(&sem_almacen, 0, 0) == -1) {
        perror("Error inicializando el semáforo de almacén");
        exit(1);
    }

    if (sem_init(&sem_ventas, 0, 0) == -1) {
        perror("Error inicializando el semáforo de ventas");
        exit(1);
    }

    // Crear los hilos
    pthread_create(&hilo_fabrica, NULL, iniciar_fabrica, NULL);
    pthread_create(&hilo_almacen, NULL, iniciar_almacen, NULL);
    pthread_create(&hilo_ventas, NULL, iniciar_ventas, NULL);

    // Esperar que los hilos terminen
    pthread_join(hilo_fabrica, NULL);
    pthread_join(hilo_almacen, NULL);
    pthread_join(hilo_ventas, NULL);

    return 0;
}
