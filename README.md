# Nintendo Switch · BT & Beats — 1.2.0

Creé este proyecto para personalizar el sonido y las imágenes de arranque de mi Nintendo Switch y sincronizar controles de sysMMC a emuMMC.

El asistente permite elegir tu audio y convertirlo con FFmpeg, instalar un logo de Hekate y preparar un único sincronizador Bluetooth. Muestra los cambios antes de instalar y guarda un respaldo en tu ordenador.

## Instalar

Conecta la microSD de tu Switch, con Hekate, Atmosphère y emuMMC ya funcionando. Abre una terminal sin administrador y pega el comando de tu sistema.

**Windows — PowerShell**

```powershell
irm https://raw.githubusercontent.com/JheraldC/Nintendo-Switch-BT-Sound/main/install.ps1 | iex
```

**macOS, Ubuntu o Fedora — Terminal**

```sh
curl -fsSL https://raw.githubusercontent.com/JheraldC/Nintendo-Switch-BT-Sound/main/install.sh | sh
```

El asistente descarga las herramientas necesarias. No necesitas instalar Git, Python ni compilar manualmente. [Detalles de instalación](docs/INSTALACION.md).

## Versiones y compatibilidad

| Componente | Versión de mi instalación |
|---|---|
| BT & Beats | 1.2.0 |
| Atmosphère | 1.12.2 |
| Hekate | 6.5.4 anterior y 6.5.5 actual |
| sysMMC | 23.0.1, con microSD FAT32 y emuMMC habilitada |

En la instalación actual, BT Sync terminó en **OK** y volvió a Hekate, conservando la continuación 6.5.4. Falta comprobar la reconexión de los mandos. El perfil habitual es Pro Controller/Joy-Con; otros modelos mediante MissionControl siguen siendo experimentales. [Compatibilidad y alcance de las pruebas](docs/COMPATIBILIDAD.md).

## Sincronizar controles

Después de preparar Bluetooth, abre **Hekate → Payloads → BT-Sync-Una-Vez.bin** y entra en emuMMC cuando vuelva a Hekate. Solo ejecutas ese archivo; repítelo cuando cambien los emparejamientos de sysMMC.

Si aparece FALLO tras cambiar la configuración, usa la opción **4** del asistente para actualizar el sincronizador con respaldo. Cada consola necesita su propia preparación.

[Sonido](docs/SONIDO.md) · [Imágenes](docs/ARRANQUE.md) · [Bluetooth](docs/BLUETOOTH.md) · [Reversión](docs/REVERSION.md) · [Licencias](LICENSES.md)

![Diseño original de BootSound](components/bootsplash/images/BootSound_splash.png)
