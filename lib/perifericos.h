#include <hardware/pio.h>           
#include "hardware/clocks.h" 
#include "hardware/pwm.h"
#include "hardware/i2c.h" // Biblioteca de hardware para I2C
#include "stdio.h"
#include "animacao_matriz.pio.h" // Biblioteca PIO para controle de LEDs WS2818B
#include "ssd1306.h" // Biblioteca para controle do display OLED


// Definição de constantes
#define LED_PIN_GREEN 11
#define LED_PIN_BLUE 12
#define LED_PIN_RED 13
#define LED_COUNT 25            // Número de LEDs na matriz
#define MATRIZ_PIN 7            // Pino GPIO conectado aos LEDs WS2818B
#define BUZZER_A 21
#define BOTAO_A 5
#define BOTAO_B 6
#define JOY_X 27 // Joystick está de lado em relação ao que foi dito no pdf
#define JOY_Y 26
#define max_value_joy 4065.0 // (4081 - 16) que são os valores extremos máximos lidos pelo meu joystick

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C


// Cabeçalho para funções de controle de periféricos
void init_ssd();
void display_info(float temperatura, int batimento);
void pwm_setup(uint pino);
void iniciar_buzzer(uint pin);
void parar_buzzer(uint pin);