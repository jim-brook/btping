#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <errno.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <pthread.h>
#include "common.h"

#define RST_THRSHLD 50
#define HCIGETCONNINFO	_IOR('H', 213, int)
// gcc btping.c sound_out.c -obtping -lbluetooth -lopenal -lm -lpthread -pthread `pkg-config --libs freealut`
// sudo hciconfig hci0 up
// sudo pulseaudio --kill
// sudo pulseaudio --system
// su - ./btping -m
pthread_t soundy;

int main(int argc, char **argv)
{
    struct hci_conn_info_req *cr; 
    bdaddr_t bdaddr;
	int8_t rssi;  
    uint16_t handle;
	uint8_t role;   
	unsigned int ptype, counter, rst_cntr, count = 0;
    int dev_id, dd, opt, ctl;
    char strAddr[50], btaddr[50];  
    char *pbtaddr = &btaddr[0];
    char *pmac = NULL;
    char dev_name[256];
  
    while((opt = getopt(argc, argv, "m:")) != -1)  
    {  
        switch(opt)  
        {  

            case 'm': 
                printf("MAC address %s\n", optarg); 
                pmac = optarg;
                break;
   
            case '?':  
                printf("unknown option: %c\n", optopt); 
                break;  
        }  
    }  
  
    if(pmac != NULL){

        count = 0xFFFFFFFF;    
        str2ba(pmac, &bdaddr);        

    } else {

        printf("-p <MAC address 11:22:33:44:55:66>\n");
        exit(1);
    }
    pthread_create(&soundy, NULL, &sound_out, NULL);
    pthread_join(soundy, NULL);
    
    dev_id = hci_get_route(&bdaddr);
    if (dev_id < 0) {

        perror("\nDevice is not available.\n");
        exit(1);
    }        
    dev_id = hci_devid( "00:15:83:FA:A7:9E" );
    dd = hci_open_dev(dev_id);
    if (dd < 0) {
        perror("\nHCI device open failed\n");
        exit(1);
    }
    rst_cntr = 0;
    for(counter = 0 ; counter < count ; counter++)
    {
        rst_cntr++;
        if(rst_cntr > RST_THRSHLD)
        {
            printf("RST\n");
            /* Open HCI socket  */
            if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0) {
                perror("Can't open HCI socket.");
                exit(1);
            }
            /* Stop HCI device */
            if (ioctl(ctl, HCIDEVDOWN, dev_id) < 0) {
                printf("Can't down device hci%d: (%d)\n",
                                dev_id, errno);
                exit(1);
            }
            sleep(1);
            /* Start HCI device */
            if (ioctl(ctl, HCIDEVUP, dev_id) < 0) {
                if (errno == EALREADY)
                    break;
                printf("Can't init device hci%d: (%d)\n",
                                dev_id, errno);
                exit(1);
            }
            rst_cntr = 0;
            sleep(1);
        }
        ba2str( &bdaddr, &strAddr[0]);
        if(counter % 2) {
            
        printf("Pinging: %s...", strAddr);
        
        } else {
            
            printf("Pinging: %s ..", strAddr);
            
        }
        fflush( stdout );


        role = 1;
        ptype = HCI_DM1 | HCI_DM3 | HCI_DM5 | HCI_DH1 | HCI_DH3 | HCI_DH5;
        if (hci_create_connection(dd, &bdaddr, htobs(ptype), htobs(0x0000), role, &handle, 25000) < 0) {

 
            printf("no reply.\n");   
            continue;          

        }

        cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
        if (!cr) {

            perror("\nCan't allocate memory\n");
            hci_disconnect(dd, cr->conn_info->handle, 0x13, 100);
  
            exit(1);
        }
 
        
        bacpy(&cr->bdaddr, &bdaddr);
        cr->type = ACL_LINK;
        if (ioctl(dd, HCIGETCONNINFO, (unsigned long) cr) < 0) {

            perror("\nGet connection info failed\n");
            hci_disconnect(dd, cr->conn_info->handle, 0x13, 100);

            free(cr);    
            continue;
        }

        if (hci_read_rssi(dd, htobs(cr->conn_info->handle), &rssi, 1000) < 0) {

            perror("\nRead RSSI failed\n");
            hci_disconnect(dd, cr->conn_info->handle, 0x13, 100);

            free(cr);            
            continue;
        } 
        if(hci_read_remote_name(dd,  &bdaddr, 256, &dev_name[0], 25000) < 0) {

                //Do nothing; we have the RSSI so let's keep going and print just that instead.
        }
        hci_disconnect(dd, cr->conn_info->handle, 0x13, 100); 
     
        free(cr);
        printf("reply ");
        if(dev_name != NULL) {

            printf("name: %s \t", dev_name);
        }
        printf("RSSI: %d\n", rssi); 
        pthread_create(&soundy, NULL, &sound_out, NULL);
        pthread_join(soundy, NULL);
        //s_main();

    }   
    hci_close_dev(dd);
    return 0;
}
