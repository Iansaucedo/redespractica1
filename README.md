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

#define MAX_PAYLOAD 500

uint32_t next_seqno_num = 0;  // Solo puede ser 0 o 1
uint32_t expected_ackno = 0;  // Solo puede ser 0 o 1
uint32_t expected_seqno = 0;
long timeout_in_ns;  
int timer_active = 0;
packet_t last_sent_packet;

/*
    Global data: Add your own data fields below.
*/

//------------------------------------------------------------------------------

void connection_initialization(int window_size, long timeout_ns) {
    timeout_in_ns = timeout_ns;
    next_seqno_num = 0;
    expected_ackno = 0;  // Inicializar el número de ACK esperado a 0
    timer_active = 0;
    memset(&last_sent_packet, 0, sizeof(last_sent_packet)); //Inicia el último
    printf("Inicializacion completa");
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    if (VALIDATE_CHECKSUM(pkt)) {
        // Para que se envie un ACK
        if (IS_ACK_PACKET(pkt)) {
            if (pkt->ackno == expected_ackno) {
                printf("ACK recibido con el numero de secuencia: %u\n", pkt->ackno);
                // Alternar el ACK esperado entre 0 y 1
                expected_ackno = (expected_ackno + 1) % 2;

                if (timer_active) {
                    CLEAR_TIMER(0);
                    timer_active = 0;
                }
            }
        } else {  // Si es un paquete de datos
 
        if (expected_seqno == pkt->seqno) {
            int accepted = ACCEPT_DATA(pkt->data, pkt_size - sizeof(pkt->cksum) - sizeof(pkt->len) - sizeof(pkt->ackno) - sizeof(pkt->seqno));
            if (accepted > 0) {
                printf("Dato aceptado, de tamaño: %d\n", accepted);
            }
            expected_seqno = (expected_seqno + 1) % 2;
        }
            // Enviar un ACK para confirmar la recepción del paquete
            SEND_ACK_PACKET(pkt->seqno);  // Responder con el ACK correspondiente
        }
    } else {
        printf("Paquete corrupto\n");
    }
}

void send_callback() {
    char buffer[MAX_PAYLOAD];
    int bytes_read = READ_DATA_FROM_APP_LAYER(buffer, MAX_PAYLOAD);
    
    if (bytes_read > 0) {
        packet_t pkt;
        pkt.cksum = 0;
        pkt.len = bytes_read + sizeof(pkt.cksum) + sizeof(pkt.len) + sizeof(pkt.ackno) + sizeof(pkt.seqno);
        pkt.ackno = 0;  // El ACK solo lo utiliza el receptor
        pkt.seqno = next_seqno_num;  // Usar el número de secuencia actual (0 o 1)
        memcpy(pkt.data, buffer, bytes_read);
        
        SEND_DATA_PACKET(pkt.len, pkt.ackno, pkt.seqno, pkt.data);
        
        last_sent_packet = pkt;

        timer_active = 1;
        SET_TIMER(0, timeout_in_ns);
        
        // Alternar el número de secuencia entre 0 y 1
        next_seqno_num = (next_seqno_num + 1) % 2;
    }
}

void timer_callback(int timer_number) {
    if (timer_number == 0 && timer_active) {
        printf("Tiempo expirado, retransmitiendo paquete: %u\n", last_sent_packet.seqno);
        SEND_DATA_PACKET(last_sent_packet.len, last_sent_packet.ackno, last_sent_packet.seqno, last_sent_packet.data);

        SET_TIMER(0, timeout_in_ns);
}
}# redespractica1