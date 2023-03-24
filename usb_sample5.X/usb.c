#include <xc.h>
#include <string.h>
#include <stdio.h>
#include "usb.h"

#define FCY 16000000UL  //__delay_ms���g�����߂ɕK�v
#include <libpic30.h>   //__delay_ms���g�����߂ɕK�v

/*
 * BDT��p�ӂ��܂�
 * Note: BDT �̃x�[�X�A�h���X�� 512 �o�C�g���E�ɃA���C�����g����K�v������܂�
 * �]���āAU1BDTP1 �̒l�͏�ɋ����ƂȂ�܂��B
 * 0x0800,0x0A00,0x0C00,0x0E00,0x1000,0x1200,0x1400,0x1600,
 * 0x1800,0x1A00,0x1C00,0x1E00,0x2000,0x2200,0x2400,0x2600
 */
BDT_TXRX BDT __attribute__((aligned(512))); //512�o�C�g�̋��E�Ń��������蓖��
uint8_t usbTxData[1024]; //USB���M�p�P�b�g�̃f�[�^�G���A���m��
uint8_t usbRxData[1024]; //USB��M�p�P�b�g�̃f�[�^�G���A���m��

void BDT_Initialize() {
    /* U1BDTP1�̐ݒ�́A�ȉ��̂悤�ɏ����Ă������̂ŁA���̒ʂ�v�Z������
     * (Desired BDT Base Address)/256
     */
    U1BDTP1 = (uint16_t) (&BDT) / 256;
    memset(&BDT, 0, sizeof (BDT)); //BDT�̒��g�����ׂ�0�ɂ���
    BDT.Tx.BUFFER_ADDRESS = (uint16_t) usbTxData; //USB���M�f�[�^�̃|�C���^��ݒ肷��
    BDT.Rx.BUFFER_ADDRESS = (uint16_t) usbRxData; //USB��M�f�[�^�̃|�C���^��ݒ肷��

    printf("*** BDT info ***\n");
    printf("sizeof(BDT):%d\n", sizeof (BDT));
    printf("U1BDTP1:%x\n", U1BDTP1);
    printf("BDT BaseAddress:%p\n", &BDT);
    printf("USB TxBufferAddress:%x\n", BDT.Tx.BUFFER_ADDRESS);
    printf("USB RxBufferAddress:%x\n", BDT.Rx.BUFFER_ADDRESS);
}

/********************
 * USB���������[�`��
 ********************/
void USB_Initialize() {
    BDT_Initialize(); //BDT�̏����������܂�
    /*
     * 27.3.4.1 USB �n�[�h�E�F�A�̗L����
     * USB ���W���[�����g�p����ɂ́A�\�t�g�E�F�A����
     * USBPWR �r�b�g (U1PWRC<0>) ���u1�v�ɃZ�b�g����K�v������܂��B
     * ����́A�N�����̃u�[�g�V�[�P���X�ōs�������ł��܂��B
     */
    U1PWRCbits.USBPWR = 1; // Turn power on

    /* 1.
     * HOSTEN �r�b�g���Z�b�g (U1CON<3> = 1) ���ăz�X�g���[�h��L���ɂ���B
     */
    U1CONbits.HOSTEN = 1;

    /* 2.
     * DPPULDNW ����� DMPULDWN �r�b�g���Z�b�g (U1OTGCON<5:4> = 1) ����
     * D+ ����� D- ���C���̃v���_�E����R��L���ɂ���B
     * DPPULUP ����� DMPULUP �r�b�g���N���A (U1OTGCON<7:6> = 0) ����
     * D+ ����� D- ���C���̃v���A�b�v��R�𖳌��ɂ���B
     */
    U1OTGCONbits.DPPULDWN = 1;
    U1OTGCONbits.DMPULDWN = 1;
    U1OTGCONbits.DPPULUP = 0;
    U1OTGCONbits.DMPULUP = 0;

    /* 3.
     * SOF �̐������J�n����B
     * SOF �J�E���^�̒l�� 12,000 �ɐݒ肷��B
     * SOFEN �r�b�g(U1CON<0>) �Ɂu0�v����������� SOF �p�P�b�g�̐����𖳌��ɂ���B
     */
    //U1SOF = 0b01001010;
    U1SOF = 12000;
    U1CONbits.SOFEN = 0;

    /* 4.
     * ATTACHIE �r�b�g (U1IE<6>) ���Z�b�g���ăf�o�C�X�ڑ����荞�݂�������B
     */
    U1IRbits.ATTACHIF = 1;
    U1IEbits.ATTACHIE = 1;

    /* 5.
     * �f�o�C�X�ڑ����荞�� ATTACHIF (U1IR<6>) ��҂B
     * ����́AUSB �f�o�C�X�� D+ �܂��� D- �̏�Ԃ��u0�v����u1�v(SE0 ���� JSTATE) �ɕύX���鎖�ɂ���Ēʒm�����B
     * �f�o�C�X�ڑ����荞�݂�����������A�f�o�C�X�̓d�������艻����܂ő҂� ( �ŏ��l 10ms�A�����l 100 ms)�B
     */
    while (U1IRbits.ATTACHIF == 0);
    __delay_ms(100);

    /* 6.
     * USB���䃌�W�X�^(U1CON)��JSTATE�����SE0�r�b�g�̏�Ԃ��`�F�b�N����
     * (JSTATE(U1CON<7>) = 0�Ȃ�ڑ��f�o�C�X�̓��[�X�s�[�h�A
     * ����ȊO�Ȃ�ڑ��f�o�C�X�̓t���X�s�[�h )�B
     */
    /* 7.
     * �ڑ��f�o�C�X�����[�X�s�[�h�̏ꍇ�A
     * �A�h���X ���W�X�^�̃��[�X�s�[�h �C�l�[�u���r�b�g LSPDEN (U1ADDR<7>) �ƁA
     * �G���h�|�C���g 0 ���䃌�W�X�^�̃��[�X�s�[�h �r�b�gLSPD (U1EP0<7>) ���Z�b�g����B
     * �ڑ��f�o�C�X���t���X�s�[�h�̏ꍇ�A�����̃r�b�g���N���A����B
     */
    switch (U1CONbits.JSTATE) {
        case 0://LowSpeed
            U1ADDRbits.LSPDEN = 1;
            U1EP0bits.LSPD = 1;
            printf("LowSpeed\n");
            break;
        case 1://FullSpeed
            U1ADDRbits.LSPDEN = 0;
            U1EP0bits.LSPD = 0;
            printf("FullSpeed\n");
            break;
    }

    /* 8.
     * ���Z�b�g�M�� (U1CON<4> = 1) �� 50 ms �ȏ㑗�M���� USB �f�o�C�X�����Z�b�g����B
     * 50ms ���o�߂�����A���Z�b�g���I������ (U1CON<4> = 0)�B
     */
    U1CONbits.RESET = 1;
    __delay_ms(50);
    U1CONbits.RESET = 0;

    /* 9.
     * �ڑ��f�o�C�X���T�X�y���h���[�h�Ɉڍs���Ȃ��悤�ASOFEN �r�b�g (U1CON<0>) ���Z�b�g����
     *  SOF �p�P�b�g������L���ɂ���B
     */
    U1CONbits.SOFEN = 1;

    /* 10.
     * �f�o�C�X�����Z�b�g���畜�A����܂� 10 ms �҂B
     */
    __delay_ms(10);

    /* 11.
     * �G�j�������[�V���� (USB 2.0 �d�l���� 9 �͎Q�� ) �����s����B
     */
    USB_enumeration();
}

/************************************
 * �G�j�������[�V�����������ɏ���
 ************************************/
void USB_enumeration() {
    /* 2.
     * �G���h�|�C���g���䃌�W�X�^�őo�����̃R���g���[���]����L���ɂ���
     * (U1EP0<4:0> = 0x0D)
     */
    U1EP0 = U1EP0 | 0x0d;

    /* 3.
     * �f�o�C�X�t���[�����[�N�Z�b�g�A�b�v�R�}���h�̃R�s�[���������o�b�t�@�ɒu���B
     * (�f�o�C�X �t���[�����[�N �R�}���h �Z�b�g�̏ڍׂ́AUSB 2.0 �d�l�� 9 �͎Q�� )
     * EPSON��S2R72A4x �V���[�Y�f�[�^�V�[�g�����֗�
     */
    uint8_t GET_DEVICE_DESCRIPTOR[] = {0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00};
    memcpy(usbTxData, GET_DEVICE_DESCRIPTOR, 8);

    /* 4.
     * �f�o�C�X�t���[�����[�N�R�}���h(���̏ꍇ��GET DEVICE DESCRIPTOR)��8�o�C�g�̃R�}���h�f�[�^��]���ł���悤��
     * ���݂�(EVEN�܂���ODD)TX EP0�o�b�t�@�f�B�X�N���v�^(BD)������������B
     * a) BD�X�e�[�^�X(BD0STAT)��0x8008(UOWN�r�b�g���Z�b�g���A�o�C�g�J�E���g��8)�ɐݒ肷��B
     * b) BD�f�[�^�o�b�t�@�A�h���X(BD0ADR)���A���̃R�}���h���܂� 8 �o�C�g�̃������o�b�t�@�̊J�n�A�h���X�ɐݒ肷��B
     */
    BDT.Tx.UOWN = 1;
    BDT.Tx.BYTE_COUNT = 8;
    /* 5.
     * �A�h���X���W�X�^(U1ADDR<6:0>)�Ƀ^�[�Q�b�g�f�o�C�X��USB�f�o�C�X �A�h���X��ݒ肷��B
     * USB �o�X�̃��Z�b�g��A�f�o�C�X�� USB �A�h���X�� 0 �ƂȂ�B
     * �G�j�������[�V������A���̒l�͕ʂ̒l (1 ? 127) �ɐݒ肳���B
     */
    U1ADDRbits.DEVADDR = 0;

    /* 6.
     * �g�[�N�����W�X�^�ɁA�^�[�Q�b�g�f�o�C�X�̊���̐���p�C�v�ł���
     * �G���h�|�C���g0�ɑ΂��� SETUP ���������� (U1TOK = 0xD0)�B
     * ����ɂ��A�o�X��SETUP�g�[�N�������s����A�����ăf�[�^�p�P�b�g�����M�����B
     * �p�P�b�g�̓]��������������ABD0STAT��PID�t�B�[���h�Ƀf�o�C�X�̃n���h�V�F�C�N���Ԃ����B
     * USB���W���[����BD0STAT���X�V����ƁA�]���������荞�݂��A�T�[�g����� (U1IR<3>)�B
     * �ȏ�ŁAUSB 2.0�d�l��9�͂Œ�߂�ꂽ�Z�b�g�A�b�v�g�����U�N�V�����̃Z�b�g�A�b�v �t�F�C�Y����������B
     */
    LATBbits.LATB2 = ~LATBbits.LATB2;
    U1TOK = 0xd0;
    while (U1IRbits.TRNIF == 0);
    U1IRbits.TRNIF = 1;

    /* 7.
     * ���ɁA�Z�b�g�A�b�v �g�����U�N�V�����̃f�[�^ �t�F�C�Y���J�n����
     * (���Ȃ킿�AGETDEVICE DESCRIPTOR�R�}���h�̃f�[�^���擾����)���߁A
     * ��M�f�[�^�i�[�p�Ƀ��������̃o�b�t�@��ݒ肷��B
     */

    /* 8.
     * �f�[�^��]���ł���悤��
     * ���݂�(EVEN �܂��� ODD)RX�܂���TX(IN�̏ꍇ��RX�AOUT�̏ꍇ��TX)EP0 BD������������B
     * a) BD�X�e�[�^�X(BD0STAT)��0xC040(UOWN�r�b�g���u1�v�A�f�[�^�g�O��(DTS)��DATA1�A
     *      �o�C�g�J�E���g���f�[�^�o�b�t�@�̒���(�����ł�64���Ȃ킿0x40))�ɐݒ肷��B
     * b) BD�f�[�^�o�b�t�@�A�h���X(BD0ADR)�Ƀf�[�^�o�b�t�@�̊J�n�A�h���X��ݒ肷��B
     */
    uint8_t *pBuffer = (uint16_t) usbRxData; //USB��M�f�[�^�o�b�t�@�̐擪�A�h���X���擾
    do {
        BDT.Rx.PID = 0; //�����ŏ���ɕύX�����PID�̃G���A���N���A
        BDT.Rx.UOWN = 1;
        BDT.Rx.DTS = 1;
        BDT.Rx.BYTE_COUNT = 0x40;
        BDT.Rx.BUFFER_ADDRESS = pBuffer;

        /*9.
         * �g�[�N�����W�X�^�ɁA�^�[�Q�b�g�f�o�C�X�̊���̐���p�C�v�ł���
         * �G���h�|�C���g0�ɑ΂���IN�܂���OUT�g�[�N������������
         * (�Ⴆ��GETDEVICEDESCRIPTOR�R�}���h�ɑ΂���IN�g�[�N���Ȃ�U1TOK=0x90)�B
         * ����ɂ��A�o�X��IN�g�[�N�������s����A�����ăf�o�C�X����z�X�g�փf�[�^�p�P�b�g�����M�����B
         * �f�[�^�p�P�b�g�̑��M������������ABD0STAT�ւ̏������݂��s���A�]���������荞�݂��A�T�[�g�����(U1IR<TRNIF>)�B
         * �R���g���[���]���̃f�[�^�t�F�C�Y��1�p�P�b�g�����Ȃ��ꍇ�A
         * �ȏ��USB2.0�d�l��9�͂Œ�߂�ꂽ�Z�b�g�A�b�v�g�����U�N�V�����̃f�[�^�t�F�C�Y����������B
         * ����Ƀf�[�^�]�����K�v�ȏꍇ�A�菇8�ɖ߂�B
         */
        LATBbits.LATB3 = ~LATBbits.LATB3;
        U1TOK = 0x90;
        while (U1IRbits.TRNIF == 0);
        U1IRbits.TRNIF = 1;
        pBuffer += BDT.Rx.BYTE_COUNT; //��M�f�[�^������̋L�^�J�n�A�h���X���ړ�����
    } while (BDT.Rx.BYTE_COUNT == 8); //�����W�o�C�g��M������W�����J��Ԃ�


    DEVICE_DESCRIPTOR_FORMAT descriptor;
    memcpy(&descriptor, usbRxData, 0x12);

    printf("*** descriptor ***\n");
    printf("bLength(1):%x\n", descriptor.bLength);
    printf("bDescriptorType(1):%x\n", descriptor.bDescriptorType);
    printf("bcdUSB(2):%x\n", descriptor.bcdUSB);
    printf("bDeviceClass(1):%x\n", descriptor.bDeviceClass);
    printf("bDeviceSubClass(1):%x\n", descriptor.bDeviceSubClass);
    printf("bDeviceProtocol(1):%x\n", descriptor.bDeviceProtocol);
    printf("bMaxPacketSize0(1):%x\n", descriptor.bMaxPacketSize0);
    printf("idVendor(2):%x\n", descriptor.idVendor);
    printf("idProduct(2):%x\n", descriptor.idProduct);
    printf("bcdDevice(2):%x\n", descriptor.bcdDevice);
    printf("iManufacturer(1):%x\n", descriptor.iManufacturer);
    printf("iProduct(1):%x\n", descriptor.iProduct);
    printf("iSerialNumber(1):%x\n", descriptor.iSerialNumber);
    printf("bNumConfigurations(1):%x\n", descriptor.bNumConfigurations);

    /* 10.
     * ���ɁA�Z�b�g�A�b�v�g�����U�N�V�����̃X�e�[�^�X�t�F�C�Y���J�n���邽�߂ɁA
     * ����0�̃X�e�[�^�X�t�F�C�Y�f�[�^�p�P�b�g�𑗐M�܂��͎�M�ł���悤��
     * ���������̃o�b�t�@��ݒ肷��B*/

    /* 11.
     * �X�e�[�^�X�f�[�^��]���ł���悤�ɁA���̎菇�Ō��݂�(EVEN�܂���ODD)TXEP0BD������������B
     * a)BD�X�e�[�^�X(BD0STAT)��0x8000(UOWN�r�b�g���u1�v�A�f�[�^�g�O��(DTS)��DATA0�A�o�C�g�J�E���g���u0�v)�ɐݒ肷��B
     * b)BDT�o�b�t�@�A�h���X�t�B�[���h���f�[�^�o�b�t�@�̊J�n�A�h���X�ɐݒ肷��B
     */

    /* 12.
     * �g�[�N�����W�X�^�ɁA�^�[�Q�b�g�f�o�C�X�̊���̐���p�C�v�ł���G���h�|�C���g0�ɑ΂���IN�܂���OUT�g�[�N������������
     * (�Ⴆ��GETDEVICEDESCRIPTOR�R�}���h�ɑ΂���OUT�g�[�N���Ȃ�U1TOK=0x10)
     * ����ɂ��A�o�X��OUT�g�[�N�������s����A�����ăz�X�g����f�o�C�X�֒���0�̃f�[�^�p�P�b�g�����M�����B
     * �f�[�^�p�P�b�g�̑��M������������A�f�o�C�X����̃n���h�V�F�C�N����BD���X�V����A
     * �]���������荞�݂��A�T�[�g�����(U1IR<3>)�B
     * �ȏ�ŁAUSB2.0�d�l��9�͂Œ�߂�ꂽ�Z�b�g�A�b�v�g�����U�N�V�����̃X�e�[�^�X�t�F�C�Y����������B*/
}

