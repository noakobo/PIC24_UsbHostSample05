#include <xc.h>
#include <string.h>
#include <stdio.h>
#include "usb.h"

#define FCY 16000000UL  //__delay_msを使うために必要
#include <libpic30.h>   //__delay_msを使うために必要

/*
 * BDTを用意します
 * Note: BDT のベースアドレスは 512 バイト境界にアライメントする必要があります
 * 従って、U1BDTP1 の値は常に偶数となります。
 * 0x0800,0x0A00,0x0C00,0x0E00,0x1000,0x1200,0x1400,0x1600,
 * 0x1800,0x1A00,0x1C00,0x1E00,0x2000,0x2200,0x2400,0x2600
 */
BDT_TXRX BDT __attribute__((aligned(512))); //512バイトの境界でメモリ割り当て
uint8_t usbTxData[1024]; //USB送信パケットのデータエリアを確保
uint8_t usbRxData[1024]; //USB受信パケットのデータエリアを確保

void BDT_Initialize() {
    /* U1BDTP1の設定は、以下のように書いてあったので、その通り計算させる
     * (Desired BDT Base Address)/256
     */
    U1BDTP1 = (uint16_t) (&BDT) / 256;
    memset(&BDT, 0, sizeof (BDT)); //BDTの中身をすべて0にする
    BDT.Tx.BUFFER_ADDRESS = (uint16_t) usbTxData; //USB送信データのポインタを設定する
    BDT.Rx.BUFFER_ADDRESS = (uint16_t) usbRxData; //USB受信データのポインタを設定する

    printf("*** BDT info ***\n");
    printf("sizeof(BDT):%d\n", sizeof (BDT));
    printf("U1BDTP1:%x\n", U1BDTP1);
    printf("BDT BaseAddress:%p\n", &BDT);
    printf("USB TxBufferAddress:%x\n", BDT.Tx.BUFFER_ADDRESS);
    printf("USB RxBufferAddress:%x\n", BDT.Rx.BUFFER_ADDRESS);
}

/********************
 * USB初期化ルーチン
 ********************/
void USB_Initialize() {
    BDT_Initialize(); //BDTの初期化をします
    /*
     * 27.3.4.1 USB ハードウェアの有効化
     * USB モジュールを使用するには、ソフトウェアから
     * USBPWR ビット (U1PWRC<0>) を「1」にセットする必要があります。
     * これは、起動時のブートシーケンスで行う事ができます。
     */
    U1PWRCbits.USBPWR = 1; // Turn power on

    /* 1.
     * HOSTEN ビットをセット (U1CON<3> = 1) してホストモードを有効にする。
     */
    U1CONbits.HOSTEN = 1;

    /* 2.
     * DPPULDNW および DMPULDWN ビットをセット (U1OTGCON<5:4> = 1) して
     * D+ および D- ラインのプルダウン抵抗を有効にする。
     * DPPULUP および DMPULUP ビットをクリア (U1OTGCON<7:6> = 0) して
     * D+ および D- ラインのプルアップ抵抗を無効にする。
     */
    U1OTGCONbits.DPPULDWN = 1;
    U1OTGCONbits.DMPULDWN = 1;
    U1OTGCONbits.DPPULUP = 0;
    U1OTGCONbits.DMPULUP = 0;

    /* 3.
     * SOF の生成が開始する。
     * SOF カウンタの値を 12,000 に設定する。
     * SOFEN ビット(U1CON<0>) に「0」を書き込んで SOF パケットの生成を無効にする。
     */
    //U1SOF = 0b01001010;
    U1SOF = 12000;
    U1CONbits.SOFEN = 0;

    /* 4.
     * ATTACHIE ビット (U1IE<6>) をセットしてデバイス接続割り込みを許可する。
     */
    U1IRbits.ATTACHIF = 1;
    U1IEbits.ATTACHIE = 1;

    /* 5.
     * デバイス接続割り込み ATTACHIF (U1IR<6>) を待つ。
     * これは、USB デバイスが D+ または D- の状態を「0」から「1」(SE0 から JSTATE) に変更する事によって通知される。
     * デバイス接続割り込みが発生したら、デバイスの電源が安定化するまで待つ ( 最小値 10ms、推奨値 100 ms)。
     */
    while (U1IRbits.ATTACHIF == 0);
    __delay_ms(100);

    /* 6.
     * USB制御レジスタ(U1CON)のJSTATEおよびSE0ビットの状態をチェックする
     * (JSTATE(U1CON<7>) = 0なら接続デバイスはロースピード、
     * それ以外なら接続デバイスはフルスピード )。
     */
    /* 7.
     * 接続デバイスがロースピードの場合、
     * アドレス レジスタのロースピード イネーブルビット LSPDEN (U1ADDR<7>) と、
     * エンドポイント 0 制御レジスタのロースピード ビットLSPD (U1EP0<7>) をセットする。
     * 接続デバイスがフルスピードの場合、これらのビットをクリアする。
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
     * リセット信号 (U1CON<4> = 1) を 50 ms 以上送信して USB デバイスをリセットする。
     * 50ms が経過したら、リセットを終了する (U1CON<4> = 0)。
     */
    U1CONbits.RESET = 1;
    __delay_ms(50);
    U1CONbits.RESET = 0;

    /* 9.
     * 接続デバイスがサスペンドモードに移行しないよう、SOFEN ビット (U1CON<0>) をセットして
     *  SOF パケット生成を有効にする。
     */
    U1CONbits.SOFEN = 1;

    /* 10.
     * デバイスがリセットから復帰するまで 10 ms 待つ。
     */
    __delay_ms(10);

    /* 11.
     * エニュメレーション (USB 2.0 仕様書第 9 章参照 ) を実行する。
     */
    USB_enumeration();
}

/************************************
 * エニュメレーションをここに書く
 ************************************/
void USB_enumeration() {
    /* 2.
     * エンドポイント制御レジスタで双方向のコントロール転送を有効にする
     * (U1EP0<4:0> = 0x0D)
     */
    U1EP0 = U1EP0 | 0x0d;

    /* 3.
     * デバイスフレームワークセットアップコマンドのコピーをメモリバッファに置く。
     * (デバイス フレームワーク コマンド セットの詳細は、USB 2.0 仕様第 9 章参照 )
     * EPSONのS2R72A4x シリーズデータシートが超便利
     */
    uint8_t GET_DEVICE_DESCRIPTOR[] = {0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00};
    memcpy(usbTxData, GET_DEVICE_DESCRIPTOR, 8);

    /* 4.
     * デバイスフレームワークコマンド(この場合はGET DEVICE DESCRIPTOR)の8バイトのコマンドデータを転送できるように
     * 現在の(EVENまたはODD)TX EP0バッファディスクリプタ(BD)を初期化する。
     * a) BDステータス(BD0STAT)を0x8008(UOWNビットをセットし、バイトカウントを8)に設定する。
     * b) BDデータバッファアドレス(BD0ADR)を、このコマンドを含む 8 バイトのメモリバッファの開始アドレスに設定する。
     */
    BDT.Tx.UOWN = 1;
    BDT.Tx.BYTE_COUNT = 8;
    /* 5.
     * アドレスレジスタ(U1ADDR<6:0>)にターゲットデバイスのUSBデバイス アドレスを設定する。
     * USB バスのリセット後、デバイスの USB アドレスは 0 となる。
     * エニュメレーション後、この値は別の値 (1 ? 127) に設定される。
     */
    U1ADDRbits.DEVADDR = 0;

    /* 6.
     * トークンレジスタに、ターゲットデバイスの既定の制御パイプである
     * エンドポイント0に対する SETUP を書き込む (U1TOK = 0xD0)。
     * これにより、バスにSETUPトークンが発行され、続いてデータパケットが送信される。
     * パケットの転送が完了したら、BD0STATのPIDフィールドにデバイスのハンドシェイクが返される。
     * USBモジュールがBD0STATを更新すると、転送完了割り込みがアサートされる (U1IR<3>)。
     * 以上で、USB 2.0仕様第9章で定められたセットアップトランザクションのセットアップ フェイズが完了する。
     */
    LATBbits.LATB2 = ~LATBbits.LATB2;
    U1TOK = 0xd0;
    while (U1IRbits.TRNIF == 0);
    U1IRbits.TRNIF = 1;

    /* 7.
     * 次に、セットアップ トランザクションのデータ フェイズを開始する
     * (すなわち、GETDEVICE DESCRIPTORコマンドのデータを取得する)ため、
     * 受信データ格納用にメモリ内のバッファを設定する。
     */

    /* 8.
     * データを転送できるように
     * 現在の(EVEN または ODD)RXまたはTX(INの場合はRX、OUTの場合はTX)EP0 BDを初期化する。
     * a) BDステータス(BD0STAT)を0xC040(UOWNビットを「1」、データトグル(DTS)をDATA1、
     *      バイトカウントをデータバッファの長さ(ここでは64すなわち0x40))に設定する。
     * b) BDデータバッファアドレス(BD0ADR)にデータバッファの開始アドレスを設定する。
     */
    uint8_t *pBuffer = (uint16_t) usbRxData; //USB受信データバッファの先頭アドレスを取得
    do {
        BDT.Rx.PID = 0; //ここで勝手に変更されるPIDのエリアをクリア
        BDT.Rx.UOWN = 1;
        BDT.Rx.DTS = 1;
        BDT.Rx.BYTE_COUNT = 0x40;
        BDT.Rx.BUFFER_ADDRESS = pBuffer;

        /*9.
         * トークンレジスタに、ターゲットデバイスの既定の制御パイプである
         * エンドポイント0に対するINまたはOUTトークンを書き込む
         * (例えばGETDEVICEDESCRIPTORコマンドに対するINトークンならU1TOK=0x90)。
         * これにより、バスにINトークンが発行され、続いてデバイスからホストへデータパケットが送信される。
         * データパケットの送信が完了したら、BD0STATへの書き込みが行われ、転送完了割り込みがアサートされる(U1IR<TRNIF>)。
         * コントロール転送のデータフェイズが1パケットしかない場合、
         * 以上でUSB2.0仕様第9章で定められたセットアップトランザクションのデータフェイズが完了する。
         * さらにデータ転送が必要な場合、手順8に戻る。
         */
        LATBbits.LATB3 = ~LATBbits.LATB3;
        U1TOK = 0x90;
        while (U1IRbits.TRNIF == 0);
        U1IRbits.TRNIF = 1;
        pBuffer += BDT.Rx.BYTE_COUNT; //受信データ分次回の記録開始アドレスを移動する
    } while (BDT.Rx.BYTE_COUNT == 8); //もし８バイト受信したら８項を繰り返す


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
     * 次に、セットアップトランザクションのステータスフェイズを開始するために、
     * 長さ0のステータスフェイズデータパケットを送信または受信できるように
     * メモリ内のバッファを設定する。*/

    /* 11.
     * ステータスデータを転送できるように、次の手順で現在の(EVENまたはODD)TXEP0BDを初期化する。
     * a)BDステータス(BD0STAT)を0x8000(UOWNビットを「1」、データトグル(DTS)をDATA0、バイトカウントを「0」)に設定する。
     * b)BDTバッファアドレスフィールドをデータバッファの開始アドレスに設定する。
     */

    /* 12.
     * トークンレジスタに、ターゲットデバイスの既定の制御パイプであるエンドポイント0に対するINまたはOUTトークンを書き込む
     * (例えばGETDEVICEDESCRIPTORコマンドに対するOUTトークンならU1TOK=0x10)
     * これにより、バスにOUTトークンが発行され、続いてホストからデバイスへ長さ0のデータパケットが送信される。
     * データパケットの送信が完了したら、デバイスからのハンドシェイク情報でBDが更新され、
     * 転送完了割り込みがアサートされる(U1IR<3>)。
     * 以上で、USB2.0仕様第9章で定められたセットアップトランザクションのステータスフェイズが完了する。*/
}

