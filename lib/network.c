/*

    This file is part of Emu-Pizza

    Emu-Pizza is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Emu-Pizza is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Emu-Pizza.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "global.h"
#include "network.h"
#include "serial.h"
#include "utils.h"


/* network special binary semaphore */
typedef struct network_sem_s {
    pthread_mutex_t mutex;
    pthread_cond_t cvar;
    int v;
} network_sem_t;

/* network sockets */
int network_sock_broad = -1;
int network_sock_bound = -1;

/* peer addr */
struct sockaddr_in network_peer_addr;

/* uuid to identify myself */
unsigned int network_uuid;

/* progressive number (debug purposes) */
unsigned int network_prog = 0;

/* track that network is running */
unsigned char network_running = 0;

/* network thread */
pthread_t network_thread;

/* semaphorone */
network_sem_t network_sem;

/* prototypes */
void  network_sem_init(network_sem_t *p);
void  network_sem_post(network_sem_t *p);
void  network_sem_wait(network_sem_t *p);
void  network_send_data(uint8_t v, uint8_t clock);
void *network_start_thread(void *args);


/* start network thread */
void network_start()
{
    /* init semaphore */
    // sem_init(&network_sem, 0, 0);
    network_sem_init(&network_sem);

    /* reset bool */
    network_running = 0;
    
    /* start thread! */
    pthread_create(&network_thread, NULL, network_start_thread, NULL);    
}

/* stop network thread */
void network_stop()
{
    /* already stopped? */
    if (network_running == 0)
        return;
        
    /* tell thread to stop */
    network_running = 0;    
    
    /* wait for it to exit */
    pthread_join(network_thread, NULL);
}

void *network_start_thread(void *args)
{
    utils_log("Starting network thread\n");

    /* open socket sending broadcast messages */
    network_sock_broad = socket(AF_INET, SOCK_DGRAM, 0);
    
    /* exit on error */
    if (network_sock_broad < 1)
    {
        utils_log("Error opening broadcast socket");
        return NULL;
    }
        
    /* open socket sending/receiving serial cable data */
    network_sock_bound = socket(AF_INET, SOCK_DGRAM, 0);

    /* exit on error */
    if (network_sock_bound < 1)
    {
        utils_log("Error opening serial-link socket");
        close (network_sock_broad);
        return NULL;
    }
    
    /* enable to broadcast */
    int enable=1;
    setsockopt(network_sock_broad, SOL_SOCKET, SO_BROADCAST, 
               &enable, sizeof(enable));

    /* prepare dest stuff */
    struct sockaddr_in broadcast_addr;    
    struct sockaddr_in bound_addr;    
    struct sockaddr_in addr_from;    
    socklen_t addr_from_len = sizeof(addr_from);

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));  
    broadcast_addr.sin_family = AF_INET;                
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
//    inet_aton("239.255.0.37",
//              (struct in_addr *) &broadcast_addr.sin_addr.s_addr);
    //inet_aton("192.168.10.168", 
    //          (struct in_addr *) &broadcast_addr.sin_addr.s_addr);
    broadcast_addr.sin_port = htons(64333);             

    /* setup listening socket */
    memset(&bound_addr, 0, sizeof(bound_addr));   
    bound_addr.sin_family = AF_INET;                 
    bound_addr.sin_addr.s_addr = INADDR_ANY;   
    bound_addr.sin_port = htons(64333);                
   
    /* bind to selected port */
    if (bind(network_sock_bound, (struct sockaddr *) &bound_addr, 
             sizeof(bound_addr)))
    {
        utils_log("Error binding to port 64333");

        /* close sockets and exit */
        close(network_sock_broad);
        close(network_sock_bound);
        
        return NULL;
    }

    /* assign it to our multicast group */
/*    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr=inet_addr("239.255.0.37");
    mreq.imr_interface.s_addr=htonl(INADDR_ANY);

    if (setsockopt(network_sock_bound, IPPROTO_IP, IP_ADD_MEMBERSHIP,
               &mreq, sizeof(mreq)) < 0)
    {
        utils_log("Error joining multicast network");

        close(network_sock_broad);
        close(network_sock_bound);
        
        return NULL;
    }*/

    fd_set rfds;
    char buf[64];
    int ret;
    ssize_t recv_ret;
    struct timeval tv;
    int timeouts = 4;
    unsigned int v, clock, prog;
    // uint16_t prog;

    /* message parts */
    char         msg_type;
    unsigned int msg_uuid;
    char         msg_content[64];

    /* generate a random uuid */
    srand(time(NULL));
    network_uuid = rand() & 0xFFFFFFFF;

    /* set callback in case of data to send */
    serial_set_send_cb(&network_send_data);

    /* declare network is running */
    network_running = 1;

    utils_log("Network thread started\n");

    /* loop forever */
    while (network_running)
    {
        FD_ZERO(&rfds);
        FD_SET(network_sock_bound, &rfds);

        /* wait one second */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        /* one second timeout OR something received */
        ret = select(network_sock_bound + 1, &rfds, NULL, NULL, &tv);

        // utils_log_urgent("SELECT RET: %d\n", ret);

        /* error! */
        if (ret == -1)
            break;

        /* ret 0 = timeout */
        if (ret == 0)
        {
            if (++timeouts == 5)
            {
                /* build output message */
                sprintf(buf, "B%08x%s", network_uuid, global_cart_name);

                /* send broadcast message */
                sendto(network_sock_broad, buf, strlen(buf), 0, 
                             (struct sockaddr *) &broadcast_addr, 
                             sizeof(broadcast_addr));  

                timeouts = 0;
            }
        }
        else
        {
            /* reset message content */
            bzero(buf, sizeof(buf));
            bzero(msg_content, sizeof(msg_content));

            /* exit if an error occour */
            if ((recv_ret = recvfrom(network_sock_bound, buf, 64, 0,
                         (struct sockaddr *) &addr_from, 
                         (socklen_t *) &addr_from_len)) < 1)
                break;

            /* dissect message */
            if (sscanf(buf, "%c%08x%s", 
                       &msg_type, &msg_uuid, msg_content) == 3)
            {
                /* was it send by myself? */
                if (msg_uuid != network_uuid)
                {
                    /* is it a serial message? */
                    if (msg_type == 'M')
                    {
                        /* extract value from hex string */
                        sscanf(msg_content, "%02x%02x%02x", &v, &clock, &prog);

                        /* tell serial module something has arrived */
                        serial_recv_byte((uint8_t) v, (uint8_t) clock);

                        /*if (clock == 0)
                        {
                            utils_ts_log("SEM POST\n");

                            network_sem_post(&network_sem);
                        } */
                    }
                    else if (msg_type == 'B')
                    {
                        /* someone is claiming is playing with the same game? */
                        if (strcmp(msg_content, global_cart_name) == 0 && 
                            serial.peer_connected == 0)
                        {
                            /* save sender */
                            memcpy(&network_peer_addr, &addr_from, 
                                   sizeof(struct sockaddr_in));

                            /* just change dst port */
                            network_peer_addr.sin_port = htons(64333);

                            /* notify the other peer by sending a b message */
                            sprintf(buf, "B%08x%s", network_uuid, 
                                    global_cart_name);

                            /* send broadcast message */
                            sendto(network_sock_broad, buf, strlen(buf), 0,
                                   (struct sockaddr *) &network_peer_addr,
                                   sizeof(network_peer_addr));

                            /* log that peer is connected */
                            utils_log("Peer connected: %s\n", 
			        inet_ntoa(network_peer_addr.sin_addr));

                            /* YEAH */
                            serial.peer_connected = 1;
                        }
                    } 
                }
            }
        }
    }
   
    /* free serial */
    serial.peer_connected = 0;

    /* close sockets */
    close(network_sock_broad);
    close(network_sock_bound);    
    
    return NULL;
}

void network_send_data(uint8_t v, uint8_t clock)
{
    char msg[64];

    /* format message */
    network_prog = ((network_prog + 1) & 0xff);
    sprintf(msg, "M%08x%02x%02x%02x", network_uuid, v, clock, network_prog);

    /* send */
    sendto(network_sock_bound, msg, strlen(msg), 0,
           (struct sockaddr *) &network_peer_addr, sizeof(network_peer_addr));

    /* if clock == 1, wait for response */
/*    if (clock)
    {
        network_sem_wait(&network_sem);

        utils_ts_log("SEM WAIT USCITOOOOOOOOOOOOO\n");
*/
        /*if (network_sem_wait(&network_sem, &t) < 0)
        {
            sem_getvalue(&network_sem, &val);

            utils_ts_log("SEM - TIMEOUT - %d\n", val);
        }
        else
        {
            sem_getvalue(&network_sem, &val);

            utils_ts_log("SEM - ARRIVATA LA RISPOSTA - VALUE %d\n", val);
        }*/
 //   }

/*    {
        if (recvfrom(network_sock_bound, buf, 64, 0,
                    (struct sockaddr *) &addr_from,
                    (socklen_t *) &addr_from_len) < 1)
    } */
}


void network_sem_init(network_sem_t *p)
{
    pthread_mutex_init(&p->mutex, NULL);
    pthread_cond_init(&p->cvar, NULL);
    p->v = 0;
}

void network_sem_post(network_sem_t *p)
{
    return;

    pthread_mutex_lock(&p->mutex);
    p->v = 1;
    pthread_cond_signal(&p->cvar);
    pthread_mutex_unlock(&p->mutex);

    utils_ts_log("SEM POST DONE\n");
}

void network_sem_wait(network_sem_t *p)
{
    return;

    pthread_mutex_lock(&p->mutex);
    while (!p->v && network_running)
        pthread_cond_wait(&p->cvar, &p->mutex);
    p->v = 0;
    pthread_mutex_unlock(&p->mutex);

    utils_ts_log("SEM WAIT DONE\n");
}
