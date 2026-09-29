Autor: David Benjamin Fuentes Castro (Proyecto desarrollado de forma individual)

Modo de uso: 
Para compilar el programa, simplemente ejecute el comando `make` en la terminal, el cual utiliza las flags estrictas requeridas por la rúbrica (-Wall -Wextra -std=c17). Para iniciar la simulación, ejecute `./planificador plan.txt K`, donde K representa el límite máximo de procesos concurrentes.

Funciones implementadas: 
Se utilizó un arreglo de estructuras `Actividad` para modelar el DAG en memoria tras el parseo del archivo. El control de concurrencia se implementó utilizando `fork()` para la creación de procesos hijos (limitados por K) y `wait()` en el proceso padre para evitar el busy-waiting. Para el paso de mensajes se utilizaron tuberías (`pipe`) y para la clausura de la Seremi se capturó la señal `SIGINT` mediante la función `sigaction()`.

Justificación de diseño (Broker de Mensajes): 
Para garantizar que el sistema soporte la carga de estrés de 10.000 actividades sin colapsar por el límite de descriptores de archivos del sistema operativo (error EMFILE), se diseñó al proceso Padre como un "Broker" intermediario. En lugar de abrir todos los pipes al inicio, el Padre crea y cierra las tuberías (fd_in y fd_out) dinámicamente justo antes de cada `fork()` y lee los resultados tras el `wait()`, reciclando los recursos eficientemente.
