#ifndef USB_H
#define USB_H

/***** BDTの構造を書いてみる *****/
typedef struct {

    union {

        struct {
            unsigned BYTE_COUNT : 10;
            unsigned BSTALL : 1;
            unsigned DTSEN : 1;
            unsigned : 1;
            unsigned : 1;
            unsigned DTS : 1;
            unsigned UOWN : 1;
            unsigned BUFFER_ADDRESS : 16;
        };

        struct {
            unsigned BYTE_COUNT : 10;
            unsigned PID : 4;
            unsigned DTS : 1;
            unsigned UOWN : 1;
            unsigned BUFFER_ADDRESS : 16;
        };
    };
} BDT_FORMAT;

typedef struct {
    BDT_FORMAT Rx __attribute__((packed));
    BDT_FORMAT Tx __attribute__((packed));
} BDT_TXRX;
/***** BDTの構造を書いてみる(ここまで) *****/

/***** DEVICE DESCRIPTORの構造を書いてみる *****/
typedef struct {
    unsigned bLength : 8;
    unsigned bDescriptorType : 8;
    unsigned bcdUSB : 16;
    unsigned bDeviceClass : 8;
    unsigned bDeviceSubClass : 8;
    unsigned bDeviceProtocol : 8;
    unsigned bMaxPacketSize0 : 8;
    unsigned idVendor : 16;
    unsigned idProduct : 16;
    unsigned bcdDevice : 16;
    unsigned iManufacturer : 8;
    unsigned iProduct : 8;
    unsigned iSerialNumber : 8;
    unsigned bNumConfigurations : 8;
} DEVICE_DESCRIPTOR_FORMAT;
/***** DEVICE DESCRIPTORの構造を書いてみる(ここまで) *****/


//プロトタイプ宣言
void USB_Initialize(void); //USBの初期化
void USB_enumeration(void); //エニュメレーション処理

#endif