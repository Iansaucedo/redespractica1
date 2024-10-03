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
#define TIMEOUT 1 // 10 ms en nanosegundos
#define DATA_SIZE 1024

// Variables globales
int next_seq_num;      // Número de secuencia del siguiente paquete a enviar
int ack_received;       // Indica si se ha recibido un ACK
long timeout;           // Timeout para el temporizador
char data[DATA_SIZE];   // Buffer para almacenar datos a enviar

// Mensaje quemado para enviar
const char *fixed_message = "Hello, this is a fixed message!";

// Función de inicialización de la conexión
void connection_initialization(int window_size, long timeout_in_ns) {
    next_seq_num = 0;  // Iniciar en el primer número de secuencia
    ack_received = 0;   // Inicializar el estado del ACK
    timeout = timeout_in_ns; // Configurar el timeout
}

// Función de recepción de paquetes
void receive_callback(packet_t *pkt, size_t pkt_size) {
    // Validar el paquete recibido
    if (VALIDATE_CHECKSUM(pkt)) {
        printf("Received packet with seqno: %d\n", pkt->seqno);
        
        // Comprobar si el paquete es un ACK
        if (pkt->seqno == next_seq_num) {  // Usar seqno en lugar de seq_num
            ack_received = 1; // Marcar que se recibió el ACK
            // Reiniciar el temporizador para evitar retransmisiones innecesarias
            SET_TIMER(0, timeout);
        } else {
            printf("ACK not for expected seqno: %d\n", pkt->seqno);
        }
    } else {
        // Si el paquete está corrupto, ignorarlo
        printf("Packet corrupted\n");
    }
}

// Función de envío de paquetes
void send_callback() {
    // Enviar el mensaje fijo en lugar de leer desde la capa de aplicación
    snprintf(data, DATA_SIZE, "%s", fixed_message); // Copia de manera segura
    size_t data_size = strlen(data); // Tamaño del mensaje

    // Enviar el paquete solo si hay datos a enviar
    if (data_size > 0) {
        // Enviar el paquete
        SEND_DATA_PACKET(data_size, 0, next_seq_num, data);
        printf("Sent packet with sequence number: %d, data: '%s'\n", next_seq_num, data);

        // Iniciar temporizador para el paquete enviado
        SET_TIMER(0, timeout);

        // Esperar hasta recibir el ACK
        while (!ack_received) {
            usleep(1); // Esperar 1 ms
        }

        // Procesar ACK y preparar el próximo número de secuencia
        next_seq_num = (next_seq_num + 1) % (MAX_SEQ_NUM + 1); // Incrementar secuencia
        ack_received = 0; // Reiniciar el estado de ACK
    } else {
        printf("No data to send.\n");
    }
}

// Función de temporizador
void timer_callback(int timer_number) {
    // Este callback no se usa directamente en Stop-and-Wait
    // La lógica está integrada en la función send_callback
}
