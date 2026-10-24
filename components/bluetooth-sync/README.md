# BT Sync Una Vez — payload manual

[Guía de requisitos, preparación, instalación y uso](../../docs/BLUETOOTH.md).

Esta es la interfaz actual: progreso por etapas, pausa de 500 ms al completar el 100%, OK verde/FALLO rojo 3 segundos y regreso automático al menú de Hekate. No espera una pulsación al finalizar. No modifica las entradas Original/Hack ni ejecuta sincronización en el arranque normal.

El árbol público no contiene `include/auto_binding.h`: `prepare.py` lo genera localmente para la SD destino. No publiques ese archivo ni `output/`. Para comprobar fuentes sin consola, ejecuta `python tests/test_portable.py --prepare-synthetic`; solo produce bindings de prueba y nunca se debe instalar ese binario en una consola.

GPL-2.0 y avisos incluidos. Los nombres internos autosync-v1 corresponden al origen del proyecto, no a una ejecución automática en esta edición.
