/* 3 PRODUCTORES Y 1 CONSUMIDOR HILOS CON MUTEX Y VARIABLES DE CONDICION COLA DE PRIORIDAD */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define N 10
#define iteraciones 80

void* productor(void* arg);
void* consumidor(void* arg);

//Valor entero con su prioridad
typedef struct
{
    int dato; //Valor entero
    unsigned int prioridad; //Prioridad del entero
}PrioInt;

//Buffer compartido entre hilos (cola de prioridad)
typedef struct{
    PrioInt cola[N]; //cola donde se guardan los slots
    int count;       //numero de elementos en el buffer
}Buffer;

//Argumentos para el productor (id y nombre del archivo que trabaja)
typedef struct
{
    char* nombreArchivo;
    unsigned int id;
}ProdArg;


//variable global que comparten los hilos
Buffer buffer;

//arrays globales para recuento de las suma de cada uno de los hilos
int sumas_prod[3] = {0,0,0};
int sumas_cons[3] = {0,0,0};

//Contador de productores finalizados
int prod_fin = 0;

//mutex y variables de condicion
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t no_lleno = PTHREAD_COND_INITIALIZER;  //señal: hay hueco en el buffer
pthread_cond_t no_vacio = PTHREAD_COND_INITIALIZER;   //señal: hay datos en el buffer

/* FUNCIONES */

//Espera entre 1 y max segundos, valor elegido aleatoriamente
void esperaRandom(int max, unsigned int* seed)
{
    sleep(1 + rand_r(seed) % max);
}


int produce_item(FILE *archivo, unsigned int id)
{
    int num;

    //se lee el archivo de int en int
    if (fscanf(archivo, "%d", &num) != 1){
        return -1;
    }

    //se incrementa el contador global
    sumas_prod[id-1] += num;
    return num;
}

//Espera y consume un item (lo añade a la suma)
void consume_item(PrioInt item, unsigned int* seed)
{
    esperaRandom(3, seed);
    sumas_cons[item.prioridad-1] += item.dato;
}

//inserta un item en la cola por el final (en su rango de prioridad)
void insert_item(int item, unsigned int prio, int iteracion)
{
    int indice = buffer.count - 1;

    printf("[Iteración: %2d]Productor: Introduciendo %d en la posicion %d. Count: %d.\n",iteracion, item, indice, buffer.count);

    //Reordenamos elementos para mantener la estructura fifo y el orden de prioridad
    while (indice >= 0 && buffer.cola[indice].prioridad > prio) 
    {
        buffer.cola[indice + 1] = buffer.cola[indice];
        indice--;
    }

    //Insertamos items
    buffer.cola[indice+1].dato = item;
    buffer.cola[indice+1].prioridad = prio;
    
    //Aumentamos count
    buffer.count++;

    printf("[Iteración: %2d]Productor: Count actualizado a %d.\n", iteracion, buffer.count);
}

PrioInt remove_item(int iteracion)
{
    //Recogemos item a eliminar
    PrioInt retorno = buffer.cola[0];

    //Reordenamos elementos (movemos toda la cola a la izquierda)
    for (int i = 1; i < buffer.count; i++) 
    {
        buffer.cola[i - 1] = buffer.cola[i];
    }

    printf("[Iteración: %2d]Consumidor: retiro %2d de la posicion %d. Count era: %d.\n",iteracion, retorno.dato, buffer.count - 1, buffer.count);

    //Reducimos count
    buffer.count--;

    printf("[Iteración: %2d]Consumidor: Count actualizado a %d.\n", iteracion, buffer.count);
    return retorno;
}

//codigo del productor
void* productor(void* arg)
{
    //Semilla de aleatoriedad propia del hilo
    unsigned int seed = time(NULL) ^ pthread_self();

    //Desreferenciamos parámetros
    ProdArg args = *(ProdArg*) arg;

    //abrimos archivo de texto en modo lectura
    FILE* fp = fopen(args.nombreArchivo, "r");
    if (fp == NULL){
        printf("[ERROR]: Error al abrir el archivo en modo lectura\n");
        pthread_exit(NULL);
    }

    for (int i = 0; i < iteraciones; i++)
    {
        //producimos el item FUERA de la region critica
        int item = produce_item(fp, args.id);
        esperaRandom(6, &seed); //Esperamos entre 1 y 6 segundos

        //En caso de terminar de leer, el productor sale
        if (item == -1)
        {
            pthread_cond_broadcast(&no_vacio);
            break;
        }
            
        //---------- REGION CRITICA ----------//
        pthread_mutex_lock(&mutex);

        //mientras el buffer este lleno, esperamos a que el consumidor nos avise
        while (buffer.count == N){
            pthread_cond_wait(&no_lleno, &mutex);
        }

        //insertamos el elemento
        insert_item(item, args.id, i);

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

    //Aumentamos suma de productores terminados, protegiendo el acceso con mutex
    pthread_mutex_lock(&mutex);
    prod_fin++;
    pthread_cond_broadcast(&no_vacio);
    pthread_mutex_unlock(&mutex);

    fclose(fp);
    pthread_exit((void*) 0);
}

//codigo del consumidor
void* consumidor(void* arg)
{
    //Semilla de aleatoriedad
    unsigned int seed = time(NULL) ^ pthread_self();

    for (int i = 0; 1; i++)
    {
        //---------- REGION CRITICA ----------//
        pthread_mutex_lock(&mutex);

        //mientras el buffer este vacio, esperamos a que el productor nos avise
        while (buffer.count == 0 && prod_fin < 3){
            pthread_cond_wait(&no_vacio, &mutex);
        }

        if(buffer.count == 0 && prod_fin == 3)
        {
            break;
        }


        //recogemos item
        PrioInt item = remove_item(i);

        //avisamos al productor de que hay hueco disponible
        pthread_cond_signal(&no_lleno);

        pthread_mutex_unlock(&mutex);
        //---------- FIN REGION CRITICA ----------//

        //consumimos item FUERA de la region critica
        consume_item(item, &seed);

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
    //Ahora 3 productores y 1 consumidor
    pthread_t prod[3], cons;

    //Un archivo por productor
    char* archivos[] = {"prod1.txt", "prod2.txt", "prod3.txt"};

    //inicializamos el buffer
    buffer.count = 0;
    for (int i = 0; i < N; i++){
        buffer.cola[i].dato = 0;
        buffer.cola[i].prioridad = 0;
    }

    //Estructuras con argumentos de entrada para los productores
    ProdArg argumentos[3];

    //creamos hilos de productores y consumidor
    for(int i = 0; i < 3; i++)
    {
        //Asignamos nombre de archivo e id
        argumentos[i].nombreArchivo = archivos[i];
        argumentos[i].id = i + 1;

        //Creamos hilo
        if (pthread_create(&prod[i], NULL, productor, &argumentos[i]) != 0)
        {
            perror("Error al crear hilo productor\n");
            exit(1);
        }
    }
    if (pthread_create(&cons, NULL, consumidor, NULL) != 0)
    {
        perror("Error al crear hilo consumidor\n");
        exit(1);
    }

    //esperamos a los productores y al consumidor
    for(int i = 0; i < 3; i++)
    {
        pthread_join(prod[i], NULL);
    }
    pthread_join(cons, NULL);

    //destruimos mutex y variables de condicion
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&no_lleno);
    pthread_cond_destroy(&no_vacio);

    //imprimimos sumas
    printf("RESULTADOS:\n\tSuma productores: %d, %d, %d\n\tSuma consumidores: %d, %d, %d\n", 
        sumas_prod[0], sumas_prod[1], sumas_prod[2],
        sumas_cons[0], sumas_cons[1], sumas_cons[2]);

    printf("Programa acabado\n");

    return 0;
}

