# BT Sync Una Vez — payload manual

[Guía de requisitos, preparación, instalación y uso](../../docs/BLUETOOTH.md).

Preparé este payload para sincronizar mis controles de sysMMC a emuMMC cuando lo necesito. La pantalla muestra el progreso y el resultado: OK en verde o FALLO en rojo. Al terminar vuelve automáticamente al menú de Hekate. Las entradas sysMMC/emuMMC conservan su funcionamiento habitual.

El árbol público no contiene `include/auto_binding.h`: `prepare.py` lo genera localmente para la SD destino. No publiques ese archivo ni `output/`. Para comprobar fuentes sin consola, ejecuta `python tests/test_portable.py --prepare-synthetic`; solo produce bindings de prueba y nunca se debe instalar ese binario en una consola.

GPL-2.0 y avisos incluidos. Los nombres internos autosync-v1 corresponden al origen del proyecto, no a una ejecución automática en esta edición.
