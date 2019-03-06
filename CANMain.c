
// PIC18F66K80 Configuration Bit Settings


#include <xc.h>

#pragma config WDTEN = OFF
#pragma config XINST = OFF
#pragma config FOSC = HS1
#pragma config PLLCFG = ON
#pragma config CANMX = PORTB

#include "LCD.h"
#include <stdio.h>

#define MSG1_ID     0x123
#define MSG2_ID     0x0AA


void ConfigPins(void);
void ConfigCAN(void);
void ConfigSystem(void);
char WriteCANMsg(int msgID, void *data, unsigned char dataLen, unsigned char priority);
int ReadPot(void);

volatile char buttonPressed = 0;
volatile unsigned int msgCount = 0;
volatile char update;
volatile unsigned int id;
volatile unsigned char len;
volatile unsigned char data[8];

void main(void) {
    ConfigPins();
    ConfigSystem();
    ConfigCAN();
    LCDInit();
    lprintf(0, "CAN Lab");
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
    while (1) {
        if (update) {
            lprintf(1, "ID=%03x Data=%d", id, *((int *)data));
            update = 0;
        }
    }
}

void ConfigPins(void) {
    LATD = 0;
    TRISD = 0;
    TRISAbits.TRISA0 = 1;
}

void ConfigCAN(void) {
    TRISBbits.TRISB3 = 1; //CANRX must be input
    TRISBbits.TRISB2 = 0; //CANTX is output
    //Switch to config mode and wait for it
    CANCONbits.REQOP = 4;
    while (CANSTATbits.OPMODE != 4);
    
    //Now we're in config mode
    ECANCONbits.MDSEL = 0b00; //Legacy mode
    //Set baud to 125 Kbps
    BRGCON1bits.BRP = 15; //TQ = 1us
    BRGCON1bits.SJW = 0;
    BRGCON2bits.SEG2PHTS = 1; //Don't assign phase 2 automatically
    BRGCON2bits.SEG1PH = 3;  //Phase 1 = 4TQ
    BRGCON2bits.PRSEG = 0; //Propagation = 1 TQ 
    BRGCON3bits.SEG2PH = 1; //Phase 2 = 2 TQ

    //Set up Mask and filters for RB0
    RXM0SIDH = 0b00000000;  //Mask0
    RXM0SIDLbits.SID = 0b000; //Set mask to 0 to accept all id's to RXB0
    //Only accept standard messages in filters
    RXF0SIDLbits.EXIDEN = RXF1SIDLbits.EXIDEN = 0; 
    //Set F0 and F1 to accept 0x00
    //These filters are used by RXB0
    RXF0SIDH = 0b00000000;
    RXF0SIDLbits.SID = 0b000;
    RXF1SIDH = 0b00000000;
    RXF1SIDLbits.SID = 0b000;
    
    //Set up Mask and filters for RB1
    RXM1SIDH = 0b11111111;
    RXM1SIDLbits.SID = 0b111;  //Mask is set to all ones so filter must match
    RXF2SIDLbits.EXIDEN = RXF3SIDLbits.EXIDEN = RXF4SIDLbits.EXIDEN 
            = RXF5SIDLbits.EXIDEN = 0; //Only accept standard messages in all filters
    //F2 - F5 accept 0x00
    RXF2SIDH = 0b00000000;
    RXF2SIDLbits.SID = 0b000;
    RXF3SIDH = 0b00000000;
    RXF3SIDLbits.SID = 0b000;
    RXF4SIDH = 0b00000000;
    RXF4SIDLbits.SID = 0b000;
    RXF5SIDH = 0b00000000;
    RXF5SIDLbits.SID = 0b000;
    
    //Set up buffers with filters
    RXB0CONbits.RXM1 = RXB0CONbits.RXM0 = 0; //accept all messages based on filter
    //Use a trick here to keep RXB1 from accepting any messages
    RXB1CONbits.RXM1 = 1; //RXB1 will only accepts extended ID's but the
    RXB1CONbits.RXM0 = 0; //EXIDEN bit in the filter is set to standard so no ID's will be accepted
    
    //Mark RX0 as empty so it is ready to receive
    RXB0CONbits.RXFUL = 0;
    //And RX1 as well in case we use it later
    RXB1CONbits.RXFUL = 0;
    RXB0CONbits.RB0DBEN = 0; //Don't allow overflow to RXB1

    //Switch to operational mode and wait for it
    CANCONbits.REQOP = 0;
    while (CANSTATbits.OPMODE != 0);

    update = 0; //firmware buffer is empty
    //Set up CAN interrupts
    PIR5bits.RXB0IF = 0;
    PIE5bits.RXB0IE = 1; //Enable RXB0 interrupt
}

char WriteCANMsg(int msgID, void *data, unsigned char dataLen, unsigned char priority) {
    char i;
    if (TXB0CONbits.TXREQ == 0) {
        TXB0CONbits.TXPRI = priority;
        TXB0SIDH = msgID >> 3;
        TXB0SIDLbits.SID = (msgID & 0b111);
        TXB0SIDLbits.EXIDE = 0;
        TXB0DLC = dataLen & 0b1111;
        for (i = 0; i < dataLen; ++i) {
            //Use pointers to access the data and the data sfr's as arrays of bytes
            ((unsigned char*) &TXB0D0)[i] = ((unsigned char *) data)[i];
        }
        TXB0CONbits.TXREQ = 1;
        return 1;
    }
    return 0;
}

void ConfigSystem(void) {
    OSCTUNEbits.PLLEN = 1;
    ANCON0 = 1;
    ANCON1 = 0;
    ADCON1 = 0;
    ADCON2 = 0b10111010;
    RCONbits.IPEN = 0;
    TRISB = 0b00000001;
    INTCON2bits.INTEDG0 = 0;
    INTCONbits.INT0IF = 0;
    INTCONbits.INT0IE = 1;
}

int ReadPot(void) {
    int result;
    ADCON0bits.ADON = 1;
    ADCON0bits.CHS = 0;
    ADCON0bits.GODONE = 1;
    while (ADCON0bits.GODONE);
    result = ADRES;
    ADCON0bits.ADON = 0;
    return result;
}

void __interrupt(high_priority) HighISR(void) {
    int i;
    if (INTCONbits.INT0IF) {
        __delay_ms(10);
        if (!buttonPressed) {
            buttonPressed = 1;
            LATDbits.LATD0 = ~LATDbits.LATD0;
            ++msgCount;
            switch (msgCount % 2) {
                case 1: WriteCANMsg(MSG1_ID, (void *) &msgCount, sizeof (msgCount), 0);
                    break;
                case 0: WriteCANMsg(MSG2_ID, (void *) &msgCount, sizeof (msgCount), 0);
                    break;
            }
        } else {
            buttonPressed = 0;
        }
        INTCON2bits.INTEDG0 = ~INTCON2bits.INTEDG0;
        INTCONbits.INT0IF = 0;
    } else if (PIR5bits.RXB0IF) {
        id = RXB0SIDH;
        id <<= 3;
        id |= RXB0SIDLbits.SID;
        len = RXB0DLC & 0b1111;
        for (i = 0; i < len; ++i) {
            //use a pointer to the first data byte sfr so we can access the data
            //as an array of char
            data[i] = ((unsigned char *) &RXB0D0)[i];  
        }
        update = 1;
        RXB0CONbits.RXFUL = 0;
        PIR5bits.RXB0IF = 0;
    }
}
