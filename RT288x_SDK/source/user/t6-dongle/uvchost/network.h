
#ifndef __NETWORK_H
#define __NETWORK_H

#define MNSP_MAX_MCAST_PACKET_SIZE              (32 * 1024)
#define IMAGE_DIVIDED_UNIT          			(MNSP_MAX_MCAST_PACKET_SIZE - 32)



#define JUVC_TAG                                1247106627 // JUVC

//  JUVC_TYPE_XXX
#define JUVC_TYPE_NONE                          0
#define JUVC_TYPE_CONTROL                       1
#define JUVC_TYPE_CAMERA                        2
#define JUVC_TYPE_MIC                           3
#define JUVC_TYPE_SPK                           4
#define JUVC_TYPE_ALIVE                         255

//  header size is 32 bytes
typedef struct juvc_hdr_packet {
    unsigned int                    Tag;      //  JUVC
    unsigned char                   XactType;            //  JUVC_TYPE_XXX
    unsigned char                   HdrSize;         //  should be 32 always
    unsigned char                   XactId;          //
    unsigned char                   Flags;           //  JUVC_FLAGS_XXX or JUVC_CONTROL_XXX
    unsigned int                    PayloadLength;
    unsigned int                    TotalLength;
    unsigned int                    XactOffset;
    union {
            struct cameractrl {
                    unsigned char                   formatidx;
                    unsigned char                   frameidx;
                    unsigned int                    camera;
                    unsigned int                    process;
                    char                            Rsvd[2];
            }__attribute__ ((packed)) CamaraCtrl;
            struct camerainfo {
                    unsigned char                   mjpeg_res_bitfield;
                    unsigned char                   h264_res_bitfield;
                    unsigned char                   yuv_res_bitfield;
                    unsigned char                   nv12_res_bitfield;
                    unsigned char                   i420_res_bitfield;
                    unsigned char                   m420_res_bitfield;
                    unsigned char                   camera[3];
                    unsigned char                   process[3];
            }__attribute__ ((packed)) CamaraInfo;
			
			struct cameradescriptor {  //  for JUVC_CONTROL_DESCRIPTOR
					unsigned short					idVendor;
					unsigned short					idProduct;
			} __attribute__ ((packed)) CamaraDescriptor;

			struct uvccontrol {
					unsigned char  bmRequestType;
					unsigned char  bRequest;
					unsigned short wValue;
					unsigned short wIndex;
					unsigned short wLength;
			}__attribute__ ((packed)) UvcControl;
			char Rsvd[12];
    } c;
} __attribute__ ((packed)) JUVCHDR, *PJUVCHDR;

//  JUVC_CONTROL_XXX
#define JURC_CONTROL_NONE                       0
#define JUVC_CONTROL_LOOKUP                     1
#define JUVC_CONTROL_CLNTINFO                   2
#define JUVC_CONTROL_CAMERAINFO                 3
#define JUVC_CONTROL_CAMERACTRL                 4
#define JUVC_CONTROL_MANUFACTURER               5
#define JUVC_CONTROL_PRODUCT                    6
#define JUVC_CONTROL_DESCRIPTOR                 7
#define JUVC_CONTROL_UVCCTRL_SET                8
#define JUVC_CONTROL_UVCCTRL_GET                9
#define JUVC_CONTROL_VC_DESC                    10
#define JUVC_CONTROL_VC_STOP                    11

//  JUVC_RES_XXX
#define JUVC_RESPONSE_ACK                       1


typedef struct tMnspXactHdr
{
    unsigned int              Tag;                    // 0000h Protocol tag. Must be MNSP_TAG.
    unsigned char             XactType;               // 0004h Transaction type. See MNSP_XACTTYPE_XXX.
    unsigned char             HdrSize;                // 0005h Header size, in byte. It means the payload start.
    unsigned char             XactId;                 // 0006h Transaction Id. Range: 0 ~ 255.
    unsigned char             Flags;                  // 0007h Flags.
    unsigned int              PayloadLength;          // 0008h Payload length, in byte.
    //unsigned char             Rsvd_0Ah[2];            // 000Ah Reserved. maybe payload length is not enough for command (tcp)
    unsigned int              TotalLength;            // 000Ch Total data length of the transaction
    unsigned int              XactOffset;             // 0010h Data offset at this transaction
	unsigned char 			 formatidx;
	unsigned char            frameidx;
	unsigned int             camera;
	unsigned int             process;
	char Rsvd[2];
    
} MNSPXACTHDR, *PMNSPXACTHDR;



int  TcpConnect(char* ipaddr ,int port ,int t);
int  TcpWrite(int clientSocket,char * buffer, int size);
int  TcpRead(int clientSocket,char * buffer, int size);
int  UdpInit();
int  UdpWrite(int sockfd ,char *ipaddr ,int port, char* buf,int size);
int UdpRead(int sockfd ,char *ipaddr ,int port, char* buf,int size);

int  udpImageWrite(int sockfd ,char *ipaddr ,int port,char id ,char* image_data , int total_size);
void closeSocket(int clientSocket);
void hex_dump(char *data, int size, char *caption);



#endif

