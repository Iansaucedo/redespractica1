#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include "rlib.h"
#define MAX_SEQ_NUM 255

// Definiciones adicionales
#define DATA_SIZE 1024

// Variables globales
int next_seq_num;      // Número de secuencia del siguiente paquete a enviar
int base_seq_num;      // Número de secuencia del paquete base no confirmado
int window_size;       // Tamaño de la ventana deslizante
long timeout;          // Timeout para el temporizador
char data[DATA_SIZE];  // Buffer para almacenar datos a enviar
int ack_received[MAX_SEQ_NUM + 1]; // Arreglo de ACKs recibidos

// Mensaje quemado para enviar
const char *fixed_message = "Hello, this is a fixed message!";

// Función de inicialización de la conexión
void connection_initialization(int window_size_param, long timeout_in_ns) {
    next_seq_num = 0;           // Iniciar en el primer número de secuencia
    base_seq_num = 0;           // El número base también empieza en 0
    window_size = window_size_param; // Configurar el tamaño de la ventana
    timeout = timeout_in_ns;     // Configurar el timeout

    // Inicializar el estado de ACKs
    for (int i = 0; i <= MAX_SEQ_NUM; i++) {
        ack_received[i] = 0;    // Ningún paquete ha sido confirmado aún
    }
}

// Función de recepción de paquetes
void receive_callback(packet_t *pkt, size_t pkt_size) {
    // Validar el paquete recibido
    if (VALIDATE_CHECKSUM(pkt)) {
        printf("Received packet with seqno: %d\n", pkt->seqno);

        // Comprobar si es un ACK para el número de secuencia dentro de la ventana
        if (pkt->seqno >= base_seq_num && pkt->seqno < (base_seq_num + window_size)) {
            ack_received[pkt->seqno] = 1; // Marcar que se recibió el ACK
            printf("ACK for seqno: %d received.\n", pkt->seqno);

            // Mover la base si hemos recibido ACKs en orden
            while (ack_received[base_seq_num]) {
                ack_received[base_seq_num] = 0; // Reiniciar el estado de ACK
                base_seq_num = (base_seq_num + 1) % (MAX_SEQ_NUM + 1);
            }

            // Reiniciar temporizador solo si hemos recibido un ACK para el paquete base
            SET_TIMER(0, timeout);
        } else {
            printf("ACK not for expected seqno: %d\n", pkt->seqno);
        }
    } else {
        printf("Packet corrupted\n");
    }
}

// Función de envío de paquetes
void send_callback() {
    // Enviar el mensaje fijo en lugar de leer desde la capa de aplicación
    snprintf(data, DATA_SIZE, "%s", fixed_message);
    size_t data_size = strlen(data);  // Tamaño del mensaje

    // Enviar paquetes hasta que la ventana esté llena
    while (next_seq_num < base_seq_num + window_size && data_size > 0) {
        // Enviar el paquete
        SEND_DATA_PACKET(data_size, 0, next_seq_num, data);
        printf("Sent packet with sequence number: %d, data: '%s'\n", next_seq_num, data);

        // Iniciar temporizador solo si es el primer paquete en la ventana
        if (base_seq_num == next_seq_num) {
            SET_TIMER(0, timeout);
        }

        next_seq_num = (next_seq_num + 1) % (MAX_SEQ_NUM + 1);
    }
}

// Función de temporizador (para retransmisión)
void timer_callback(int timer_number) {
    printf("Timer expired. Retransmitting from seqno: %d\n", base_seq_num);

    // Retransmitir todos los paquetes desde el número base hasta el número siguiente
    for (int i = base_seq_num; i < next_seq_num; i++) {
        SEND_DATA_PACKET(DATA_SIZE, 0, i, data);
        printf("Retransmitted packet with sequence number: %d\n", i);
    }

    // Reiniciar el temporizador
    SET_TIMER(0, timeout);
}
