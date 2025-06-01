#include "perifericos.h"

// Declaração de variáveis globais
PIO pio;
uint sm;


// Declaração de matriz de LEDs
uint padrao_led[10][LED_COUNT] = {
    {0, 0, 1, 0, 0,
     0, 1, 1, 1, 0,
     1, 1, 1, 1, 1,
     1, 1, 1, 1, 1,
     0, 1, 1, 1, 0,
    }, 
    {2, 0, 2, 0, 2,
     0, 2, 2, 2, 0,
     2, 2, 2, 2, 2,
     0, 2, 2, 2, 0,
     2, 0, 2, 0, 2,
    }, 
    {0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
    } // Desliga os LEDs
};

// Ordem da matriz de LEDS, útil para poder visualizar na matriz do código e escrever na ordem correta do hardware
int ordem[LED_COUNT] = {0, 1, 2, 3, 4, 9, 8, 7, 6, 5, 10, 11, 12, 13, 14, 19, 18, 17, 16, 15, 20, 21, 22, 23, 24};


// Rotina para definição da intensidade de cores do led
static uint32_t matrix_rgb(unsigned r, unsigned g, unsigned b){
    return (g << 24) | (r << 16) | (b << 8);
}

// Rotina para desenhar o padrão de LED
void display_desenho(int number){
    uint32_t valor_led;

    for (int i = 0; i < LED_COUNT; i++){
        // Define a cor do LED de acordo com o padrão
        if (padrao_led[number][ordem[24 - i]] == 1){
            valor_led = matrix_rgb(0, 0, 10); // Azul
        } else if (padrao_led[number][ordem[24 - i]] == 2){
            valor_led = matrix_rgb(30, 10, 0); // Amarelo
        } else{
            valor_led = matrix_rgb(0, 0, 0); // Desliga o LED
        }
        // Atualiza o LED
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Configuração do PWM
void pwm_setup(uint pino) {
    gpio_set_function(pino, GPIO_FUNC_PWM);   // Configura o pino como saída PWM
    uint slice = pwm_gpio_to_slice_num(pino); // Obtém o slice correspondente
    
    pwm_set_wrap(slice, max_value_joy);  // Define o valor máximo do PWM

    pwm_set_enabled(slice, true);  // Habilita o slice PWM
}


// Função para iniciar o buzzer
void iniciar_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o slice correspondente

    pwm_set_clkdiv(slice_num, 125); // Define o divisor de clock
    pwm_set_wrap(slice_num, 1000);  // Define o valor máximo do PWM

    pwm_set_gpio_level(pin, 10); //Para um som mais baixo foi colocado em 10
    pwm_set_enabled(slice_num, true);
}

// Função para parar o buzzer
void parar_buzzer(uint pin) {
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o slice correspondente
    pwm_set_enabled(slice_num, false); // Desabilita o slice PWM
    gpio_put(pin, 0); // Coloca o pino em nível para garantir que o buzzer está desligado
}