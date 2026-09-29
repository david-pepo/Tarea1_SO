#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>     // Para fork() y usleep()
#include <sys/wait.h>   // Para wait()
#include <sys/types.h>

#define MAX_LINE 256
#define MAX_DEPS 20
#define MAX_ACTIVIDADES 10000

typedef struct {
    char id[32];
    char nombre[100];
    int tiempo_ms;
    char dependencias[MAX_DEPS][32];
    int num_dependencias;
    
    int estado;    
    pid_t pid;     
    
    // -- NUEVOS CAMPOS PARA PIPES --
    char mensaje_salida[100]; // Aquí el padre guardará el mensaje que dejó el hijo
    int fd_in[2];             // Pipe: Padre -> Hijo (para entregar insumos)
    int fd_out[2];            // Pipe: Hijo -> Padre (para enviar resultado final)
} Actividad;

Actividad plan[MAX_ACTIVIDADES];
int total_actividades = 0;

int main(int argc, char *argv[]) {
    // 1. Validar argumentos
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <archivo_plan.txt> <K_concurrencia>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Error al abrir el archivo");
        return 1;
    }

    char buffer[MAX_LINE];
    
    printf("--- Construyendo el DAG en memoria ---\n");
    while (fgets(buffer, sizeof(buffer), file) && total_actividades < MAX_ACTIVIDADES) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (strlen(buffer) == 0) continue;

        // Punteros para usar strtok_r y evitar conflictos al anidar parseos
        char *saveptr1, *saveptr2;
        
        char *id_str = strtok_r(buffer, ":", &saveptr1);
        char *nombre_str = strtok_r(NULL, ":", &saveptr1);
        char *tiempo_str = strtok_r(NULL, ":", &saveptr1);
        char *deps_str = strtok_r(NULL, ":", &saveptr1);

        if (!id_str || !nombre_str) continue; 

        // Guardar ID y Nombre
        strcpy(plan[total_actividades].id, id_str);
        strcpy(plan[total_actividades].nombre, nombre_str);

        // Guardar Tiempo (fijo o aleatorio entre 100 y 5000)
        int tiempo_ms = 0;
        if (tiempo_str != NULL && atoi(tiempo_str) > 0) {
            tiempo_ms = atoi(tiempo_str);
        } else {
            tiempo_ms = (rand() % 4901) + 100;
        }
        plan[total_actividades].tiempo_ms = tiempo_ms;

        // Guardar Dependencias (separando por comas)
        plan[total_actividades].num_dependencias = 0;
        if (deps_str != NULL) {
            char *dep = strtok_r(deps_str, ",", &saveptr2);
            while (dep != NULL && plan[total_actividades].num_dependencias < MAX_DEPS) {
                // Eliminar posibles espacios en blanco iniciales (opcional pero seguro)
                while(*dep == ' ') dep++; 
                strcpy(plan[total_actividades].dependencias[plan[total_actividades].num_dependencias], dep);
                plan[total_actividades].num_dependencias++;
                dep = strtok_r(NULL, ",", &saveptr2);
            }
        }
        
        total_actividades++;
    }
    fclose(file);

    // Capturar el límite K desde argv[2]
    int K = atoi(argv[2]);
    if (K <= 0) K = 1; // Seguridad mínima

    // Inicializar estados
    for (int i = 0; i < total_actividades; i++) {
        plan[i].estado = 0;
    }

    int terminadas = 0;
    int en_vuelo = 0;

    printf("\n--- Iniciando Simulación con K=%d ---\n", K);

    // Bucle principal del planificador
    while (terminadas < total_actividades) {

        // 1. Intentar lanzar nuevas actividades si no hemos superado K
        for (int i = 0; i < total_actividades && en_vuelo < K; i++) {
            if (plan[i].estado == 0) { // Si está pendiente
                
                // Verificar si todas sus dependencias están terminadas
                int dependencias_listas = 1;
                for (int j = 0; j < plan[i].num_dependencias; j++) {
                    for (int k = 0; k < total_actividades; k++) {
                        if (strcmp(plan[i].dependencias[j], plan[k].id) == 0) {
                            if (plan[k].estado != 2) {
                                dependencias_listas = 0; // Falla una dependencia
                            }
                            break;
                        }
                    }
                }

                // Si está lista para ejecutarse, hacemos el fork
                if (dependencias_listas) {
                    plan[i].estado = 1; // Marcamos como en ejecución
                    en_vuelo++;

                    pid_t pid = fork();
                    if (pid == 0) {
                        // --- ESPACIO DEL PROCESO HIJO ---
                        printf("[Hijo %d] Iniciando %s (%d ms)\n", getpid(), plan[i].nombre, plan[i].tiempo_ms);
                        
                        // Simular el trabajo (usleep usa microsegundos, así que multiplicamos por 1000)
                        if (pid == 0) {
                        // --- ESPACIO DEL PROCESO HIJO ---
                        printf("[Hijo %d] Iniciando %s (%d ms)\n", getpid(), plan[i].nombre, plan[i].tiempo_ms);
                        
                        // Uso estándar de POSIX para pausar el proceso en milisegundos
                        struct timespec ts;
                        ts.tv_sec = plan[i].tiempo_ms / 1000;
                        ts.tv_nsec = (plan[i].tiempo_ms % 1000) * 1000000L;
                        nanosleep(&ts, NULL);
                        
                        printf("[Hijo %d] Terminó %s\n", getpid(), plan[i].nombre);
                        exit(0); // El hijo muere aquí obligatoriamente
                    }
                        
                        printf("[Hijo %d] Terminó %s\n", getpid(), plan[i].nombre);
                        exit(0); // El hijo muere aquí obligatoriamente
                    } else if (pid > 0) {
                        // --- ESPACIO DEL PROCESO PADRE ---
                        plan[i].pid = pid; // Guardamos el PID del hijo para saber quién es cuando termine
                    } else {
                        perror("Error al hacer fork");
                        exit(1);
                    }
                }
            }
        }

        // 2. Si hay procesos corriendo, debemos esperar obligatoriamente a que uno termine
        // Esto evita el busy-waiting. wait() detiene al padre hasta que un hijo avise que murió.
        if (en_vuelo > 0) {
            pid_t pid_terminado = wait(NULL);
            if (pid_terminado > 0) {
                // Buscamos a qué actividad le pertenecía este PID
                for (int i = 0; i < total_actividades; i++) {
                    if (plan[i].pid == pid_terminado) {
                        plan[i].estado = 2; // Marcamos como terminada
                        en_vuelo--;
                        terminadas++;
                        printf("[Padre] Registro que %s finalizó. (%d/%d completadas)\n", 
                               plan[i].nombre, terminadas, total_actividades);
                        break;
                    }
                }
            }
        }
    }

    printf("\n--- Simulación Completada Exitosamente ---\n");
    return 0;
}
