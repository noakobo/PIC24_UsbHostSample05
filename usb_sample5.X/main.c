#include "mcc_generated_files/system.h"
#include "usb.h"

/* �������烁�C�� */
int main(void)
{
    // initialize the device
    SYSTEM_Initialize();
    USB_Initialize();

    while (1) {
        // Add your application code
    }

    return 1;
}

