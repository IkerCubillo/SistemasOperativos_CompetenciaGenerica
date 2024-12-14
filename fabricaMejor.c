#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <mqueue.h> // Para la cola de mensajes POSIX

#define MQ_NAME "/cola_ventas" // Nombre de la cola de mensajes

pid_t pid_almacen, pid_fabrica, pid_ventas;

sem_t sem_ciclo_completo, sem_pintar, sem_empaquetar, sem_inicio, sem_ventas, sem_stock, sem_almacen;
int unidades_producto = 0;

int tiempo_aleatorio(int min, int max) {
    return rand() % (max - min + 1) + min;
}

void* ensamblar(void* args) {
    printf("[Ensamblaje] Comienzo de mi ejecución...\n");
    sem_post(&sem_inicio);
    sem_wait(&sem_inicio);

    while (1) {
        sem_wait(&sem_ciclo_completo);
        printf("[Ensamblaje] Ensamblando producto...\n");
        sleep(tiempo_aleatorio(3, 8));
        printf("[Ensamblaje] Producto ensamblado.\n");

        sem_post(&sem_pintar);
    }
}

void* pintar(void* arg) {
    printf("[Pintado] Comienzo de mi ejecución...\n");
    sem_post(&sem_inicio);
    sem_wait(&sem_inicio);

    while (1) {
        sem_wait(&sem_pintar);
        printf("[Pintado]: Producto recibido. Pintando...\n");
        sleep(tiempo_aleatorio(2, 4));
        printf("[Pintado]: Producto pintado.\n");

        sem_post(&sem_empaquetar);
    }
}

void* empaquetar(void* arg) {
    printf("[Empaquetado] Comienzo de mi ejecución...\n");
    sem_post(&sem_inicio);
    sem_wait(&sem_inicio);

    while (1) {
        sem_wait(&sem_empaquetar);
        printf("[Empaquetado] Producto recibido: empaquetando...\n");
        sleep(tiempo_aleatorio(2, 5));
        printf("[Empaquetado] Producto empaquetado.\n");

        sem_wait(&sem_stock);
        unidades_producto++;
        printf("[Empaquetado] Producto enviado al almacén. Unidades totales: %d.\n", unidades_producto);
        sem_post(&sem_stock);

        sem_post(&sem_ciclo_completo);
    }
}

void* almacen(void* arg) {
    printf("[Almacén] Comienzo mi ejecución...\n");
    sem_post(&sem_inicio);
    sem_wait(&sem_inicio);

    // Crear y abrir la cola de mensajes POSIX
    mqd_t mq = mq_open(MQ_NAME, O_RDONLY | O_CREAT, 0666, NULL);
    if (mq == -1) {
        perror("Error al abrir la cola de mensajes");
        exit(1);
    }

    while (1) {
        char buffer[256];
        ssize_t bytes_read;

        // Leer mensaje de la cola
        bytes_read = mq_receive(mq, buffer, sizeof(buffer), NULL);
        if (bytes_read >= 0) {
            printf("[Almacén] Orden de venta recibida.\n");

            sem_wait(&sem_stock);
            if (unidades_producto > 0) {
                unidades_producto--;
                printf("[Almacén] Producto vendido. Unidades restantes: %d.\n", unidades_producto);
            } else {
                printf("[Almacén] No hay stock disponible para procesar la orden.\n");
            }
            sem_post(&sem_stock);
        }
    }

    mq_close(mq); // Cerrar la cola de mensajes
}

void* ventas(void* arg) {
    printf("[Ventas] Comienzo de mi ejecución...\n");
    sem_post(&sem_inicio);
    sem_wait(&sem_inicio);

    // Crear y abrir la cola de mensajes POSIX
    mqd_t mq = mq_open(MQ_NAME, O_WRONLY, 0666, NULL);
    if (mq == -1) {
        perror("Error al abrir la cola de mensajes");
        exit(1);
    }

    while (1) {
        sleep(tiempo_aleatorio(10, 15)); // Simular una compra en el tiempo aleatorio
        printf("[Ventas] Recibida compra desde cliente. Enviando orden...\n");

        // Enviar mensaje a la cola para informar al almacén de una venta
        const char* orden = "venta";
        if (mq_send(mq, orden, sizeof(orden), 0) == -1) {
            perror("Error al enviar la orden de venta");
        }
    }

    mq_close(mq); // Cerrar la cola de mensajes
}


int main(int argc, char* argv[]) {
    srand(time(NULL));

    sem_init(&sem_ciclo_completo, 0, 1);
    sem_init(&sem_pintar, 0, 0);
    sem_init(&sem_empaquetar, 0, 0);
    sem_init(&sem_inicio, 0, 0);
    sem_init(&sem_ventas, 0, 0);
    sem_init(&sem_stock, 0, 1);
    sem_init(&sem_almacen, 0, 0); // Semáforo para sincronizar almacén y ventas

    pid_almacen = fork();

    if (pid_almacen == 0) {
        almacen(NULL);
    } else {
        pid_fabrica = fork();

        if (pid_fabrica == 0) {
            printf("[Fábrica] Comienzo mi ejecución...\n");
            sem_post(&sem_inicio);
            sem_wait(&sem_inicio);

            pthread_t hilo_ensamblaje, hilo_pintado, hilo_empaquetado;

            pthread_create(&hilo_ensamblaje, NULL, ensamblar, NULL);
            pthread_create(&hilo_pintado, NULL, pintar, NULL);
            pthread_create(&hilo_empaquetado, NULL, empaquetar, NULL);

            pthread_join(hilo_ensamblaje, NULL);
            pthread_join(hilo_pintado, NULL);
            pthread_join(hilo_empaquetado, NULL);
        } else {
            pid_ventas = fork();

            if (pid_ventas == 0) {
                ventas(NULL);
            } else {
                sem_wait(&sem_inicio);
                sem_wait(&sem_inicio);
                sem_wait(&sem_inicio);
                sem_wait(&sem_inicio);

                for (int i = 0; i < 4; i++) {
                    sem_post(&sem_inicio);
                }

                wait(NULL);
                sem_destroy(&sem_ciclo_completo);
                sem_destroy(&sem_pintar);
                sem_destroy(&sem_empaquetar);
                sem_destroy(&sem_inicio);
                sem_destroy(&sem_ventas);
                sem_destroy(&sem_stock);
                sem_destroy(&sem_almacen);
            }
        }
    }

    // Eliminar la cola de mensajes al final
    mq_unlink(MQ_NAME);

    return 0;
}
