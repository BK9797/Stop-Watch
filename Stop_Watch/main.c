/*
 ============================================================================
 Name        : main.c
 Author      : BISHOY KAMEL
 Date        : 10-9-2024
 Copyright   : Your copyright notice
 Description : Stop Watch Application with Increment and Countdown Modes
 ============================================================================
 */

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// Function prototypes
void INT0_Init(void);        // Initialize INT0 (Reset)
void INT1_Init(void);        // Initialize INT1 (Pause)
void INT2_Init(void);        // Initialize INT2 (Resume)
void TIMER1_COMP_INIT(void); // Initialize Timer1 in CTC mode
void count(void);            // Function to count time (increment or decrement)
void Display(void);          // Display the time on 7-segment displays
void Buzzer(void);           // Function to activate the buzzer
void toggle_mode(void);      // Toggle between Increment and Countdown modes
void configure_IO_pins(void);// Configure I/O pins for input and output
void adjust_time(void);      // Adjust time while the timer is paused

// Global variables
unsigned char seconds = 0;   // Store seconds
unsigned char minutes = 0;   // Store minutes
unsigned char hours = 0;     // Store hours
unsigned char mode = 0;      // 0 for Increment, 1 for Countdown
unsigned char Timer_Flag = 0;// Flag to indicate Timer1 compare match
unsigned char is_paused = 0; // Flag to check if the timer is paused

// ISR for INT0 (Reset button, falling edge trigger)
ISR(INT0_vect) {
    TCNT1 = 0;               // Reset Timer1 counter
    seconds = 0;             // Reset seconds
    minutes = 0;             // Reset minutes
    hours = 0;               // Reset hours
}

// ISR for INT1 (Pause button, rising edge trigger)
ISR(INT1_vect) {
    TCCR1B &= ~(1 << CS12) & ~(1 << CS11) & ~(1 << CS10); // Stop the timer
    is_paused = 1;            // Set flag to indicate paused state
}

// ISR for INT2 (Resume button, falling edge trigger)
ISR(INT2_vect) {
    TCCR1B = (1 << WGM12) | (1 << CS10) | (1 << CS12); // Start the timer with prescaler
    is_paused = 0;            // Clear flag to indicate resume
}

// ISR for Timer1 Compare Match (1-second interval)
ISR(TIMER1_COMPA_vect) {
    Timer_Flag = 1;           // Set flag to indicate that 1 second has passed
}

int main(void) {
    // Initialize I/O pins and peripherals
    configure_IO_pins();
    INT0_Init();              // Initialize reset button (INT0)
    INT1_Init();              // Initialize pause button (INT1)
    INT2_Init();              // Initialize resume button (INT2)
    TIMER1_COMP_INIT();       // Initialize Timer1

    while (1) {
        Display();            // Display the time on the 7-segment displays

        if (is_paused) {
            adjust_time();    // Allow time adjustment while paused
        } else if (Timer_Flag) {
            count();          // Update the time every second
            Timer_Flag = 0;   // Clear the Timer_Flag after updating time
        }

        // Check if the mode toggle button is pressed (PB7)
        if (!(PINB & (1 << PB7))) {
            _delay_ms(30);    // Debounce delay
            if (!(PINB & (1 << PB7))) { // Confirm button press after debounce
                toggle_mode(); // Toggle between increment and countdown modes
                while(!(PINB & (1<<PB7))); // Wait for button release
            }
        }
    }
}

// Function to initialize INT0 (Reset button on PD2)
void INT0_Init(void) {
    MCUCR |= (1 << ISC01);    // Trigger on falling edge
    GICR |= (1 << INT0);      // Enable INT0
    sei();                    // Enable global interrupts
}

// Function to initialize INT1 (Pause button on PD3)
void INT1_Init(void) {
    MCUCR |= (1 << ISC11) | (1 << ISC10); // Trigger on rising edge
    GICR |= (1 << INT1);      // Enable INT1
    sei();                    // Enable global interrupts
}

// Function to initialize INT2 (Resume button on PB2)
void INT2_Init(void) {
    MCUCSR &= ~(1 << ISC2);   // Trigger on falling edge
    GICR |= (1 << INT2);      // Enable INT2
    sei();                    // Enable global interrupts
}

// Function to initialize Timer1 in CTC mode with 1-second interval
void TIMER1_COMP_INIT(void) {
    TCNT1 = 0;                // Initialize Timer1 counter
    TCCR1A = (1 << FOC1A);     // Force Output Compare for non-PWM mode
    TCCR1B = (1 << WGM12) | (1 << CS10) | (1 << CS12); // CTC mode, prescaler 1024
    OCR1A = 15625;            // Set compare match register for 1-second interval
    TIMSK |= (1 << OCIE1A);   // Enable Timer1 compare interrupt
    sei();                    // Enable global interrupts
}

// Function to configure I/O pins for the application
void configure_IO_pins(void) {
    // Configure Output Pins
    DDRD |= (1 << PD0) | (1 << PD4) | (1 << PD5);  // Buzzer (PD0), Increment LED (PD4), Countdown LED (PD5)
    PORTD &= ~(1 << PD0);      // Ensure buzzer is off initially
    PORTD |= (1 << PD4);       // Set Increment LED on initially (Increment mode)
    PORTD &= ~(1 << PD5);      // Countdown LED off initially

    // Configure PORTA for 7-segment display enable control (PA0-PA5)
    DDRA = 0x3F;               // Set PA0-PA5 as output

    // Configure PORTC for 7-segment data (lower nibble PC0-PC3)
    DDRC |= 0x0F;              // Set PC0-PC3 as output

    // Configure Input Pins with pull-up resistors
    DDRD &= ~((1 << PD2) | (1 << PD3));  // PD2: Reset button, PD3: Pause button
    PORTD |= (1 << PD2);       // Enable pull-up resistor for PD2 (INT0)

    DDRB &= ~((1 << PB2) | (1 << PB7));  // PB2: Resume button, PB7: Mode toggle button
    PORTB |= (1 << PB2) | (1 << PB7);    // Enable pull-up resistors for PB2 and PB7

    // Configure adjustment buttons (PB0-PB6) with pull-up resistors
    DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB3) | (1 << PB4) | (1 << PB5) | (1 << PB6));
    PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB3) | (1 << PB4) | (1 << PB5) | (1 << PB6);
}

// Function to count time (either increment or decrement)
void count(void) {
    if (mode == 0) {
        // Increment mode: Increment the time
        seconds++;
        if (seconds == 60) {
            seconds = 0;
            minutes++;
            if (minutes == 60) {
                minutes = 0;
                hours++;
                if (hours == 24) {
                    hours = 0;
                }
            }
        }
    } else if (mode == 1) {
        // Countdown mode: Decrement the time
        if (seconds == 0) {
            if (minutes == 0) {
                if (hours == 0) {
                    TCCR1B &= ~(1 << CS12) & ~(1 << CS11) & ~(1 << CS10); // Stop timer
                    Buzzer();         // Activate buzzer when countdown reaches zero
                } else {
                    hours--;
                    minutes = 59;
                    seconds = 59;
                }
            } else {
                minutes--;
                seconds = 59;
            }
        } else {
            seconds--;
        }
    }
}

// Function to display the time on 7-segment displays
void Display(void) {
    // Display seconds, minutes, and hours on 7-segment displays using multiplexing
    PORTA = 0x20;  // Enable display for seconds' unit place
    PORTC = (PORTC & 0xF0) | ((seconds % 10) & 0x0F);
    _delay_ms(2);

    PORTA = 0x10;  // Enable display for seconds' tens place
    PORTC = (PORTC & 0xF0) | ((seconds / 10) & 0x0F);
    _delay_ms(2);

    PORTA = 0x08;  // Enable display for minutes' unit place
    PORTC = (PORTC & 0xF0) | ((minutes % 10) & 0x0F);
    _delay_ms(2);

    PORTA = 0x04;  // Enable display for minutes' tens place
    PORTC = (PORTC & 0xF0) | ((minutes / 10) & 0x0F);
    _delay_ms(2);

    PORTA = 0x02;  // Enable display for hours' unit place
    PORTC = (PORTC & 0xF0) | ((hours % 10) & 0x0F);
    _delay_ms(2);

    PORTA = 0x01;  // Enable display for hours' tens place
    PORTC = (PORTC & 0xF0) | ((hours / 10) & 0x0F);
    _delay_ms(2);
}

// Function to adjust time while paused with debounce on all buttons
void adjust_time(void) {
    // Adjust hours
    if (!(PINB & (1 << PB0))) {  // Decrement hours button (PB0)
        _delay_ms(30);  // Debounce delay
        if (!(PINB & (1 << PB0))) {  // Confirm button press after debounce
            if (hours > 0) hours--;
        }
        while(!(PINB & (1<<PB0)));
    }

    if (!(PINB & (1 << PB1))) {  // Increment hours button (PB1)
        _delay_ms(30);  // Debounce delay
        if (!(PINB & (1 << PB1))) {  // Confirm button press after debounce
            if (hours < 23) hours++;
        }
        while(!(PINB & (1<<PB1)));
    }

    // Adjust minutes
    if (!(PINB & (1 << PB3))) {  // Decrement minutes button (PB3)
        _delay_ms(30);  // Debounce delay
        if (!(PINB & (1 << PB3))) {  // Confirm button press after debounce
            if (minutes > 0) minutes--;
        }
        while(!(PINB & (1<<PB3)));
    }

    if (!(PINB & (1 << PB4))) {  // Increment minutes button (PB4)
        _delay_ms(30);  // Debounce delay
        if (!(PINB & (1 << PB4))) {  // Confirm button press after debounce
            if (minutes < 59) minutes++;
        }
        while(!(PINB & (1<<PB4)));
    }

    // Adjust seconds
    if (!(PINB & (1 << PB5))) {  // Decrement seconds button (PB5)
        _delay_ms(30);  // Debounce delay
        if (!(PINB & (1 << PB5))) {  // Confirm button press after debounce
            if (seconds > 0) seconds--;
        }
        while(!(PINB & (1<<PB5)));
    }

    if (!(PINB & (1 << PB6))) {  // Increment seconds button (PB6)
        _delay_ms(30);  // Debounce delay
        if (!(PINB & (1 << PB6))) {  // Confirm button press after debounce
            if (seconds < 59) seconds++;
        }
        while(!(PINB & (1<<PB6)));
    }

    // Mode change button (PB7) with debounce
    if (!(PINB & (1 << PB7))) {  // Mode toggle button (PB7)
        _delay_ms(30);  // Debounce delay
        if (!(PINB & (1 << PB7))) {  // Confirm button press after debounce
            toggle_mode();  // Toggle between increment and countdown modes
        }
        while(!(PINB & (1<<PB7)));
    }
}

// Function to toggle between Increment and Countdown modes with LED handling
void toggle_mode(void) {
    mode = !mode;  // Toggle the mode variable

    if (mode == 0) {
        // Increment mode: Set Increment LED, clear Countdown LED
        PORTD |= (1 << PD4);   // Turn on Increment LED (PD4)
        PORTD &= ~(1 << PD5);  // Turn off Countdown LED (PD5)
    } else {
        // Countdown mode: Set Countdown LED, clear Increment LED
        PORTD |= (1 << PD5);   // Turn on Countdown LED (PD5)
        PORTD &= ~(1 << PD4);  // Turn off Increment LED (PD4)
    }
}

// Function to activate the buzzer for 2 seconds
void Buzzer(void) {
    PORTD |= (1 << PD0);   // Turn on the buzzer
    _delay_ms(2000);       // Keep buzzer on for 2 seconds
    PORTD &= ~(1 << PD0);  // Turn off the buzzer
}
