#include "perifericos.h"

// Declaração de variáveis globais
ssd1306_t ssd;


void init_ssd(){
    // Inicialização do display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
}

void display_info(float temperatura, int batimento) {
    char buffer[32]; // Buffer para formatação de strings
    
    ssd1306_fill(&ssd, 1);

    // Borda do display
    ssd1306_rect(&ssd, 3, 3, 122, 58, false, true);

    // Título do sistema
    ssd1306_draw_string(&ssd, "Paciente", 32, 6);
    ssd1306_draw_string(&ssd, "Seguro", 38, 16);
    
    // Linha divisória
    ssd1306_hline(&ssd, 8, 119, 26, true);
    
    // Exibir temperatura
    sprintf(buffer, "Temp: %.1f C", temperatura);
    ssd1306_draw_string(&ssd, buffer, 8, 32);
    
    // Exibir batimento
    sprintf(buffer, "BPM: %d", batimento);
    ssd1306_draw_string(&ssd, buffer, 8, 42);

    // Cruz médica
    // Barra vertical da cruz (3 pixels de largura)
    ssd1306_rect(&ssd, 6, 112, 3, 12, true, true);
    
    // Barra horizontal da cruz (3 pixels de altura)
    ssd1306_rect(&ssd, 10, 108, 11, 3, true, true);

    ssd1306_send_data(&ssd);
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