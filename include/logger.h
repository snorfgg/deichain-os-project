/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#ifndef LOGGER_H
#define LOGGER_H

// Inicializa o logger (abre o ficheiro de log)
int init_logger();

// Fecha o logger (fecha o ficheiro de log)
void close_logger();

// Função de logging thread-safe – escreve a mensagem formatada no stdout e no ficheiro de log
void log_message(const char *format, ...);

#endif // LOGGER_H
