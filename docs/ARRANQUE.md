# Imágenes de arranque

Hay tres mecanismos independientes. Cambiar uno no garantiza que se muestre en todas las rutas de arranque.

| Pantalla | Qué necesita | Alcance de esta publicación |
|---|---|---|
| Logo de Hekate | BMP 32-bit, retrato 720×1280 y `logopath=` en una entrada | Logo preparado y asistente con respaldo |
| Splash temprano de Atmosphère | PNG 1280×720 insertado en una **copia** de `package3` | Herramienta para preparar y verificar la copia local |
| Splash interno del módulo `boot` de Atmosphère | Imagen `.inc`, compilar `boot.kip` de la versión exacta y reempaquetar | Conversores, comprobador y repacker; proceso avanzado |

## Logo listo para Hekate

Abre `instalar.cmd`, elige **2** e indica el nombre de la entrada Hack. El plan copia `bootloader/res/jherald_bootlogo.bmp` y configura `logopath=` solo en esa entrada, conservando su nombre y las otras opciones. No activa el arranque automático ni agrega entradas. El respaldo permite revertirlo.

**Este logo se usa en la ruta de autoboot de Hekate con tiempo de espera. Entrar manualmente desde Nyx/Launch puede omitirlo.** Por eso no promete replicar la pantalla interna de Atmosphère en todos los arranques. Si tu consola ya muestra el splash que quieres, conserva tu `package3`.

Las imágenes editables están en `components/bootsplash/images/`. El BMP preparado usa `splash_opcion_1_oscura.png`. También hay un diseño original `BootSound_splash.png`. Para convertir tu propia imagen:

```powershell
python -m pip install -r requirements.txt
python components/bootsplash/tools/hekate_splash/make_hekate_bootlogo.py mi_imagen.png mi_logo.bmp
```

Entrada 1280×720 PNG. `--resize` permite otras resoluciones **16:9**; no deforma imágenes cuadradas. Revisa el resultado antes de instalarlo.

## Splash temprano de package3

El asistente no reemplaza `package3`. La herramienta siguiente solo genera una **salida nueva en el PC**, valida tamaño/firma, limita las diferencias al área de imagen y verifica todos los píxeles:

```powershell
python tools/patch_early_splash.py --base C:/Copias/package3 --image components/bootsplash/images/BootSound_splash.png --out C:/Copias/package3.personalizado
```

Trabaja siempre con la versión de `package3` correspondiente a tu Atmosphère y conserva el original fuera de la SD. Este splash pertenece al flujo de Fusee: una entrada directa `pkg3=atmosphere/package3` de Hekate puede omitirlo. No cambies la ruta de arranque solo para forzarlo sin revisar la configuración.

## Splash interno de Atmosphère: avanzado

Usa el código fuente de **la misma versión y revisión** que tu `package3`. Genera el array desde un PNG con:

```powershell
python components/bootsplash/tools/boot_splash/png_to_boot_inc.py imagen.png boot_splash_screen_notext.inc
python components/bootsplash/tools/boot_splash/boot_inc_to_png.py boot_splash_screen_notext.inc preview.png
```

El array no se rota. El conversor acepta hasta 1280×720 y alfa 0..255, pero ese límite geométrico **no significa que quepa en un KIP**. Todos los KIP comparten 3 MiB; mide el tamaño final y el espacio disponible. Compila `boot.kip` con el sistema de compilación oficial de esa revisión de Atmosphère. No mezcles un KIP antiguo con `package3` nuevo.

Después, sobre copias en el PC:

```powershell
python components/bootsplash/tools/package3/replace_boot_kip.py package3.base boot_custom.kip package3.custom
python components/bootsplash/tools/package3/verify_second_splash.py boot_custom.kip boot_splash_screen_notext.inc 640 360
```

Sustituye 640×360 por las dimensiones reales del array. El repacker conserva los demás componentes y comprueba el presupuesto. No valida por sí mismo compatibilidad entre versiones; una prueba física y un respaldo siguen siendo necesarios. No se distribuye un `package3` modificado ni un `boot.kip` antiguo como actualización universal.

Las notas en `components/bootsplash/historical-docs/` registran experimentos con Atmosphère 1.11.2. Sus estados por fases son históricos; esta guía describe lo publicado ahora.
