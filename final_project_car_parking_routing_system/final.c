#include <msp430fr5969.h>
#include <stdint.h>
#include <stdio.h>
#define WRITE_SIZE 4
void FRAMWrite(void);
#pragma PERSISTENT(FRAM_write)
uint8_t FRAM_write[WRITE_SIZE] = {0};
void RTC_Init(void);

typedef enum                            // State values of main FSM:
{
    IDLE,                           // 0:   Idle
    M_C,                            // 1:   CPRS_ 'C'
    M_P,                            // 2:   CPRS_ 'P'
    M_R,                            // 3:   CPRS_ 'R'
    M_S,                            // 4:   CPRS_ 'S'
    M__,                            // 5:   CPRS_ '_'
    R_R,                            // 6:   RESET 'R'
    R_E,                            // 7:   RESET 'E'
    R_S,                            // 8:   RESET 'S'
    D_D,                            // 9:   DIRECTION 'D'
    D_I,                            // 10:  DIRECTION 'I'
    D_R,                            // 11:  DIRECTION 'R'
    L_C,                            // 12:  CLOSE/COUNT 'C'
    L_L,                            // 13:  CLOSE 'L'
    L_S,                            // 14:  CLOSE 'S'
    L_O,                            // 15:  OPEN 'O'
    L_P,                            // 16:  OPEN 'P'
    L_N,                            // 17:  OPEN 'N'
    S_M,                            // 18:  MAX 'M'
    S_A,                            // 19:  MAX 'A'
    S_X,                            // 20:  MAX 'X'
    SX1,                            // 21:  MAX 'Number1'
    SX2,                            // 22:  MAX 'Number2'
    C_N,                            // 23:  COUNT 'N'
    C_T,                            // 24:  COuNT 'T'
    P_S,                            // 25:  SPEED 'S'
    P_P,                            // 26:  SPEED 'P'
    P_D,                            // 27:  SPEED 'D'

} STATES_MAIN;

volatile uint8_t state = IDLE;
volatile uint8_t dir;               // Direction 0 or 1
volatile uint8_t cnt;               // number of vehicle
volatile uint8_t max;               // storage capacity
volatile uint8_t m = 0;             // count sensor S2 when pressed
volatile uint8_t j = 0;             // count sensor S1 when pressed
volatile uint16_t t = 0;            // capture time
volatile uint32_t average = 0;        // average speed of vehicle
volatile  uint8_t watchdogstate = 0;// watchdog timer state
volatile uint8_t daynight;          // 0 if time is 20-8, 1 if time is 8-20

/* SERIAL INTERFACE */
#define RXBUF 32                        // Size of circular receiver buffer.
volatile uint8_t rxBuf[RXBUF];          // The circular buffer.
volatile uint8_t rxBufS = 0;            // Start: head position.
volatile uint8_t rxBufE = 0;            // End: tail position.
static uint8_t rxChar = 0;              // store passed character

void parse_main(uint8_t *data);         // parsing the character
void uartTx(uint8_t data);              // transmit data to terminal
void bdiv(uint32_t dividend, uint32_t divisor);

/* TIMEOUT TIMER */
void start_timeout(void);               // Initialize timeout watchdog timer.
void reset_timeout(void);               // Reset timeout watchdog timer.
void stop_timeout(void);                // Stop timeout watchdog timer.

/* MAIN PROGRAM */
void main(void)
{
    // Stop watchdog timer.
    WDTCTL = WDTPW | WDTHOLD;
    // Initialize the clock system to generate 1 MHz DCO clock.
    CSCTL0_H    = CSKEY_H;              // Unlock CS registers.
    CSCTL1      = DCOFSEL_0;            // Set DCO to 1 MHz, DCORSEL for
                                        //   high speed mode not enabled.
    CSCTL2      = SELA__VLOCLK |        // Set ACLK = VLOCLK = 10 kHz.
                  SELS__DCOCLK |        //   Set SMCLK = DCOCLK.
                  SELM__DCOCLK;         //   Set MCLK = DCOCLK.
                                        // SMCLK = MCLK = DCOCLK = 1 MHz.
    CSCTL3      = DIVA__1 |             //   Set ACLK divider to 1.
                  DIVS__1 |             //   Set SMCLK divider to 1.
                  DIVM__1;              //   Set MCLK divider to 1.
                                        // Set all dividers to 1.
    CSCTL0_H    = 0;                    // Lock CS registers.

    // Initialize unused GPIOs to minimize energy-consumption.
    // Port 1:
    P1DIR = 0xFF;
    P1OUT = 0x00;
    // Port 2:
    P2DIR = 0xFF;
    P2OUT = 0x00;
    // Port 3:
    P3DIR = 0xFF;
    P3OUT = 0x00;
    // Port 4:
    P4DIR = 0xFF;
    P4OUT = 0x00;
    // Port J:
    PJDIR = 0xFFFF;
    PJOUT = 0x0000;

    // Initialize port 1:
    P1DIR |= BIT0;                      // P1.0 - output for LED2, off.
    P1OUT &= ~BIT0;
    P1DIR &= ~BIT1;                     // P1.1 - input for S2, pullup.
    P1REN |= BIT1;
    P1OUT |= BIT1;
    // Initialize port 4:
    P4DIR |= BIT6;                      // P4.6 - output for LED1, off.
    P4OUT &= ~BIT6;
    P4DIR &= ~BIT5;                     // P4.5 - input for S1, pullup.
    P4REN |= BIT5;
    P4OUT |= BIT5;

    // Initialize port interrupts:
    P1IES |= BIT1;                      // P1.1 - falling edge detection.
    P1IE |= BIT1;                       //   Port interrupt enabled.
    P4IES |= BIT5;                      // P4.5 - falling edge detection.
    P4IE |= BIT5;                       //   Port interrupt enabled.

    // Initialize port 2:
    // Select Tx and Rx functionality of eUSCI0 for hardware UART.
    // P2.0 - UART Tx (UCA0TXD).
    // P2.1 - UART Rx (UCA0RXD).
    P2SEL0 &= ~(BIT1 | BIT0);
    P2SEL1 |= BIT1 | BIT0;

    PJSEL0 = BIT4 | BIT5;                   // Initialize LFXT pins

    // Disable the GPIO power-on default high-impedance mode to activate the
    // previously configured port settings.
    PM5CTL0 &= ~LOCKLPM5;

    /* Initialize serial UART interface */
    UCA0CTLW0 = UCSWRST;                // Enable software reset.
    UCA0CTLW0 |= UCSSEL__SMCLK;         // Select clock source SMCLK = 1 MHz.
    // Set Baud rate of 9600 Bd.
    // Recommended settings available in table 30-5, p. 779 of the User's Guide.
    UCA0BRW = 6;                        // Clock prescaler of the
                                           //   Baud rate generator.
    UCA0MCTLW = UCBRF_8 |               // First modulations stage.
                UCBRS5 |                // Second modulation stage.
                UCOS16;                 // Enable oversampling mode.
    UCA0CTLW0 &= ~UCSWRST;              // Disable software reset and start
                                           //   eUSCI state machine.
    UCA0IE |= UCRXIE;

    // Clear interrupt flags that have been raised due to high-impedance settings.
    P1IFG &= ~BIT1;
    P4IFG &= ~BIT5;

    // Enable interrupts globally.
    __bis_SR_register(GIE);

    if (SYSRSTIV == SYSRSTIV_LPM5WU){               // system is waked up from LPM3.5 when reset vector SYSRSTIV points  PMMLPM5IFG = 1
        RTCCTL01 &= ~RTCAIE;                        // disable RTC alarm interrupt
        RTCCTL01 = RTCRDYIE | RTCBCD | RTCHOLD;     // RTC in BCD and hold mode
        RTCAHOUR = 0;                                 // reset alarm
        RTCAHOUR |= RTCAE;                            // enable alarm
        RTCAHOUR |= 0x20;                                // set alarm at 20:00 O'clock every day
        daynight = 1;
        RTCCTL01 |= RTCAIE;                          // enable alarm interrupt
        RTCCTL01 &= ~(RTCHOLD);                      // calendar mode
        cnt = FRAM_write[0];                         // retrieve count
        dir = FRAM_write[1];                         // retrieve direction
        max = FRAM_write[2];                         // retrieve storage
    }
    else{
        RTC_Init();                                  // initialize RTC for the first time
    }

    /* MAIN LOOP */
    while(1){
        rxChar = rxBuf[rxBufS];                       // store present character
        if (rxBufS != rxBufE){                        // if character is present
            parse_main(&rxChar);               // Parse the character.
            rxBufS++;
            if(rxBufS > 32){                    // if character exceed the buffer size
                rxBufS = 0;
            }
        }
        else{
            if(watchdogstate == 1){             // state being hold for more than 0.8 sec
                stop_timeout();
                watchdogstate = 0;
                state = IDLE;                   //  state back to IDLE
            }
            if (daynight == 0 ){                // check for the time to go sleep 0 goes to sleep
                FRAMWrite();                    // recover important information
                RTCAHOUR = 0;
                RTCAHOUR |= RTCAE;
                RTCAHOUR |= 8;                   // before going to sleep set alarm at 8am
                P4IE &= ~BIT5;                   // Enable port interrupt again.
                P1IE &= ~BIT1;                   // Enable port interrupt again.
                PMMCTL0_H = PMMPW_H;                      // Open PMM Registers for write
                PMMCTL0_L |= PMMREGOFF;                   // and set PMMREGOFF
                __bis_SR_register(LPM3_bits | GIE);     // Enter LPM3.5 mode interrupts enabled
            }
            __bis_SR_register(LPM4_bits | GIE);     // Enter LPM4 mode interrupts enabled
        }
    }
}

/* initialize RTC*/
void RTC_Init(void){
    cnt = 0;                                    // count = 0, default direction = 0 and default storage is 10
    dir = 0;
    max = 10;
    RTCCTL01 = RTCRDYIE | RTCBCD | RTCHOLD;    // RTC initialized in BCD and hold mode
    RTCYEAR = 0x2021;                       // Year = 0x2021
    RTCMON = 0x2;                           // Month = 0x02 = February
    RTCDAY = 0x01;                          // Day = 0x01 = 1st
    RTCDOW = 0x01;                          // Day of week = 0x01 = Monday
    RTCHOUR = 0x8;                         // Hour = 0x10
    RTCMIN = 0x0;                          // Minute = 0x0
    RTCSEC = 0x0;                          // Seconds = 0x55
    RTCAHOUR = 0;                           // all other alarm disabled except hour alarm
    RTCADOW  = 0;
    RTCADAY = 0;
    RTCAMIN = 0;
    RTCAHOUR = RTCAE;                       // enable hour alarm
    RTCAHOUR |= 0x8;
    RTCCTL01 |= RTCAIE;
    daynight = 1;

    RTCCTL01 &= ~(RTCHOLD);                 // Start RTC
    }

/* store critical values before power off */
void FRAMWrite(void)
{

    FRAM_write[0] = cnt;
    FRAM_write[1] = dir;
    FRAM_write[2] = max;
}

/* parsing the character*/
void parse_main(uint8_t *data)
{
    static uint16_t cn = 0;
    static uint16_t cm = 0;

    switch(state)
    {
    case IDLE:                // IDLE
        if(*data == 'C')
        {
            start_timeout();
            state  = M_C;    // CPRS_
        }
        break;
    case M_C:
        if(*data == 'P')        //CPRS_
        {
            reset_timeout();
            state  = M_P;       //CPRS_
        }
        else{
            state = IDLE;
            stop_timeout();
        }
        break;
    case M_P:                    // CPRS_
            if(*data == 'R')
            {
                reset_timeout();
                state  = M_R;
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case M_R:                    // CPRS_
            if(*data == 'S')
            {
                reset_timeout();
                state  = M_S;    // CPRS_
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case M_S:                    // CPRS_
               if(*data == '_')
               {
                   reset_timeout();
                   state  = M__;    // CPRS_
               }
               else{
                   state = IDLE;
                   stop_timeout();
               }
               break;
    case M__:                    // CPRS_
            if(*data == 'R')
            {
                reset_timeout();
                state  = R_R;    // RESET
            }
            else if(*data == 'D') {
                reset_timeout();
                state = D_D;        //DIRECTION
            }
            else if(*data == 'C') {
                reset_timeout();
                state = L_C;        //CLOSE OR COUNT
            }
            else if(*data == 'O') {
                reset_timeout();
                state = L_O;        // OPEN
            }
            else if(*data == 'M') {
                reset_timeout();
                state = S_M;        // MAX
            }
            else if(*data == 'S') {
                reset_timeout();
                state = P_S;        //SPEED
            }
            else{
                state = IDLE;
                stop_timeout();
            }

            break;
   case R_R:                 // IDLE
            if(*data == 'E')
            {
                reset_timeout();
                state  = R_E;    // REST
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case R_E:              // RESET
            if(*data == 'S')
            {
                max = 10;                    // reset the values
                dir = 0;
                cnt = 0;
                average = 0;
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case D_D:                    // DIRECTION
            if(*data == 'I')
            {
                reset_timeout();
                state  = D_I;    // DIRECTION
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case D_I:                    // DIRECTION
            if(*data == 'R')
            {
                reset_timeout();
                state  = D_R;    // DIRECTION
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case D_R:                  // DIRECTION
            if(*data == '1')
            {
                reset_timeout();
                dir = 0;                    // ENTER FIRST S2 AND S1
            }
            else if (*data == '2'){
                reset_timeout();
                dir = 1;                    // ENTER FIRST S1 AND S2
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case L_C:                    // CLOSE OR COUNT
            if(*data == 'L')    // CLOSE
            {
                reset_timeout();
                state  = L_L;    // CLOSE
            }
            else if (*data == 'N'){     // COUNT
                reset_timeout();
                state = C_N;
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case L_L:                    // CLOSE
            if(*data == 'S')
            {
                if (cnt < max){     // CLOSE THE LED RED ID RED IS GLOWING
                    P1OUT &= ~BIT0;
                    state = IDLE;
                    stop_timeout();
                }
                else{
                    P4OUT &= ~BIT6;     // CLOSE THE LED GREEN IS GREEN IS GLOWING
                    state = IDLE;
                    stop_timeout();
                }
            }
            break;
    case L_O:                    // OPEN
            if(*data == 'P')
            {
                reset_timeout();
                state = L_P;        //OPEN
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case L_P:                    // OPEN
            if(*data == 'N')
            {
                if (cnt < max){         // OPEN GREEN IF COUNT IS LESS THAN MAX OTHERWISE OPEN RED LED
                    reset_timeout();
                    P1OUT |= BIT0;
                    state = IDLE;
                    stop_timeout();
                }
                else{
                    reset_timeout();
                    P4OUT |= BIT6;
                    state = IDLE;
                    stop_timeout();
                }
            }
            break;
    case S_M:                    // MAX
            if(*data == 'A')
            {
                reset_timeout();
                state = S_A;
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case S_A:                   // MAX
            if(*data == 'X')
            {
                reset_timeout();
                state = S_X;
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case S_X:                  // MAX
            if(*data >= 48 && *data <= 57)          // CHECK THE NUMBER
            {
                reset_timeout();
                cm = *data-48;
                state = SX1;
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case SX1:                  // MAX IF DIGIT IS 2 DIGIT
                if(*data >= 48 && *data <= 57)
                {
                    reset_timeout();
                    cn = cm*100+(*data-48)*10;
                    cm = cm*10+(*data-48);
                    state = SX2;
                }
                else{
                    max = cm;
                    state = IDLE;
                    stop_timeout();
                }
                break;
    case SX2:                  // MAX IF DATA IS 3 DIGIT
                   if(*data >= 48 && *data <= 57)
                   {
                       reset_timeout();
                       cn = cn+(*data-48);
                       max = cn;
                       state = IDLE;
                       stop_timeout();
                   }
                   break;
    case C_N:                    // DISPALY COUNT
            if(*data == 'T')
            {
                reset_timeout();
                cn = (cnt / 100) + 48;
                cm = (cnt % 100);
                uartTx(cn);
                cn = (cm / 10) + 48;
                uartTx(cn);
                cn = (cm % 10) + 48;
                uartTx(cn);
            }
            else{
                state = IDLE;
                stop_timeout();
            }
            break;
    case P_S:                    // SPEED
               if(*data == 'P')
               {
                   reset_timeout();
                   state = P_P;
               }
               else{
                   state = IDLE;
                   stop_timeout();
               }
               break;
    case P_P:                    // DISPLAY SPEED
               if(*data == 'D')
               {
                   reset_timeout();
                   cn = (average / 10000) + 48;
                   cm = (average % 10000);
                   uartTx(cn);
                   cn = (cm / 1000) + 48;
                   cm = (cm % 1000);
                   uartTx(cn);
                   cn = (cm / 100) + 48;
                   cm = (cm % 100);
                   uartTx(cn);
                   cn = 46;
                   uartTx(cn);
                   cn = (cm / 10)+48;
                   uartTx(cn);
                   cn = (cm % 10) + 48;
                   uartTx(cn);
               }
               else{
                   state = IDLE;
                   stop_timeout();
               }
               break;
    default:
        state = IDLE;
        stop_timeout();
    }
}

void uartTx(uint8_t data)
{
    while((UCA0STATW & UCBUSY));    // Wait while module is busy with data.
    UCA0TXBUF = data;            // Transmit element i of data array.
}

#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
    switch(__even_in_range(UCA0IV, USCI_UART_UCTXCPTIFG))
    {
    case USCI_NONE:                     // No interrupts
        break;
    case USCI_UART_UCRXIFG:             // Received data
        rxChar = UCA0RXBUF;             // store the received character
        rxBuf[rxBufE] = rxChar;         // push into buffer
        rxBufE++;                       // increment tail of buffer
        if(rxBufE > 32){
            rxBufE = 0;
        }
        if(rxBufE == rxBufS){
            rxBufE--;
        }
        __bic_SR_register_on_exit(LPM4_bits);      // CLEAR LMP4 MODE
        break;
    case USCI_UART_UCTXIFG:             // Transmit data
        break;
    case USCI_UART_UCSTTIFG:            //
        break;
    case USCI_UART_UCTXCPTIFG:          //
        break;
    }
}

/* ### BUTTON DEBOUNCING ### */

/* ISR PORT 1 */
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    switch(__even_in_range(P1IV, P1IV_P1IFG7))
    {
    case P1IV_P1IFG0:                   // P1.0
        break;
    case P1IV_P1IFG1:                   // P1.1
        /* COUNT CAPTURE FOR S2  */
        P1IE &= ~BIT1;                  // Disable port interrupt during delay.
        m++;
        if(m>2){
            m = 0;
            j = 0;
        }
        if ((m == j) && (j == 1)) {                    // ONLY IF BOTH BUTTONS ARE PRESSED
            TA0CCTL0 = CM_3 |         // CAPTURE VALUE ON BOTH EDGE
                       CCIS_2 |      // SELECT INPUT GROUND
                       CAP;         // CAPTURE MODE ENABLE
            TA0CCTL0 |= CCIS_3;     // SELECT INPUT VCC
            t = TA0CCR0;
            if(average == 0){                       // average calculation = 50 *60 *60 * 10^-6/t*10^-4 = 180000 /TA0CCR0
                average = 180000/TA0CCR0;           // 10^-2 removed and is added while displaying it on terminal.
            }
            else{
                average = (average + 180000/TA0CCR0) >> 1;
            }
            TA0CTL = TACLR;
        }

        if ((j == m) && (dir == 1) && (cnt < max) && (j==2)){     // ONLY COUNTS IF BOTH BUTTONS ARE PRESSED AND COUNT IS LESS THAN MAX
            cnt++;
            m = 0;
            j = 0;
            if(cnt == max){             // IF NO PARKING SPACE THEN GLOW RED LED
                P4OUT |= BIT6;
                TA1CCR0 = 1999;                 // 200ms
                TA1CTL = TASSEL__ACLK |        // Select clock source ACLK, 1 MHz.
                         MC__CONTINUOUS;        // Start timer in continuous mode.
                TA1CCTL0 &= ~CCIFG;             // CLEAR INTERRUPT FLAG
                TA1CCTL0 |= CCIE;               // Enable interrupt capability.
                __bis_SR_register_on_exit(LPM3_bits);       // ENABLE LMP3 MODE TO GLOW LED FOR 200 MS
                P4OUT &= ~BIT6;                 // CLEAR LED
            }
        }
        else if ((j == m) && (dir == 0) && (j == 2)){
            m = 0;
            j = 0;
            if (cnt != 0){
                cnt--;
                if(cnt == max-1){               // IF THERE IS A SPACE AVAILABLE THEN GLOW GREEN LED
                    P1OUT |= BIT0;
                    TA1CCR0 = 1999;                 // // 200ms
                    TA1CTL = TASSEL__ACLK |        // Select clock source SMCLK, 1 MHz.
                             MC__CONTINUOUS;        // Start timer in continuous mode.
                    TA1CCTL0 &= ~CCIFG;             // CLEAR INTERRUPT FLAG
                    TA1CCTL0 |= CCIE;               // Enable interrupt capability.
                    __bis_SR_register_on_exit(LPM3_bits);   // ENALBE LMP3 MODE
                    P1OUT &= ~BIT0;
                }
            }
        }

        /* Timer-based delay of 25 ms */
        // Initialize timer A1 for a delay of 25 ms:
        TA1CCR0 =  249;                 // Compare value of 249 with CCR1.
        TA1CTL = TASSEL__ACLK |        // Select clock source ACLK, 1 MHz.
                 MC__CONTINUOUS;        // Start timer in continuous mode.
        TA1CCTL0 &= ~CCIFG;
        TA1CCTL0 |= CCIE;               // Enable interrupt capability.
        TA0CTL = TASSEL__ACLK |        // Select clock source ACLK, 1 MHz.
                 MC__CONTINUOUS;        // Start timer in continuous mode.
        __bis_SR_register_on_exit(LPM3_bits);           // ENALBE LMP3 MODE
        break;
    case P1IV_P1IFG2:                   // P1.2
        break;
    case P1IV_P1IFG3:                   // P1.3
        break;
    case P1IV_P1IFG4:                   // P1.4
        break;
    case P1IV_P1IFG5:                   // P1.5
        break;
    case P1IV_P1IFG6:                   // P1.6
        break;
    case P1IV_P1IFG7:                   // P1.7
        break;
    }
}

/* ISR PORT 4 */
#pragma vector=PORT4_VECTOR
__interrupt void Port_4(void)
{
    switch(__even_in_range(P4IV, P4IV_P4IFG7))
    {
    case P4IV_P4IFG0:                   // P4.0
        break;
    case P4IV_P4IFG1:                   // P4.1
        break;
    case P4IV_P4IFG2:                   // P4.2
        break;
    case P4IV_P4IFG3:                   // P4.3
        break;
    case P4IV_P4IFG4:                   // P4.4
        break;
    case P4IV_P4IFG5:                   // P4.5
        /* Start Debouncing Button S1 */
        P4IE &= ~BIT5;                  // Disable port interrupt during delay.
        j++;
        if (j>2){
            j = 0;
            m = 0;
        }
        if ((m == j) && (m == 1)){
            TA0CCTL0 = CM_3 |           // CAPTURE VALUE ON BOTH EDGE
                       CCIS_2 |         // SELECT INPUT GROUND
                       CAP;             // CAPTURE MODE ENABLE
            TA0CCTL0 |= CCIS_3;         // SELECT INPUT VCC
            t = TA0CCR0;
            if(average == 0){           // average calculation
                average = 180000/TA0CCR0;               // average calculation = 50 *60 *60 * 10^-6/t*10^-4 = 180000 /TA0CCR0
            }                                            // 10^-2 removed and is added while displaying it on terminal.
            else{
                average = (average + 180000/TA0CCR0) >> 1;
                       }
            TA0CTL = TACLR;
        }

        if ((m == j) && (dir == 0) && (cnt < max) && (m == 2)){
            cnt++;
            m = 0;
            j = 0;
            if(cnt == max){             //BLINK LED IF THERE IS NO SPACE
                P4OUT |= BIT6;
                TA0CCR1 = TA0R + 1999;         // 200ms
                TA0CTL = TASSEL__ACLK |        // Select clock source SMCLK, 1 MHz.
                         MC__CONTINUOUS;        // Start timer in continuous mode.
                TA0CCTL1 &= ~CCIFG;
                TA0CCTL1 |= CCIE;               // Enable interrupt capability.
                __bis_SR_register_on_exit(LPM3_bits);
                P4OUT &= ~BIT6;
            }
        }
        else if ((m == j) && (dir == 1) && (cnt != 0) ){           // DIRECTION CHENGED
            m = 0;
            j = 0;
            cnt--;
            if(cnt == max-1){               // BLINK GREEN LED IF ONE VEHICLE LEAVES WHEN THE SPACE WAS FULL
                P1OUT |= BIT0;
                TA0CCR1 = TA0R + 1999;             // 200ms
                TA0CTL = TASSEL__ACLK |        // Select clock source SMCLK, 1 MHz.
                         MC__CONTINUOUS;        // Start timer in continuous mode.
                TA0CCTL1 &= ~CCIFG;
                TA0CCTL1 |= CCIE;               // Enable interrupt capability.
                __bis_SR_register_on_exit(LPM3_bits);           // ENABLE LMP3 SLEEP MODE
                P1OUT &= ~BIT0;
            }
        }

        TA2CCR0  = 249;                 // Compare value of 249 with CCR0.
        TA2CTL = TASSEL__ACLK |        // Select clock source SMCLK, 1 MHz.
                 MC__CONTINUOUS;        // Start timer in continuous mode.
        TA2CCTL0 &= ~CCIFG;
        TA2CCTL0 |= CCIE;               // Enable interrupt capability.
        TA0CTL = TASSEL__ACLK |        // Select clock source ACLK, 1 MHz.
                 MC__CONTINUOUS;        // Start timer in continuous mode.
        __bis_SR_register_on_exit(LPM3_bits);

        break;
    case P4IV_P4IFG6:                   // P4.6
        break;
    case P4IV_P4IFG7:                   // P4.7
        break;
    }
}

/* ISR TIMER A1 - CCR0 */
#pragma vector = TIMER1_A0_VECTOR
__interrupt void Timer1_A0_ISR (void)
{                                       /* Complete  Button S2 */
    TA1CTL = TACLR;
    TA1CCTL0 &= ~CCIE;              // Disable interrupt capability.
    P1IFG &= ~BIT1;                 // Drop meanwhile raised flag.
    P1IE |= BIT1;                   // Enable port interrupt again.
    __bic_SR_register_on_exit(LPM3_bits);
}

/* ISR TIMER A2 - CCR0 */
#pragma vector = TIMER2_A0_VECTOR
__interrupt void Timer2_A0_ISR (void)
{                                       /* Complete  Button S1 */
    TA2CTL = TACLR;
    TA2CCTL0 &= ~CCIE;              // Disable interrupt capability.
    P4IFG &= ~BIT5;                 // Drop meanwhile raised flag.
    P4IE |= BIT5;                   // Enable port interrupt again.
    __bic_SR_register_on_exit(LPM3_bits);
}

/* ISR TIMER A0 - CCR0 */
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR (void)
{                                       // TA0 CCR0

}

/* ISR TIMER A0 - CCR1, CCR2 AND TAIFG */
#pragma vector = TIMER0_A1_VECTOR
__interrupt void Timer0_A1_ISR (void)
{
    switch(__even_in_range(TA0IV, TA0IV_TA0IFG))
    {

    case TA0IV_TA0CCR1:                 // TA0 CCR1
        TA0CTL = TACLR;
        TA0CCTL1 &= ~CCIE;              // Disable interrupt capability.

        break;
    case TA0IV_TA0CCR2:                 // TA0 CCR2
        break;
    case TA0IV_TA0IFG:                  // TA0 TAIFG
        break;
    }
}

/* ### TIMEOUT TIMER ### */

void start_timeout(void)                // Initialize timeout watchdog timer.
{
    /*  START TIMEOUT */

    // Timeout interrupt after interval of 0.8 s using ACLK.
    WDTCTL = WDTPW |                    // The watchdog timer password (0x5A).
             WDTSSEL__ACLK |            // Select ACLK = 10 kHz as clock source.
             WDTTMSEL |                 // Interval timer mode.
             WDTIS__8192;                // Set interrupt prescaler: 2^15
                                            // 1 / (10,000 Hz / 8192) = 0.8192 s
                                            // Interrupt after 0.8 s.
    SFRIE1 |= WDTIE;                        // Enable watchdog timer interrupt.
}

void reset_timeout(void)                // Reset timeout watchdog timer.
{
    /*  RESET TIMEOUT */
    WDTCTL = (WDTCTL & 0x00FF) |        // Carry lower byte configuration.
             WDTPW |                    // The watchdog timer password (0x5A).
             WDTCNTCL;                  // Clear watchdog timer.
}

void stop_timeout(void)                 // Stop timeout watchdog timer.
{
    /*  STOP TIMEOUT */
    WDTCTL = (WDTCTL & 0x00FF) |        // Carry lower byte configuration.
             WDTPW |                    // The watchdog timer password (0x5A).
             WDTCNTCL |                 // Clear watchdog timer.
             WDTHOLD;                   // Stop watchdog timer.
    SFRIE1 &= ~WDTIE;                   // Disable watchdog timer interrupt.
}

/* ISR WATCHDOG */
#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR(void)
{
    watchdogstate = 1;
    __bic_SR_register_on_exit(LPM4_bits);
}

/* RTC */
#pragma vector=RTC_VECTOR
__interrupt void RTC_ISR(void)
{
    switch(__even_in_range(RTCIV, RTCIV_RT1PSIFG))
    {
        case RTCIV_NONE:      break;        // No interrupts
        case RTCIV_RTCOFIFG:  break;        // RTCOFIFG
        case RTCIV_RTCRDYIFG: break;        // RTCRDYIFG
        case RTCIV_RTCTEVIFG: break;        // RTCEVIFG
        case RTCIV_RTCAIFG:                 // RTCAIFG
            daynight = 0;
            __bic_SR_register_on_exit(LPM4_bits);       // disable LMP4 MODE
            break;
        case RTCIV_RT0PSIFG:  break;        // RT0PSIFG
        case RTCIV_RT1PSIFG:  break;        // RT1PSIFG
        default: break;
    }
}

