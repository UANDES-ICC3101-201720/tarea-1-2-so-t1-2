# Tarea 1 Parte 2 - Sistemas Operativos 2018-02
> Pedro Ricci
### Problemas encontrados en la implementación
- El debuging de threads no es fácil, así que se optó por implementar un loop en vez de crear threads y luego que se verificó el correcto funcionamineto, se paralelizó con pthreads.
- En la implementación del algoritmo de ordenación quicksort paralelo existía un comportamiento imprevisto aleatorio que se solucionó agregando un sleep de una fracción de segundo dentro de la función "recursive_parallel_quicksort". El motivo de porqué esto funcionó no está claro. 
### Funcionalidad pendiente de implementar
- Aunque sí se utilizan locks para evitar race conditions al elegir el pivote a utilizar, debido a la manera que se implementó la función paralelizada, hacer un broadcast para avisarle a los demás threads que ya se eligió un pivote no tenía mucho sentido. Esto debido a que el primer thread no ejecuta una función distinta a la de los demás threads.

