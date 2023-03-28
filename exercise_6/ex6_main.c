/******************************************************************************
 *                                                                            *
 * Exercise 6                                                                 *
 *                                                                            *
 * Task 1: Hello From the Other Side       X /  3 Points                      *
 * Task 2: Tell Me More                    X /  5 Points                      *
 * Comprehension Questions                 X /  2 Points                      *
 *                                        ----------------                    *
 *                                        XX / 10 Points                      *
 *                                                                            *
 ******************************************************************************
 *                                                                            *
 * name:                    Rajan                                             *
 * matriculation number:    1522581                                           *
 * e-mail:                  rajan.dhital@student.uni-siegen.de                *
 *                                                                            *
 ******************************************************************************
 *                                                                            *
 * Hardware Setup                                                             *
 *                                                                            *
 *                               MSP430FR5969                                 *
 *                            -----------------                               *
 *                           |                 |                              *
 *               UART Tx> <--|P2.0         P4.6|--> (LED1)                    *
 *               <UART Rx -->|P2.1         P1.0|--> (LED2)                    *
 *                           |                 |                              *
 *                           | 16 MHz          |                              *
 *                            -----------------                               *
 *                                                                            *
 ******************************************************************************/

#include <msp430fr5969.h>
#include <stdint.h>



volatile struct                         // State variables of the FSMs:
{
    uint8_t Parser : 3;                 // FSM of parser:
                                        // 0:   Idle
                                        // 1:   'L'
                                        // 2:   'E'
                                        // 3:   'D'
                                        // 4:   '1'
                                        // 5:   '2'
                                        // 6:   LED control
    uint8_t ParSub : 4;                 // FSM of on/off sub-parser:
                                        // 0:   Idle
                                        // 1:   'O'
                                        // 2:   'N'
                                        // 3:   'F' #1
                                        // 4:   'F' #2
                                        // 5:   'D'
                                        // 6:   digit1
                                        // 7:   digit2
                                        // 8:   digit3
    uint8_t Tout : 1;                   // Timeout event:
                                        // 0:   No timeout event
                                        // 1:   Timeout event released
} States;

typedef enum                            // State values of main FSM:
{
    PARSE_IDLE,                         // 0:   Idle
    PARSE_L,                            // 1:   'L'
    PARSE_E,                            // 2:   'E'
    PARSE_D,                            // 3:   'D'
    PARSE_1,                            // 4:   '1'
    PARSE_2,                            // 5:   '2'
    PARSE_CTRL                          // 6:   LED control
} STATES_MAIN;

typedef enum                            // State values of control sub-FSM:
{
    PARSE_CTRL_IDLE,                    // 0:   Idle
    PARSE_CTRL_O,                       // 1:   'O'
    PARSE_CTRL_N,                       // 2:   'N'
    PARSE_CTRL_F1,                      // 3:   'F' #1
    PARSE_CTRL_F2,                      // 4:   'F' #2
    PARSE_CTRL_D,                       // 5:   'D'
    PARSE_CTRL_D1,                      // 6:   digit1
    PARSE_CTRL_D2,                      // 7:   digit2
    PARSE_CTRL_D3                       // 8:   digit3
} STATES_CTRL;

typedef enum                            // Return values of control sub-FSM:
{
    RETURN_ERR,                         // 0:   Error
    RETURN_CONT,                        // 1:   Continue
    RETURN_CMPl                         // 2:   Completed
} CTRL_RETURN;


/* SERIAL INTERFACE */

#define RXBUF 32                        // Size of circular receiver buffer.

volatile uint8_t rxBuf[RXBUF];          // The circular buffer.
volatile uint8_t rxBufS = 0;            // Start: head position.
volatile uint8_t rxBufE = 0;            // End: tail position.
volatile uint8_t k = 0;
uint8_t rxBufErr = 0;                   // Overflow error counter.

static uint8_t rxChar = 0;
void uartTx(uint8_t *data);


/* PARSER ROUTINES */

// Major FSM of the parser.
void parse_main(uint8_t *data);
// Sub-FSM of the parser which considers selected LED output.
int8_t parse_ctrl(uint8_t *data, uint8_t led);


/* TIMEOUT TIMER */

void start_timeout(void);               // Initialize timeout watchdog timer.
void clear_timeout(void);               // Clear timeout watchdog timer.
void stop_timeout(void);                // Stop timeout watchdog timer.


/* PWM LOOKUP-TABLE */

// Look-up table for a linearized dimming of the LEDs. Because the human
// perception scales logarithmic, an exponential increase is applied to
// map 256 steps of perceived brightness to 16 bit of linear PWM.
const uint16_t PWM_16[256] =
{
        0,     1,     1,     1,     1,     1,     1,     1,
        1,     2,     2,     2,     2,     2,     2,     2,
        2,     2,     2,     2,     2,     3,     3,     3,
        3,     3,     3,     3,     4,     4,     4,     4,
        4,     4,     5,     5,     5,     5,     5,     6,
        6,     6,     6,     7,     7,     7,     8,     8,
        8,     9,     9,    10,    10,    10,    11,    11,
       12,    12,    13,    13,    14,    15,    15,    16,
       17,    17,    18,    19,    20,    21,    22,    23,
       24,    25,    26,    27,    28,    29,    31,    32,
       33,    35,    36,    38,    40,    41,    43,    45,
       47,    49,    52,    54,    56,    59,    61,    64,
       67,    70,    73,    76,    79,    83,    87,    91,
       95,    99,   103,   108,   112,   117,   123,   128,
      134,   140,   146,   152,   159,   166,   173,   181,
      189,   197,   206,   215,   225,   235,   245,   256,
      267,   279,   292,   304,   318,   332,   347,   362,
      378,   395,   412,   431,   450,   470,   490,   512,
      535,   558,   583,   609,   636,   664,   693,   724,
      756,   790,   825,   861,   899,   939,   981,  1024,
     1069,  1117,  1166,  1218,  1272,  1328,  1387,  1448,
     1512,  1579,  1649,  1722,  1798,  1878,  1961,  2048,
     2139,  2233,  2332,  2435,  2543,  2656,  2773,  2896,
     3025,  3158,  3298,  3444,  3597,  3756,  3922,  4096,
     4277,  4467,  4664,  4871,  5087,  5312,  5547,  5793,
     6049,  6317,  6596,  6889,  7194,  7512,  7845,  8192,
     8555,  8933,  9329,  9742, 10173, 10624, 11094, 11585,
    12098, 12634, 13193, 13777, 14387, 15024, 15689, 16384,
    17109, 17867, 18658, 19484, 20346, 21247, 22188, 23170,
    24196, 25267, 26386, 27554, 28774, 30048, 31378, 32768,
    34218, 35733, 37315, 38967, 40693, 42494, 44376, 46340,
    48392, 50534, 52772, 55108, 57548, 60096, 62757, 65534
};


/* MAIN PROGRAM */
void main(void)
{
    // Stop watchdog timer.
    WDTCTL = WDTPW | WDTHOLD;

    // Configure one FRAM waitstate as required by the device datasheet for MCLK
    // operation beyond 8MHz before configuring the clock system.
    FRCTL0 = FRCTLPW | NWAITS_1;        // Set password and enable wait state.
    FRCTL0 ^= FXPW;                     // Lock register with invalid password.
    // Initialize the clock system to generate 16 MHz DCO clock.
    CSCTL0_H    = CSKEY_H;              // Unlock CS registers.
    CSCTL1      = DCOFSEL_4 |           // Set DCO to 16 MHz, DCORSEL for
                  DCORSEL;              //   high speed mode enabled.
    CSCTL2      = SELA__VLOCLK |        // Set ACLK = VLOCLK = 10 kHz.
                  SELS__DCOCLK |        //   Set SMCLK = DCOCLK.
                  SELM__DCOCLK;         //   Set MCLK = DCOCLK.
                                        // SMCLK = MCLK = DCOCLK = 16 MHz.
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
    // Initialize port 4:
    P4DIR |= BIT6;                      // P4.6 - output for LED1, off.
    P4OUT &= ~BIT6;

    // Initialize port 2:
    // Select Tx and Rx functionality of eUSCI0 for hardware UART.
    // P2.0 - UART Tx (UCA0TXD).
    // P2.1 - UART Rx (UCA0RXD).
    P2SEL0 &= ~(BIT1 | BIT0);
    P2SEL1 |= BIT1 | BIT0;

    // Disable the GPIO power-on default high-impedance mode to activate the
    // previously configured port settings.
    PM5CTL0 &= ~LOCKLPM5;


    /* Initialize serial UART interface */

    /* TODO UART INIT */
    /* Initialization of the serial UART interface */
    UCA0CTLW0 |= UCSSEL__SMCLK |        // Select clock source SMCLK = 16 MHz.
                    UCSWRST;               // Enable software reset.
    // Set Baud rate of 192000 Bd.
    // Recommended settings available in table 30-5, p. 765 of the User's Guide.
    UCA0BRW = 52;                        // Clock prescaler of the
                                           //   Baud rate generator.
    UCA0MCTLW = UCBRF_1 |               // First modulations stage.
                  0x4900 |                // Second modulation
                  UCOS16;                 // Enable oversampling mode.
    UCA0CTLW0 &= ~UCSWRST;              // Disable software reset and start
                                           //   eUSCI state machine.
    UCA0IE |= UCRXIE;


    /* Initialize hardware/software PWM */

    // Initialize Timer A0 for PWM at > 60 Hz:
    // Timer counts always up to TOP = 0xFFFF = 65535 and rolls over.
    // 16 MHz / 65535 = 244.144 Hz -> timer rolls over at about 244 Hz.
    // Required are 60 Hz: 244.144 Hz / 60 Hz = 4.069 -> prescaler: 4
    // Resulting period frequency: 16,000,000 / 4 / 65535 = 61.036 Hz
    TA0EX0 = TAIDEX_1;                  // Secondary input divider stage of 2.
    TA0CTL = TASSEL__SMCLK |            // Select clock source SMCLK, 16 MHz.
             ID__2 |                    // First input divider stage of 2.
             MC__CONTINUOUS;            // Enable continuous mode.

    // Hardware PWM on P1.0:
    P1SEL0 |= BIT0;                     // Select the OUT1 function of P1.0.
    P1SEL1 &= ~BIT0;
    TA0CCR1 = 0;                        // Duty cycle of 0%, off.
    TA0CCTL1 = OUTMOD_7;                // Output reset/set mode.
    // Software PWM on P4.6:
    TA0CCR2 = 0;                        // Duty cycle of 0%, off.
    TA0CCTL0 |= CCIE;                   // Enable interrupt of CCR0.
    TA0CCTL2 |= CCIE;                   // Enable interrupt of CCR2.


    /* Initialize status bits */

    States.Parser = PARSE_IDLE;
    States.ParSub = PARSE_CTRL_IDLE;
    States.Tout = 0;


    // Enable interrupts globally.
    __bis_SR_register(GIE);


    /* MAIN LOOP */
    while(1)
    {
        /* Overflow handling */


        while(rxBufErr > 0)             // Buffer overflow occurred?
        {
            uartTx("Error: Buffer overflow!\r\n");
            rxBufErr--;                 // Decrement overflow error counter.
        }

        /* Input handling */

        /* TODO BUFFER POP */

        if(rxBufE != rxBufS )                            // Received characters available?
        {
            // Get received character from circular buffer.


            parse_main(&rxChar);               // Parse the character.
            rxBufS++;                           //increase head
            if (rxBufS > 31)
           {
               rxBufS = 0;                      // reset head
           }
        }

        /* Timeout handling */

        if(States.Tout)
        {
            States.Tout = 0;            // Reset timeout status bit.
            stop_timeout();             // Stop timeout watchdog timer.

            // Reset parser FSM states.
            States.Parser = PARSE_IDLE;
            States.ParSub = PARSE_CTRL_IDLE;

            // Send timeout notification.
            uartTx("Error: Timeout!\r\n");

            // Blink the LEDs.

            // Protect current PWM values.
            uint16_t tmp_led1 = TA0CCR2;
            uint16_t tmp_led2 = TA0CCR1;
            TA0CCR2 = PWM_16[0x00];     // Turn off LED1.
            TA0CCR1 = PWM_16[0x00];     // Turn off LED2.

            uint8_t i;
            for(i=0; i<3; i++)
            {
                TA0CCR2 = PWM_16[0xFF]; // Turn on LED1.
                TA0CCR1 = PWM_16[0x00]; // Turn off LED2.
                __delay_cycles(400000);
                TA0CCR2 = PWM_16[0x00]; // Turn off LED1.
                __delay_cycles(400000);
                TA0CCR2 = PWM_16[0x00]; // Turn off LED1.
                TA0CCR1 = PWM_16[0xFF]; // Turn on LED2.
                __delay_cycles(400000);
                TA0CCR1 = PWM_16[0x00]; // Turn off LED2.
                __delay_cycles(400000);
            }

            // Restore PWM values.
            TA0CCR2 = tmp_led1;
            TA0CCR1 = tmp_led2;
        }
    }
}


void parse_main(uint8_t *data)
{
    uint8_t ret = 0;                    // Return state of sub-FSM.

    /* TODO PARSER MAJOR FSM */

    switch(States.Parser)
    {
    case PARSE_IDLE:                    // IDLE
        if(*data == 'L')
        {
            start_timeout();            // Start timeout watchdog timer.
            States.Parser = PARSE_L;    // Go to L state.
        }
        else
        {
            stop_timeout();             // Stop timeout watchdog timer.
            States.Parser = PARSE_IDLE; // Go back to idle state.
        }
        break;
    case PARSE_L:                       // L
        //TODO FSM_L
        if(*data == 'E')
        {
            start_timeout();
            States.Parser = PARSE_E;        // go to E state.
        }
        else
        {
            stop_timeout();
            States.Parser = PARSE_IDLE;         //go back to idle state.
        }

        break;
    case PARSE_E:                       // E
        //TODO FSM_E
        if(*data == 'D')
        {
            start_timeout();
            States.Parser = PARSE_D;            // go to D state.
        }
        else
        {
            stop_timeout();
            States.Parser = PARSE_IDLE;         // Go back to idle state
        }

        break;
    case PARSE_D:                       // D
        //TODO FSM_D
        if(*data == '1')
        {
            start_timeout();
            States.Parser = PARSE_1;            // Go to 1 state
        }
        else if(*data == '2')
        {
            start_timeout();
            States.Parser = PARSE_2;            // Go to 2 state
        }
        else
        {
            stop_timeout();
            States.Parser = PARSE_IDLE;         // go to idle state
        }

        break;
    case PARSE_1:                       // LED 1
        ret = parse_ctrl(data, 1);      // Execute sub-FSM to control LED 1.
        // Catch completion or error and restart FSM.
        if(ret == RETURN_CMPl || ret == RETURN_ERR)
            States.Parser = PARSE_IDLE; // Go back to idle state.
        break;
    case PARSE_2:                       // LED 2
        ret = parse_ctrl(data, 2);      // Execute sub-FSM to control LED 2.
        // Catch completion or error and restart FSM.
        if(ret == RETURN_CMPl || ret == RETURN_ERR)
            States.Parser = PARSE_IDLE; // Go back to idle state.
        break;
    }
}


int8_t parse_ctrl(uint8_t *data, uint8_t led)
{
    static uint8_t pwm_digit1 = 0;
    static uint8_t pwm_digit2 = 0;
    static uint8_t pwm_digit3 = 0;


    /* TODO PARSER SUB-FSM */

    switch(States.ParSub)
    {
    case PARSE_CTRL_IDLE:                   // IDLE
        if(*data == 'O')
        {
            clear_timeout();                // Clear timeout watchdog timer.
            States.ParSub = PARSE_CTRL_O;   // Go to on/off branch.
            break;
        }
        else if(*data == 'D')
        {
            pwm_digit1 = 0;                 // Reset PWM digit values.
            pwm_digit2 = 0;
            pwm_digit3 = 0;

            clear_timeout();                // Clear timeout watchdog timer.
            States.ParSub = PARSE_CTRL_D;   // Go to dimming branch.
            break;
        }
        stop_timeout();                     // Stop timeout watchdog timer.
        States.ParSub = PARSE_CTRL_IDLE;    // Reset control state.
        return RETURN_ERR;                  // Error
    case PARSE_CTRL_O:                      // 'O'
        //TODO SFSM_O
        if(*data == 'N')
        {
            clear_timeout();                // Clear timeout watchdog timer.
            States.ParSub = PARSE_CTRL_N;   // Go to on/off branch.
            break;
        }
        else if(*data == 'F')
        {
            clear_timeout();                // Clear timeout watchdog timer.
            States.ParSub = PARSE_CTRL_F1;   // Go to on/off branch.
            break;
        }
        stop_timeout();                     // Stop timeout watchdog timer.
        States.ParSub = PARSE_CTRL_IDLE;    // Reset control state.
        return RETURN_ERR;                  // Error
    case PARSE_CTRL_N:                      // 'N'
        //TODO SFSM_N

        if((*data == 0x0A)|| (*data == 0x0D))       // check carriage return \r or a line feed \n character.
        {
            clear_timeout();                // Clear timeout watchdog timer.
            if(led == 1)
            {
                TA0CCR2 = 65534;            // maximum Pwm

            }
            if(led == 2)
            {
                TA0CCR1 = 65534;            // maximum PWM of 2nd Led
            }


        }

        stop_timeout();                     // Stop timeout watchdog timer.
        States.ParSub = PARSE_CTRL_IDLE;    // Reset control state.
        return RETURN_ERR;                  // Error
    case PARSE_CTRL_F1:                     // 'F'#1
        //TODO SFSM_F1
        if(*data == 'F')
        {
            clear_timeout();                // Clear timeout watchdog timer.
            States.ParSub = PARSE_CTRL_F2;   // Go to on/off branch.
            break;
        }
        stop_timeout();                     // Stop timeout watchdog timer.
        States.ParSub = PARSE_CTRL_IDLE;    // Reset control state.
        return RETURN_ERR;                  // Error
    case PARSE_CTRL_F2:                     // 'F'#2
        //TODO SFSM_F2
        if((*data == 0x0A)|| (*data == 0x0D))           // check carriage return \r or a line feed \n character.
       {
            clear_timeout();                // Clear timeout watchdog timer.
           if(led == 1)
           {
               TA0CCR2 = 0;                             // Minimum PWM of led 1.

           }
           if(led == 2)
           {
               TA0CCR1 = 0;                             // Minimum PWM of led 2.
           }

       }
       stop_timeout();                     // Stop timeout watchdog timer.
       States.ParSub = PARSE_CTRL_IDLE;    // Reset control state.

        return RETURN_ERR;                  // Error

    /* TODO SUB-FSM DIMM */

    case PARSE_CTRL_D:                      // 'D'
        //TODO SFSM_D
        if (isdigit(*data))                 // check number is or is not
        {
            clear_timeout();
            pwm_digit1 = *data - '0';       // change character to number
            States.ParSub = PARSE_CTRL_D1;  // go to state D1.
            break;
        }

        stop_timeout();                     // Stop timeout watchdog timer.
        States.ParSub = PARSE_CTRL_IDLE;    // Reset control state.

        return RETURN_ERR;                  // Error
    case PARSE_CTRL_D1:                     // digit1
        //TODO SFSM_D1
        if((*data == 0x0A)|| (*data == 0x0D))
        {
            if(led == 1)
            {
                TA0CCR2 = PWM_16[pwm_digit1];       // PWM for first digit

            }
            if(led == 2)
            {
                TA0CCR1 = PWM_16[pwm_digit1];       //pwm for first digit
            }

            stop_timeout();                // Clear timeout watchdog timer.
            States.ParSub = PARSE_CTRL_IDLE;;   // Go to on/off branch.
            return RETURN_CMPl;                 // completion of process
        }
        else
        {
            if (isdigit(*data))
            {
                clear_timeout();
                pwm_digit1 = *data - '0';
                States.ParSub = PARSE_CTRL_D2;      // go to state D2
                break;
            }

        }
        stop_timeout();                     // Stop timeout watchdog timer.
        States.ParSub = PARSE_CTRL_IDLE;    // Reset control state.

        return RETURN_ERR;                  // Error
    case PARSE_CTRL_D2:                     // digit2
        //TODO SFSM_D2
        if((*data == 0x0A)|| (*data == 0x0D))
        {

            if(led == 1)
            {
                TA0CCR2 = PWM_16[10*pwm_digit1+pwm_digit2];

            }
            if(led == 2)
            {
                TA0CCR1 = PWM_16[10*pwm_digit1+pwm_digit2];
            }

            stop_timeout();                // Clear timeout watchdog timer.
            States.ParSub = PARSE_CTRL_IDLE;   // Go to on/off branch.
            break;
        }
        else
        {
            if (isdigit(*data))
            {
                clear_timeout();
                pwm_digit1 = *data - '0';
                States.ParSub = PARSE_CTRL_D3;          // go to state D3
                break;

            }

        }
        stop_timeout();                     // Stop timeout watchdog timer.
        States.ParSub = PARSE_CTRL_IDLE;    // Reset control state.


        return RETURN_ERR;                  // Error
    case PARSE_CTRL_D3:                     // digit3
        //TODO SFSM_D3
        if((*data == 0x0A)|| (*data == 0x0D) )
        {

            if(led == 1)
            {
                TA0CCR2 = PWM_16[100*pwm_digit1+10*pwm_digit2+pwm_digit3];

            }
            if(led == 2)
            {
                TA0CCR1 = PWM_16[100*pwm_digit1+10*pwm_digit2+pwm_digit3];
            }

            clear_timeout();                // Clear timeout watchdog timer.
            States.ParSub = PARSE_CTRL_IDLE;   // Go to on/off branch.
            return RETURN_ERR;

        }
        return RETURN_ERR;                  // Error
    }

    return RETURN_CONT;
}


/* ### SERIAL INTERFACE ### */

void uartTx(uint8_t *data)
{
    // Iterate through array, look for null pointer at end of string.
    uint16_t i = 0;
    while(data[i])
    {
        while((UCA0STATW & UCBUSY));    // Wait while module is busy with data.
        UCA0TXBUF = data[i];            // Transmit element i of data array.
        i++;                            // Increment variable for array address.
    }
}


#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
    switch(__even_in_range(UCA0IV, USCI_UART_UCTXCPTIFG))
    {
    case USCI_NONE:                     // No interrupts
        break;
    case USCI_UART_UCRXIFG:             // Received data

        /* TODO BUFFER PUSH */
        if(rxBufErr)
        {
            break;
        }


        rxChar = UCA0RXBUF;             // store the received character


        rxBuf[rxBufE] = rxChar;         // push into buffer

        rxBufE++;                       // increment tail of buffer
        if(rxBufE > 31)
        {
            rxBufE = 0;
        }

        if(rxBufE == rxBufS)            // check for overflow
        {
            rxBufErr++;                 // raise overflow flag
        }





        break;
    case USCI_UART_UCTXIFG:             // Transmit data
        break;
    case USCI_UART_UCSTTIFG:            //
        break;
    case USCI_UART_UCTXCPTIFG:          //
        break;
    }
}


/* ISR TIMER A0 - CCR0 */
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR (void)
{                                       // TA0 CCR0

    P4OUT |= BIT6;                      // Turn LED1 on.

}

/* ISR TIMER A0 - CCR1, CCR2 AND TAIFG */
#pragma vector = TIMER0_A1_VECTOR
__interrupt void Timer0_A1_ISR (void)
{
    switch(__even_in_range(TA0IV, TA0IV_TA0IFG))
    {
    case TA0IV_TA0CCR1:                 // TA0 CCR1
        break;
    case TA0IV_TA0CCR2:                 // TA0 CCR2

        P4OUT &= ~BIT6;                 // Turn LED1 off.

        break;
    case TA0IV_TA0IFG:                  // TA0 TAIFG
        break;
    }
}


/* ### TIMEOUT TIMER ### */

void start_timeout(void)
{
    // Initialize watchdog timer for timeout interrupt:
    // Timeout interrupt after interval 3.3 s.
    WDTCTL = WDTPW |                    // The watchdog timer password (0x5A).
             WDTSSEL__ACLK |            // Select ACLK = 10 kHz as clock source.
             WDTTMSEL |                 // Interval timer mode.
             WDTIS__32K;                // Set interrupt prescaler: 2^15
                                        // 1 / (10,000 Hz / 32,768) = 3.2768 s
                                        // Interrupt after 3.3 s.
    SFRIE1 |= WDTIE;                    // Enable watchdog timer interrupt.
}

void clear_timeout(void)                // Clear the timeout counter.
{
    WDTCTL = (WDTCTL & 0x00FF) |        // Carry lower byte configuration.
             WDTPW |                    // The watchdog timer password (0x5A).
             WDTCNTCL;                  // Clear watchdog timer.
}

void stop_timeout(void)                 // Stop the timeout counter.
{
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
    // Raise status bit of timeout event.
    States.Tout = 1;
}
