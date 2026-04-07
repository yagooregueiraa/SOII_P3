/* PRODUCTOR Y CONSUMIDOR HILOS TIPO COLA */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define N 10
#define iter 20

void* productor(void* arg);
void* consumidor(void* arg);

//buffer compartido entre procesos
typedef struct{
    int cola[N]; //cola donde se guardan los slots
    int inicio; //
    int fin;
    int count;
}Buffer;

//variable global que comparten los hilos
Buffer buffer;

//variables globales para recuento de la suma de cada uno de los hilos
int suma_productor = 0;
int suma_consumidor = 0;

/* FUNCIONES */
int produce_item(FILE *archivo)
{
    int num;

    //se lee el archivo de int en int, importante esapcio
    if (fscanf(archivo,"%d",&num) != 1){
        return -1;
    }
    
    //se incrementa la variable global
    suma_productor+=num;
    return num;
}


void consume_item(int num)
{
    suma_consumidor += num;
}

// Inserta un item na cola polo final
void insert_item(int item,int iteracion)
{
    int indice = buffer.fin;

    sleep(1);

    buffer.cola[indice] = item;

    printf("[Iteración: %2d]Productor: Introduciendo %d en la posicion %d. Count: %d.\n",iteracion,item,indice,buffer.count);


    buffer.fin = (indice + 1) % N;

    int tmp = buffer.count;
    sleep(1);
    buffer.count = tmp + 1;

    printf("[Iteración: %2d]Productor: Count actualizado a %d.\n", iteracion,buffer.count);
}

int remove_item(int iteracion)
{
    sleep(1);
    int item = buffer.cola[buffer.inicio];
    buffer.cola[buffer.inicio] = 0;
    printf("[Iteración: %2d]Consumidor: retiro %2d de la posicion %d. Count era: %d.\n",iteracion,item,buffer.inicio,buffer.count);
    
    buffer.inicio = (buffer.inicio + 1) % N;

    int tmp = buffer.count;
    sleep(2);
    buffer.count = tmp - 1;

    printf("[Iteración: %2d]Consumidor: Count actualizado a %d.\n", iteracion,buffer.count);
    return item;
}


int main()
{
    /**
     * prod -> Variable que guarda la ID del hilo productor
     * cons -> Variable que guarda la ID del hilo consumidor
     */
    pthread_t prod, cons;

    //Creamos hilos de productor y consumidor, pasándoles el puntero al buffer compartido por argumentos
    if(pthread_create(&prod, NULL, productor, NULL) != 0)
    {
        perror("Error al crear hilo productor\n");
        exit(1);
    }
    if(pthread_create(&cons, NULL, consumidor, NULL) != 0)
    {
        perror("Error al crear hilo consumidor\n");
        exit(1);
    }

    //Esperamos al productor y al consumidor
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    //Imprimimos sumas
    printf("RESULTADOS:\n\tSuma productor: %d\n\tSuma consumidor: %d\n", suma_productor, suma_consumidor);
    
    return 0;
}

// Código del productor
void* productor(void* arg)
{
    //Abrimos archivo de texto en modo lectura
    FILE* fp = fopen("archivo.txt", "r");
    if(fp == NULL){
        printf("[ERROR]: Error al abrir el archivo en modo lectura");
        pthread_exit(NULL);
    }

    //Bucle principal del productor
    for(int i = 0; i < iter; i++)
    {   
        while(buffer.count == N);

        // Producimos el item
        int item = produce_item(fp);
        
        // Si llegamos al final del archivo de texto, rompemos el bucle
        if (item == EOF) 
            break;

        //-----------------REGION CRITICA----------------------------------------------//

        // Cuando haya espacio, insertamos el elemento
        insert_item(item, i);
        //-------------------FIN REGION CRITICA--------------------------------------------//
    }

    pthread_exit((void*) 0);
}

// Código del consumidor
void* consumidor(void* arg)
{
    //Bucle principal del consumidor
    for(int i = 0; i < iter; i++)
    {
        while(buffer.count == 0);

        //----------------------REGION CRITICA-----------------------------------------
        //Recogemos item
        sleep(1);
        int item  = remove_item(i);

        //----------------------FIN REGION CRITICA-----------------------------------------

        //Consumimos item
        consume_item(item);
    }

    pthread_exit((void*) 0);
}