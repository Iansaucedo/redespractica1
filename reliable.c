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
#define TIMEOUT 1000000 // 1 ms en nanosegundos
#define DATA_SIZE 1024

// Variables globales
int next_seq_num;      // Número de secuencia del siguiente paquete a enviar
int ack_received;      // Indica si se ha recibido un ACK
long timeout;          // Timeout para el temporizador
char data[DATA_SIZE];  // Buffer para almacenar datos a enviar
int socket_fd;         // Descriptor de archivo del socket UDP

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
    while (1) { // Loop indefinitely until interrupted
        // Enviar el mensaje fijo
        snprintf(data, DATA_SIZE, "%s", fixed_message); // Copia de manera segura
        size_t data_size = strlen(data); // Tamaño del mensaje

        // Enviar el paquete solo si hay datos a enviar
        if (data_size > 0) {
            // Enviar el paquete
            SEND_DATA_PACKET(data_size, 0, next_seq_num, data);
            printf("Sent packet with sequence number: %d, data: '%s'\n", next_seq_num, data);

            // Iniciar temporizador para el paquete enviado
            SET_TIMER(0, timeout);

            // Utiliza poll() en lugar de un bucle con usleep para esperar el ACK
            struct pollfd fds[1];
            fds[0].fd = socket_fd;  // Usamos el descriptor del socket almacenado globalmente
            fds[0].events = POLLIN;  // Queremos que nos avise cuando haya datos para leer (POLLIN)

            int poll_timeout = timeout / 1000;  // Convertir el timeout a milisegundos para poll()
            int ret = poll(fds, 1, poll_timeout);

            if (ret > 0 && (fds[0].revents & POLLIN)) {
                // Si hay datos para leer (ACK), el receive_callback debería manejarlos
                printf("ACK received for seqno: %d!\n", next_seq_num);
                // Incrementar el número de secuencia para el siguiente paquete
                next_seq_num = (next_seq_num + 1) % (MAX_SEQ_NUM + 1);
                ack_received = 0; // Reiniciar el estado de ACK
            } else if (ret == 0) {
                // El timeout ha expirado, retransmitir
                printf("Timeout! Resending packet with sequence number: %d\n", next_seq_num);
                SEND_DATA_PACKET(data_size, 0, next_seq_num, data);
                SET_TIMER(0, timeout);  // Reiniciar el temporizador
            }
        } else {
            printf("No data to send.\n");
        }

        // Optional: Add a small sleep to avoid flooding the network
        usleep(100000); // Sleep for 100ms before sending the next packet
    }
}

// Función de temporizador
void timer_callback(int timer_number) {
    // Este callback no se usa directamente en Stop-and-Wait
    // La lógica está integrada en la función send_callback
}
