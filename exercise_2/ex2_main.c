/******************************************************************************
 *                                                                            *
 * Exercise 2                                                                 *
 *                                                                            *
 * Task 1: Push Da Button                   X /  4 Points                     *
 * Task 2: Advanced Button Handling         X /  4 Points                     *
 * Comprehension Questions                  X /  2 Points                     *
 *                                        ----------------                    *
 *                                         XX / 10 Points                     *
 *                                                                            *
 ******************************************************************************
 *                                                                            *
 * name:                    Rajan                                         *
 * matriculation number:    1522581                                        *
 * e-mail:                  rajan.dhital@student.uni-siegen.de                   *
 *                                                                            *
 ******************************************************************************
 *                                                                            *
 * Hardware Setup                                                             *
 *                                                                            *
 *                               MSP430FR5969                                 *
 *                            -----------------                               *
 *                           |                 |                              *
 *                   (S1) -->|P4.5         P4.6|--> (LED1)                    *
 *                   (S2) -->|P1.1         P1.0|--> (LED2)                    *
 *                           |                 |                              *
 *                            -----------------                               *
 *                                                                            *
 ******************************************************************************/

// Select the task you are working on:
//#define TASK_1
#define TASK_2

#include <msp430fr5969.h>



#ifdef TASK_1
/*******************************************************************************
** Task 1
*******************************************************************************/

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

    P1DIR |= BIT0;                      // Initialize port 1 as output
    P1OUT &= ~BIT0;                     // P1.0 - output for LED2, off.
    P1DIR |= BIT1;                      // Initialize port 1 as input
    P1REN |= BIT1;                      // enable pull up/down resistor
    P1OUT |= BIT1;                      // configure resistor as pull up

    P4DIR |= BIT6;                      // Initialize port 4 as output
    P4OUT &= ~BIT6;                     // P4.6 - output for LED2, off.
    P4DIR |= BIT5;                      // Initialize port 4 as input
    P4REN |= BIT5;                      // enable pull up/down resistor
    P4OUT |= BIT5;                      // configure resistor as pull up




    // Disable the GPIO power-on default high-impedance mode to activate the
    // previously configured port settings.
    PM5CTL0 &= ~LOCKLPM5;

    /* MAIN LOOP */
    while(1)
    {
        if(!(P1IN & BIT1) & ~P1OUT)         // check the condition of button 1 press
        {
            P1OUT |= BIT0;
        }

        if (P1IN & BIT1 & P1OUT)            // button 1 unpressed
        {
            P1OUT &= ~BIT0;
        }

        if(!(P4IN & BIT5) & ~P4OUT)         // check the condition of button 4 press
        {
            P4OUT |= BIT6;
        }

        if (P4IN & BIT5 & P4OUT)             // button 4 unpressed
        {
            P4OUT &= ~BIT6;
        }

    }
}

/******************************************************************************/
#endif /* TASK_1 */



#ifdef TASK_2
/*******************************************************************************
** Task 2
*******************************************************************************/

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




    P1DIR |= BIT0;                      // Initialize port 1 as output
    P1OUT &= ~BIT0;                     // P1.0 - output for LED2, off.
    P1DIR |= BIT1;                      // Initialize port 1 as input
    P1REN |= BIT1;                      // enable pull up/down resistor
    P1OUT |= BIT1;                      // configure resistor as pull up
    P1IES |= BIT1;                      // sensitivity High to low
    P1IE |= BIT1;                       //local interrupt for 1.1 enable


    P4DIR |= BIT6;                      // Initialize port 4 as output
    P4OUT &= ~BIT6;                     // P4.0 - output for LED1, off.
    P4DIR |= BIT5;                      // Initialize port 4 as input
    P4REN |= BIT5;                      // enable pull up/down resistor
    P4OUT |= BIT5;                      // configure resistor as pull up
    P4IES |= BIT5;                      // sensitivity High to low
    P4IE |= BIT5;                       //local interrupt for 4.5 enable



    // Disable the GPIO power-on default high-impedance mode to activate the
    // previously configured port settings.
    PM5CTL0 &= ~LOCKLPM5;

    // Clear interrupt flags that have been raised due to high-impedance settings.
    P1IFG &= ~BIT1;
    P4IFG &= ~BIT5;

    // Enable interrupts globally.
    __bis_SR_register(GIE);

    /* MAIN LOOP */
    while(1)
    {

    }
}

/* ISR PORT 1 */
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    switch(P1IV)
    {
    case P1IV_P1IFG0:                   // P1.0
        break;
    case P1IV_P1IFG1:                   // P1.1
        //__bic_SR_register(GIE); can we use this instead of delay and clearing P1IFG?

        __delay_cycles(25000);          //Delay period 25 ms.
        P1IFG &= ~BIT1;                 // clear raised flag

        if(!(P1IN & BIT1))              // check whether button is pressed or not
        {
            P1OUT ^= BIT0;              //Toggle P1.0 on each interrupt.

        }


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
    switch(P4IV)
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

        __delay_cycles(25000);          //Delay period 25 ms.
        P4IFG &= ~BIT5;                 // clear raised flag

        if(!(P4IN & BIT5))              // check whether button is pressed or not
        {
            P4OUT ^= BIT6;              //Toggle P4.6 on each interrupt.

         }


        break;
    case P4IV_P4IFG6:                   // P4.6
        break;
    case P4IV_P4IFG7:                   // P4.7
        break;
    }
}

/******************************************************************************/
#endif /* TASK_2 */
