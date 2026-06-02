/**
 * @file    main.c
 * @brief   Heat Stress Monitoring System — ATmega8 Firmware
 * @version 2.0
 *
 * @details
 *  Monitors ambient temperature (LM35), relative humidity (HIH-5030), and
 *  air quality / CO2 index (MQ-135) through the ATmega8 10-bit ADC.
 *  Classifies readings into SAFE / WARNING / DANGER states and drives:
 *    - 16×2 LCD (4-bit, HD44780 compatible) via PORTB
 *    - Three-colour LED bar (Green / Yellow / Red) via PORTD
 *    - UART serial stream at 9600 bps for PC monitoring
 *
 * Target MCU : ATmega8-16PU
 * Clock      : 16 MHz external crystal
 * Compiler   : AVR-GCC (avr-gcc -mmcu=atmega8 -DF_CPU=16000000UL -O1)
 *
 * Pin Map:
 *   ADC0 (PC0) ← LM35    temperature sensor
 *   ADC1 (PC1) ← HIH-5030 humidity sensor
 *   ADC2 (PC2) ← MQ-135   air quality sensor
 *   PB0        → LCD RS
 *   PB1        → LCD EN
 *   PB2–PB5    → LCD D4–D7
 *   PD0        → Green LED  (SAFE)
 *   PD1/TXD    → UART TX to PC
 *   PD5        → Yellow LED (WARNING)
 *   PD6        → Red LED    (DANGER)
 *
 * Project: ECCE4227 Embedded Systems, Sultan Qaboos University, Spring 2026
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────────────────
 *  LCD Pin Definitions (4-bit mode on PORTB)
 * ───────────────────────────────────────────────────────────────────────────*/
#define LCD_PORT    PORTB
#define LCD_DDR     DDRB
#define LCD_RS      0       /**< PB0 — Register Select (0=cmd, 1=data) */
#define LCD_EN      1       /**< PB1 — Enable strobe                   */
#define LCD_D4      2       /**< PB2 — Data bit 4                      */
#define LCD_D5      3       /**< PB3 — Data bit 5                      */
#define LCD_D6      4       /**< PB4 — Data bit 6                      */
#define LCD_D7      5       /**< PB5 — Data bit 7                      */

/* ─────────────────────────────────────────────────────────────────────────────
 *  LED Pin Definitions (PORTD)
 * ───────────────────────────────────────────────────────────────────────────*/
#define LED_GREEN   0       /**< PD0 — Green  LED (SAFE)    */
#define LED_YELLOW  5       /**< PD5 — Yellow LED (WARNING) */
#define LED_RED     6       /**< PD6 — Red    LED (DANGER)  */

/* ─────────────────────────────────────────────────────────────────────────────
 *  Danger Threshold Constants
 *  Adjust these macros to tune site-specific alert levels.
 *  Reference: Oman Ministry of Manpower heat-safety guidelines (WBGT ≥ 35°C)
 * ───────────────────────────────────────────────────────────────────────────*/
#define TEMP_WARNING        30.0f   /**< °C  — enter warning zone    */
#define TEMP_DANGER         35.0f   /**< °C  — enter danger zone     */
#define HUMIDITY_WARNING    60.0f   /**< %RH — enter warning zone    */
#define HUMIDITY_DANGER     75.0f   /**< %RH — enter danger zone     */
#define GAS_WARNING        400.0f   /**< ppm — enter warning zone    */
#define GAS_DANGER         600.0f   /**< ppm — enter danger zone     */

/* ═════════════════════════════════════════════════════════════════════════════
 *  LCD Driver (HD44780, 4-bit mode)
 * ═════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Generate the EN high→low pulse required after each nibble.
 */
static void LCD_Enable(void)
{
    LCD_PORT |=  (1 << LCD_EN);
    _delay_us(1);
    LCD_PORT &= ~(1 << LCD_EN);
    _delay_us(100);
}

/**
 * @brief Write the lower 4 bits of @p nibble onto D4–D7.
 */
static void LCD_SendNibble(uint8_t nibble)
{
    /* Clear data lines */
    LCD_PORT &= ~((1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7));

    if (nibble & 0x01) LCD_PORT |= (1 << LCD_D4);
    if (nibble & 0x02) LCD_PORT |= (1 << LCD_D5);
    if (nibble & 0x04) LCD_PORT |= (1 << LCD_D6);
    if (nibble & 0x08) LCD_PORT |= (1 << LCD_D7);

    LCD_Enable();
}

/**
 * @brief Send a command byte to the LCD (RS = 0).
 */
static void LCD_Command(uint8_t cmd)
{
    LCD_PORT &= ~(1 << LCD_RS);    /* RS = 0 → command */
    LCD_SendNibble(cmd >> 4);      /* upper nibble first */
    LCD_SendNibble(cmd & 0x0F);    /* then lower nibble  */
    _delay_ms(2);
}

/**
 * @brief Send a data byte (character) to the LCD (RS = 1).
 */
static void LCD_Data(uint8_t data)
{
    LCD_PORT |= (1 << LCD_RS);     /* RS = 1 → data */
    LCD_SendNibble(data >> 4);
    LCD_SendNibble(data & 0x0F);
    _delay_us(100);
}

/**
 * @brief Initialise the LCD in 4-bit mode using the HD44780 reset sequence.
 *
 * The three 0x03 nibble writes with mandatory inter-write delays are required
 * to reliably switch from 8-bit to 4-bit mode regardless of power-on state.
 */
static void LCD_Init(void)
{
    /* Set all LCD pins as outputs */
    LCD_DDR |= (1 << LCD_RS) | (1 << LCD_EN) |
               (1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7);
    LCD_PORT  = 0x00;
    _delay_ms(50);                  /* Wait for VCC to stabilise */

    LCD_PORT &= ~(1 << LCD_RS);

    /* Reset sequence — HD44780 datasheet §4.4.1 */
    LCD_SendNibble(0x03); _delay_ms(5);
    LCD_SendNibble(0x03); _delay_us(150);
    LCD_SendNibble(0x03); _delay_us(150);
    LCD_SendNibble(0x02); _delay_us(150); /* Switch to 4-bit mode */

    LCD_Command(0x28);  /* Function Set: 4-bit, 2 lines, 5×8 font */
    LCD_Command(0x0C);  /* Display ON, cursor OFF, blink OFF       */
    LCD_Command(0x06);  /* Entry Mode: increment cursor, no shift  */
    LCD_Command(0x01);  /* Clear Display                           */
    _delay_ms(2);
}

/** @brief Clear the LCD and return cursor to home. */
static void LCD_Clear(void)
{
    LCD_Command(0x01);
    _delay_ms(2);
}

/**
 * @brief Position the cursor.
 * @param row  0 = first line, 1 = second line
 * @param col  0-based column index
 */
static void LCD_SetCursor(uint8_t row, uint8_t col)
{
    LCD_Command(row == 0 ? (0x80 + col) : (0xC0 + col));
}

/** @brief Write a null-terminated string to the LCD at the current cursor. */
static void LCD_Print(const char *str)
{
    while (*str) LCD_Data((uint8_t)(*str++));
}

/** @brief Write a signed integer to the LCD. */
static void LCD_PrintInt(int num)
{
    char buf[8];
    sprintf(buf, "%d", num);
    LCD_Print(buf);
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  UART Driver (TX only, 8-N-1)
 * ═════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Initialise UART for transmit at the requested baud rate.
 * @param baud  e.g. 9600
 */
static void UART_Init(uint16_t baud)
{
    uint16_t ubrr = (uint16_t)(F_CPU / 16 / baud) - 1;
    UBRRH = (uint8_t)(ubrr >> 8);
    UBRRL = (uint8_t) ubrr;
    UCSRB = (1 << TXEN);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0); /* 8 data bits */
}

/**
 * @brief Blocking transmit of one byte via UART.
 */
static void UART_Transmit(uint8_t data)
{
    while (!(UCSRA & (1 << UDRE)));  /* Wait for empty TX buffer */
    UDR = data;
}

/** @brief Transmit a null-terminated string via UART. */
static void UART_Print(const char *str)
{
    while (*str) UART_Transmit((uint8_t)(*str++));
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  ADC Driver (10-bit, AVCC = 5 V reference)
 * ═════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Initialise the ADC.
 *
 * Prescaler = 128 → ADC clock = 125 kHz (well within 50–200 kHz for 10-bit).
 * A dummy conversion is run to warm up the ADC before the main loop.
 */
static void ADC_Init(void)
{
    ADMUX  = (1 << REFS0);                              /* AVCC reference */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); /* enable, /128 */
    ADCSRA |= (1 << ADSC);                             /* dummy conversion */
    while (ADCSRA & (1 << ADSC));
}

/**
 * @brief Read a single 10-bit ADC sample from the specified channel.
 * @param channel  ADC channel number (0–7)
 * @return         10-bit result (0–1023)
 *
 * A 10 µs settling delay is applied after channel selection to prevent
 * inter-channel crosstalk on the S&H capacitor.
 */
static uint16_t ADC_Read(uint8_t channel)
{
    ADMUX = (1 << REFS0) | (channel & 0x07);
    _delay_us(10);              /* Input settling time */
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  LED Driver
 * ═════════════════════════════════════════════════════════════════════════════*/

/** @brief Configure LED pins as outputs and ensure all are off. */
static void LED_Init(void)
{
    DDRD  |=  (1 << LED_GREEN) | (1 << LED_YELLOW) | (1 << LED_RED);
    PORTD &= ~((1 << LED_GREEN) | (1 << LED_YELLOW) | (1 << LED_RED));
}

/**
 * @brief Illuminate the LED corresponding to the current alert level.
 * @param level  0 = SAFE (green), 1 = WARNING (yellow), 2 = DANGER (red)
 */
static void LED_Update(uint8_t level)
{
    /* Turn all off first */
    PORTD &= ~((1 << LED_GREEN) | (1 << LED_YELLOW) | (1 << LED_RED));

    switch (level) {
        case 0:  PORTD |= (1 << LED_GREEN);  break;
        case 1:  PORTD |= (1 << LED_YELLOW); break;
        case 2:  PORTD |= (1 << LED_RED);    break;
        default: break;
    }
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Utility
 * ═════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Convert a float to a "integer.decimal" string without float printf.
 *
 * AVR-GCC disables floating-point printf by default to conserve flash.
 * This function uses integer arithmetic to produce the formatted result,
 * saving ~1.5 KB compared to enabling the float printf library.
 *
 * @param num  Float value to convert
 * @param str  Output buffer (minimum 10 bytes)
 */
static void float_to_str(float num, char *str)
{
    int integer_part = (int)num;
    int decimal_part = (int)((num - (float)integer_part) * 100.0f);
    if (decimal_part < 0) decimal_part = -decimal_part;
    sprintf(str, "%d.%02d", integer_part, decimal_part);
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Main
 * ═════════════════════════════════════════════════════════════════════════════*/

int main(void)
{
    uint16_t adc0, adc1, adc2;
    float    voltage0, voltage1, voltage2;
    float    temp_C, humidity_percent, gas_ppm;
    char     buffer[20];
    uint8_t  danger_level;

    /* ── Peripheral initialisation ─────────────────────────────────────────*/
    UART_Init(9600);
    ADC_Init();
    LED_Init();
    LCD_Init();

    /* ── Startup banner (UART) ─────────────────────────────────────────────*/
    UART_Print("\r\n====================================\r\n");
    UART_Print("  Heat Stress Monitor v2.0\r\n");
    UART_Print("====================================\r\n\r\n");

    /* ── Startup screen (LCD) ──────────────────────────────────────────────*/
    LCD_Clear();
    LCD_SetCursor(0, 0); LCD_Print("Heat Stress");
    LCD_SetCursor(1, 0); LCD_Print("Monitor v2.0");
    _delay_ms(2000);
    LCD_Clear();
    LCD_SetCursor(0, 0); LCD_Print("Initializing...");
    _delay_ms(1000);

    /* ══════════════════════════════════════════════════════════════════════
     *  Main acquisition loop — runs every 2 seconds
     * ══════════════════════════════════════════════════════════════════════*/
    while (1)
    {
        /* ── Step 1: Acquire raw ADC samples ───────────────────────────────*/
        adc0 = ADC_Read(0);   /* LM35     — temperature */
        adc1 = ADC_Read(1);   /* HIH-5030 — humidity    */
        adc2 = ADC_Read(2);   /* MQ-135   — air quality */

        /* ── Step 2: Convert to physical voltage ───────────────────────────
         *   V = ADC × Vref / 2^10  (Vref = 5.0 V, 10-bit)                 */
        voltage0 = (adc0 / 1024.0f) * 5.0f;
        voltage1 = (adc1 / 1024.0f) * 5.0f;
        voltage2 = (adc2 / 1024.0f) * 5.0f;

        /* ── Step 3: Apply sensor transfer functions ───────────────────────
         *   LM35     : T[°C]   = V × 100        (10 mV/°C)
         *   HIH-5030 : H[%]    = (V / 5.0) × 100 (ratiometric 0–5 V)
         *   MQ-135   : G[ppm]  = (V / 5.0) × 1000 (linear scale)          */
        temp_C           = voltage0 * 100.0f;
        humidity_percent = (voltage1 / 5.0f) * 100.0f;
        gas_ppm          = (voltage2 / 5.0f) * 1000.0f;

        /* ── Step 4: Threshold classification ─────────────────────────────
         *   Worst-case logic: any single parameter above its danger/warning
         *   threshold elevates the entire system to that level.             */
        if (temp_C >= TEMP_DANGER ||
            humidity_percent >= HUMIDITY_DANGER ||
            gas_ppm >= GAS_DANGER)
        {
            danger_level = 2;   /* DANGER  */
        }
        else if (temp_C >= TEMP_WARNING ||
                 humidity_percent >= HUMIDITY_WARNING ||
                 gas_ppm >= GAS_WARNING)
        {
            danger_level = 1;   /* WARNING */
        }
        else
        {
            danger_level = 0;   /* SAFE    */
        }

        /* ── Step 5a: Update LEDs ──────────────────────────────────────────*/
        LED_Update(danger_level);

        /* ── Step 5b: Update LCD ───────────────────────────────────────────
         *   Line 0: "T:xx H:xx%  SAFE"
         *   Line 1: "Gas:xxx ppm"                                           */
        LCD_Clear();

        LCD_SetCursor(0, 0);
        LCD_Print("T:");   LCD_PrintInt((int)temp_C);
        LCD_Print(" H:"); LCD_PrintInt((int)humidity_percent);
        LCD_Print("%");

        LCD_SetCursor(0, 11);
        switch (danger_level) {
            case 0: LCD_Print(" SAFE"); break;
            case 1: LCD_Print(" WARN"); break;
            case 2: LCD_Print("DANGR"); break;
        }

        LCD_SetCursor(1, 0);
        LCD_Print("Gas:"); LCD_PrintInt((int)gas_ppm); LCD_Print(" ppm");

        /* ── Step 5c: UART report ──────────────────────────────────────────*/
        UART_Print("----- Readings -----\r\n");

        UART_Print("Temp: ");
        float_to_str(temp_C, buffer);
        UART_Print(buffer); UART_Print(" C\r\n");

        UART_Print("Humi: ");
        float_to_str(humidity_percent, buffer);
        UART_Print(buffer); UART_Print(" %\r\n");

        UART_Print("Gas:  ");
        float_to_str(gas_ppm, buffer);
        UART_Print(buffer); UART_Print(" ppm\r\n");

        UART_Print("Status: ");
        switch (danger_level) {
            case 0: UART_Print("SAFE\r\n");    break;
            case 1: UART_Print("WARNING\r\n"); break;
            case 2: UART_Print("DANGER\r\n");  break;
        }
        UART_Print("\r\n");

        _delay_ms(2000);    /* 2-second acquisition interval */
    }

    return 0;   /* Never reached */
}
