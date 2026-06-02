#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

// ========== LCD Pin Definitions ==========
#define LCD_PORT    PORTB
#define LCD_DDR     DDRB
#define LCD_RS      0   // PB0
#define LCD_EN      1   // PB1
#define LCD_D4      2   // PB2
#define LCD_D5      3   // PB3
#define LCD_D6      4   // PB4
#define LCD_D7      5   // PB5

// ========== LED Pin Definitions ==========
#define LED_GREEN   0   // PD0
#define LED_YELLOW  5   // PD5
#define LED_RED     6   // PD6

// ========== Threshold Definitions ==========
#define TEMP_WARNING    30.0
#define TEMP_DANGER     35.0
#define HUMIDITY_WARNING 60.0
#define HUMIDITY_DANGER  75.0
#define GAS_WARNING     400.0
#define GAS_DANGER      600.0

// ========== LCD Functions ==========
void LCD_Enable(void) {
    LCD_PORT |= (1 << LCD_EN);
    _delay_us(1);
    LCD_PORT &= ~(1 << LCD_EN);
    _delay_us(100);
}

void LCD_SendNibble(unsigned char nibble) {
    // Clear data bits (PB2-PB5)
    LCD_PORT &= ~((1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7));
    
    // Set data bits
    if (nibble & 0x01) LCD_PORT |= (1 << LCD_D4);
    if (nibble & 0x02) LCD_PORT |= (1 << LCD_D5);
    if (nibble & 0x04) LCD_PORT |= (1 << LCD_D6);
    if (nibble & 0x08) LCD_PORT |= (1 << LCD_D7);
    
    LCD_Enable();
}

void LCD_Command(unsigned char cmd) {
    LCD_PORT &= ~(1 << LCD_RS);  // RS = 0 (command)
    LCD_SendNibble(cmd >> 4);    // Upper nibble
    LCD_SendNibble(cmd & 0x0F);  // Lower nibble
    _delay_ms(2);
}

void LCD_Data(unsigned char data) {
    LCD_PORT |= (1 << LCD_RS);   // RS = 1 (data)
    LCD_SendNibble(data >> 4);   // Upper nibble
    LCD_SendNibble(data & 0x0F); // Lower nibble
    _delay_us(100);
}

void LCD_Init(void) {
    // Set all LCD pins as outputs
    LCD_DDR |= (1 << LCD_RS) | (1 << LCD_EN) | 
               (1 << LCD_D4) | (1 << LCD_D5) | 
               (1 << LCD_D6) | (1 << LCD_D7);
    
    LCD_PORT = 0x00;
    _delay_ms(50);  // Wait for LCD power-up
    
    // Initialize in 4-bit mode (standard sequence)
    LCD_PORT &= ~(1 << LCD_RS);  // RS = 0
    
    LCD_SendNibble(0x03);
    _delay_ms(5);
    LCD_SendNibble(0x03);
    _delay_us(150);
    LCD_SendNibble(0x03);
    _delay_us(150);
    LCD_SendNibble(0x02);  // Set to 4-bit mode
    _delay_us(150);
    
    LCD_Command(0x28);  // 4-bit, 2 lines, 5x8 font
    LCD_Command(0x0C);  // Display ON, cursor OFF
    LCD_Command(0x06);  // Increment cursor
    LCD_Command(0x01);  // Clear display
    _delay_ms(2);
}

void LCD_Clear(void) {
    LCD_Command(0x01);
    _delay_ms(2);
}

void LCD_SetCursor(unsigned char row, unsigned char col) {
    unsigned char pos;
    if (row == 0) {
        pos = 0x80 + col;
    } else {
        pos = 0xC0 + col;
    }
    LCD_Command(pos);
}

void LCD_Print(char* str) {
    while (*str) {
        LCD_Data(*str++);
    }
}

void LCD_PrintInt(int num) {
    char buffer[8];
    sprintf(buffer, "%d", num);
    LCD_Print(buffer);
}

// ========== UART Functions ==========
void UART_Init(unsigned int baud) {
    unsigned int ubrr = F_CPU/16/baud - 1;
    UBRRH = (unsigned char)(ubrr >> 8);
    UBRRL = (unsigned char)ubrr;
    UCSRB = (1 << TXEN);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

void UART_Transmit(unsigned char data) {
    while (!(UCSRA & (1 << UDRE)));
    UDR = data;
}

void UART_Print(char* str) {
    while (*str) {
        UART_Transmit(*str++);
    }
}

// ========== ADC Functions ==========
void ADC_Init(void) {
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
}

uint16_t ADC_Read(uint8_t channel) {
    ADMUX = (1 << REFS0) | (channel & 0x07);
    _delay_us(10);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

// ========== LED Functions ==========
void LED_Init(void) {
    // All LEDs on PORTD
    DDRD |= (1 << LED_GREEN) | (1 << LED_YELLOW) | (1 << LED_RED);
    PORTD &= ~((1 << LED_GREEN) | (1 << LED_YELLOW) | (1 << LED_RED));
}

void LED_Update(uint8_t level) {
    // Turn all LEDs off first
    PORTD &= ~((1 << LED_GREEN) | (1 << LED_YELLOW) | (1 << LED_RED));
    
    switch (level) {
        case 0: PORTD |= (1 << LED_GREEN);  break;  // Safe
        case 1: PORTD |= (1 << LED_YELLOW); break;  // Warning
        case 2: PORTD |= (1 << LED_RED);    break;  // Danger
    }
}

// ========== Helper Functions ==========
void float_to_str(float num, char* str) {
    int integer_part = (int)num;
    int decimal_part = (int)((num - integer_part) * 100);
    if (decimal_part < 0) decimal_part = -decimal_part;
    sprintf(str, "%d.%02d", integer_part, decimal_part);
}

// ========== MAIN ==========
int main(void) {
    uint16_t adc0, adc1, adc2;
    float voltage0, voltage1, voltage2;
    float temp_C, humidity_percent, gas_ppm;
    char buffer[20];
    uint8_t danger_level;
    
    // Initialize peripherals
    UART_Init(9600);
    ADC_Init();
    LED_Init();
    LCD_Init();
    
    // Startup message on UART
    UART_Print("\r\n====================================\r\n");
    UART_Print("  Heat Stress Monitor v2.0\r\n");
    UART_Print("====================================\r\n\r\n");
    
    // Startup message on LCD
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Heat Stress");
    LCD_SetCursor(1, 0);
    LCD_Print("Monitor v2.0");
    _delay_ms(2000);
    
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Initializing...");
    _delay_ms(1000);
    
    while (1) {
        // Read sensors
        adc0 = ADC_Read(0);
        adc1 = ADC_Read(1);
        adc2 = ADC_Read(2);
        
        // Convert to voltages
        voltage0 = (adc0 / 1024.0) * 5.0;
        voltage1 = (adc1 / 1024.0) * 5.0;
        voltage2 = (adc2 / 1024.0) * 5.0;
        
        // Calculate sensor values
        temp_C = voltage0 * 100.0;
        humidity_percent = (voltage1 / 5.0) * 100.0;
        gas_ppm = (voltage2 / 5.0) * 1000.0;
        
        // Determine status
        if (temp_C >= TEMP_DANGER || humidity_percent >= HUMIDITY_DANGER || gas_ppm >= GAS_DANGER) {
            danger_level = 2;
        } else if (temp_C >= TEMP_WARNING || humidity_percent >= HUMIDITY_WARNING || gas_ppm >= GAS_WARNING) {
            danger_level = 1;
        } else {
            danger_level = 0;
        }
        
        // Update LEDs
        LED_Update(danger_level);
        
        // ===== Update LCD =====
        LCD_Clear();
        
        // Line 1: T:25 H:50 SAFE
        LCD_SetCursor(0, 0);
        LCD_Print("T:");
        LCD_PrintInt((int)temp_C);
        LCD_Print(" H:");
        LCD_PrintInt((int)humidity_percent);
        LCD_Print("%");
        
        // Status on line 1 (right side)
        LCD_SetCursor(0, 11);
        switch (danger_level) {
            case 0: LCD_Print(" SAFE"); break;
            case 1: LCD_Print(" WARN"); break;
            case 2: LCD_Print("DANGR"); break;
        }
        
        // Line 2: Gas:300 ppm
        LCD_SetCursor(1, 0);
        LCD_Print("Gas:");
        LCD_PrintInt((int)gas_ppm);
        LCD_Print(" ppm");
        
        // ===== UART Output =====
        UART_Print("----- Readings -----\r\n");
        UART_Print("Temp: ");
        float_to_str(temp_C, buffer);
        UART_Print(buffer);
        UART_Print(" C\r\n");
        
        UART_Print("Humi: ");
        float_to_str(humidity_percent, buffer);
        UART_Print(buffer);
        UART_Print(" %\r\n");
        
        UART_Print("Gas:  ");
        float_to_str(gas_ppm, buffer);
        UART_Print(buffer);
        UART_Print(" ppm\r\n");
        
        UART_Print("Status: ");
        switch (danger_level) {
            case 0: UART_Print("SAFE\r\n"); break;
            case 1: UART_Print("WARNING\r\n"); break;
            case 2: UART_Print("DANGER\r\n"); break;
        }
        UART_Print("\r\n");
        
        _delay_ms(2000);
    }
    
    return 0;
}
