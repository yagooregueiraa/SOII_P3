/* PRODUCTOR Y CONSUMIDOR HILOS CON MUTEX Y VARIABLES DE CONDICION */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define N 10
#define iteraciones 80

void* productor(void* arg);
void* consumidor(void* arg);

//buffer compartido entre hilos (cola FIFO circular)
typedef struct{
    int cola[N]; //cola donde se guardan los slots
    int inicio;  //indice de lectura (consumidor)
    int fin;     //indice de escritura (productor)
    int count;   //numero de elementos en el buffer
}Buffer;

//variable global que comparten los hilos
Buffer buffer;

//variables globales para recuento de la suma de cada uno de los hilos
int suma_productor = 0;
int suma_consumidor = 0;

//mutex y variables de condicion
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t no_lleno = PTHREAD_COND_INITIALIZER;  //señal: hay hueco en el buffer
pthread_cond_t no_vacio = PTHREAD_COND_INITIALIZER;   //señal: hay datos en el buffer

/* FUNCIONES */
int produce_item(FILE *archivo)
{
    int num;

    //se lee el archivo de int en int
    if (fscanf(archivo, "%d", &num) != 1){
        return -1;
    }

    //se incrementa la variable global
    suma_productor += num;
    return num;
}

void consume_item(int num)
{
    suma_consumidor += num;
}

//inserta un item na cola polo final
void insert_item(int item, int iteracion)
{
    int indice = buffer.fin;
    buffer.cola[indice] = item;

    printf("[Iteración: %2d]Productor: Introduciendo %d en la posicion %d. Count: %d.\n",iteracion, item, indice, buffer.count);

    buffer.fin = (indice + 1) % N;
    buffer.count++;

    printf("[Iteración: %2d]Productor: Count actualizado a %d.\n", iteracion, buffer.count);
}

int remove_item(int iteracion)
{
    int indice = buffer.inicio;
    int item = buffer.cola[indice];
    buffer.cola[indice] = 0;

    printf("[Iteración: %2d]Consumidor: retiro %2d de la posicion %d. Count era: %d.\n",iteracion, item, indice, buffer.count);

    buffer.inicio = (indice + 1) % N;
    buffer.count--;

    printf("[Iteración: %2d]Consumidor: Count actualizado a %d.\n", iteracion, buffer.count);
    return item;
}

//codigo del productor
void* productor(void* arg)
{
    //abrimos archivo de texto en modo lectura
    FILE* fp = fopen("archivo.txt", "r");
    if (fp == NULL){
        printf("[ERROR]: Error al abrir el archivo en modo lectura\n");
        pthread_exit(NULL);
    }

    for (int i = 0; i < iteraciones; i++)
    {
        //producimos el item FUERA de la region critica
        int item = produce_item(fp);

        if (item == -1)
            break;

        //---------- REGION CRITICA ----------//
        pthread_mutex_lock(&mutex);

        //mientras el buffer este lleno, esperamos a que el consumidor nos avise
        while (buffer.count == N){
            pthread_cond_wait(&no_lleno, &mutex);
        }

        //insertamos el elemento
        insert_item(item, i);

        //avisamos al consumidor de que hay datos disponibles
        pthread_cond_signal(&no_vacio);

        pthread_mutex_unlock(&mutex);
        //---------- FIN REGION CRITICA ----------//

        /*Control de velocidad FUERA de la RC:
        Iter  0-29: productor rapido (no duerme)
        Iter 30-59: productor lento (duerme 1s) para que se vacie el buffer
        Iter 60-79: aleatorio entre 0 y 3 segundos */
        if (i >= 30 && i < 60){
            sleep(1);
        }
        if (i >= 60){
            sleep(rand() % 4); //valores entre 0 y 3 segundos
        }
    }

    printf("[-]Productor: Bucle terminado\n");
    fclose(fp);
    pthread_exit((void*) 0);
}

//codigo del consumidor
void* consumidor(void* arg)
{
    for (int i = 0; i < iteraciones; i++)
    {
        //---------- REGION CRITICA ----------//
        pthread_mutex_lock(&mutex);

        //mientras el buffer este vacio, esperamos a que el productor nos avise
        while (buffer.count == 0){
            pthread_cond_wait(&no_vacio, &mutex);
        }

        //recogemos item
        int item = remove_item(i);

        //avisamos al productor de que hay hueco disponible
        pthread_cond_signal(&no_lleno);

        pthread_mutex_unlock(&mutex);
        //---------- FIN REGION CRITICA ----------//

        //consumimos item FUERA de la region critica
        consume_item(item);

        /*Control de velocidad FUERA de la RC:
        Iter  0-29: consumidor lento (duerme 1s) para que se llene el buffer
        Iter 30-59: consumidor rapido (no duerme)
        Iter 60-79: aleatorio entre 0 y 3 segundos */
        if (i < 30){
            sleep(1);
        }
        if (i >= 60){
            sleep(rand() % 4); //valores entre 0 y 3 segundos
        }
    }

    printf("[-]Consumidor: Bucle terminado\n");
    pthread_exit((void*) 0);
}

int main()
{
    srand(time(NULL));

    pthread_t prod, cons;

    //inicializamos el buffer
    for (int i = 0; i < N; i++){
        buffer.cola[i] = 0;
        buffer.inicio = 0;
        buffer.fin = 0;
        buffer.count = 0;
    }

    //creamos hilos de productor y consumidor
    if (pthread_create(&prod, NULL, productor, NULL) != 0)
    {
        perror("Error al crear hilo productor\n");
        exit(1);
    }
    if (pthread_create(&cons, NULL, consumidor, NULL) != 0)
    {
        perror("Error al crear hilo consumidor\n");
        exit(1);
    }

    //esperamos al productor y al consumidor
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    //destruimos mutex y variables de condicion
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&no_lleno);
    pthread_cond_destroy(&no_vacio);

    //imprimimos sumas
    printf("RESULTADOS:\n\tSuma productor: %d\n\tSuma consumidor: %d\n", suma_productor, suma_consumidor);

    printf("Programa acabado\n");

    return 0;
}

