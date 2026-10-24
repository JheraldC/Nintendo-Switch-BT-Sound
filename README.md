# Nintendo Switch · BT & Beats — Sonido y controles

**Sincroniza tus controles y dale sonido al arranque.**

Sonido, imágenes de arranque y sincronización manual de Pro Controller/Joy-Con para Nintendo Switch con Atmosphère y emuMMC.

| Componente | Para qué sirve | Instalación |
|---|---|---|
| **BootSound** | Reproduce un WAV al entrar a Hack y después termina. Incluye el sonido Yoshi usado en la SD del autor y un ejemplo generado. | [Guía de sonido](docs/SONIDO.md) |
| **Arranque e imágenes** | Incluye los diseños, un logo listo para Hekate y herramientas para preparar splashes de Atmosphère. | [Guía de imágenes](docs/ARRANQUE.md) |
| **BT Sync Una Vez** | Importa los Pro Controller/Joy-Con compatibles de Original a la base externa de Hack de **la misma consola**, cuando abres el payload. | [Guía Bluetooth](docs/BLUETOOTH.md) |

## Empezar en Windows

1. Descarga **Nintendo-Switch-BT-Sound-v1.0.3.zip** desde [Releases](https://github.com/JheraldC/Nintendo-Switch-BT-Sound/releases/latest) y extráelo completamente en el PC.
2. Instala [Python 3.11 o posterior](https://www.python.org/downloads/windows/) si no lo tienes.
3. Conecta una SD que ya arranque correctamente con Hekate, Atmosphère y emuMMC.
4. Abre **instalar.cmd**, elige el componente y la letra de la SD. Primero se muestra el plan; solo se instala si escribes `INSTALAR`.

El sonido y el logo ya incluyen sus archivos preparados. **Bluetooth necesita compilarse localmente para la SD de destino**, con devkitARM y WSL; el asistente reúne la preparación, compilación, pruebas y respaldo. Sus requisitos son más estrictos: consulta la guía antes de elegir esa opción.

El asistente conserva una copia verificada de los archivos que reemplaza en `Documentos/SwitchBackups`. No instala Atmosphère, firmware ni un método de modificación de la consola. Durante la preparación de esta publicación no se modificó la SD del autor.

## Cómo funciona el payload Bluetooth

Abre **Hekate → Payloads → BT-Sync-Una-Vez.bin**. Aparecen “Cargando...”, la etapa y una barra de progreso. Al completarse correctamente, el 100% permanece **0,5 segundos**. Después muestra **OK verde** o **FALLO rojo** durante **3 segundos** y vuelve al menú de Hekate. Si no puede volver de forma validada, se apaga.

Luego entra normalmente a **Sistema Hack**. El sincronizador no se ejecuta en cada arranque y conserva las entradas Original/Hack. “Una vez” significa una ejecución manual: puedes volver a abrir el payload si emparejas más controles en Original. El sonido sí se reproduce una vez **por arranque de Hack**.

## Compatibilidad y comprobaciones

- Bluetooth se limita al perfil analizado de **Original 23.0.1**, SD **FAT32**, emuMMC habilitada, base Bluetooth externa existente y **Hekate 6.5.4 exacto**. No asumas compatibilidad con otras versiones o modelos sin verificarla.
- Cada consola utiliza sus propias claves locales. **Nunca compartas `prod.keys`, la base Bluetooth, el historial ni un payload generado para otra SD.**
- El autor comprobó físicamente el resultado OK y la pantalla de progreso del payload. La pausa final de 0,5 segundos tiene comprobación automática de código; no se documenta una nueva prueba física específica de esa pausa.
- BootSound 0.4.1 se recompiló con el nuevo nombre y pasó sus comprobaciones de PC. Se conserva la configuración de las instalaciones anteriores; la prueba física específica de esta edición está pendiente.
- [Verificación de esta publicación](docs/VERIFICACION.md), [reversión](docs/REVERSION.md) y [licencias y créditos](LICENSES.md).

No se incluyen claves, datos de emparejamiento reales, copias NAND, juegos ni un `package3` completo. Las imágenes y clips conservan los derechos de sus respectivos titulares; su inclusión no los convierte en contenido MIT/GPL.

![Diseño original de BootSound](components/bootsplash/images/BootSound_splash.png)
