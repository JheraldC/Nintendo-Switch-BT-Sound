# Licencias y créditos

- BootSound: MIT, texto en `components/bootsound/LICENSE`. Versión 1.0.0, programa `42000000004A4253`. Incluye fuentes C, descriptor y herramientas de construcción. Conserva los avisos de autoría del proyecto original y la carpeta de configuración histórica.
- BT Sync Una Vez: GPL-2.0, en `components/bluetooth-sync/LICENSE` y `module/LICENSE`. Conserva los avisos de sus dependencias y el núcleo congelado identificado por `core-freeze.json`.
- TegraExplorer/BDK: código y avisos originales en `components/bluetooth-sync/vendor/tegraexplorer/`, bajo su GPL-2.0. No se afirma autoría sobre este código. El lector incluido conserva sus avisos internos.
- Hekate 6.5.4: el binario oficial sin modificar de `test-assets` se usa como fixture de pruebas; no es un payload Bluetooth universal. GPL-2.0. [Fuentes correspondientes y licencia](https://github.com/CTCaer/hekate/tree/v6.5.4), [release original](https://github.com/CTCaer/hekate/releases/tag/v6.5.4). La licencia GPL-2.0 también se conserva en el componente.
- Herramientas de splash: MIT salvo los avisos específicos incluidos. `tools/package3/blz.py` conserva la licencia ISC de su implementación de origen. Las notas explican el formato de Atmosphère y no transfieren derechos sobre sus marcas.
- Asistente, herramientas nuevas y documentación original de esta colección: MIT, texto en `LICENSE`.
- Audio de ejemplo generado: creado mediante `components/bootsound/scripts/generate_example_startup.py`, incluido con el software MIT.
- Yoshi, Mario y otros personajes, imágenes y sonidos de terceros: pertenecen a sus respectivos titulares. Incluyo estos archivos para personalizar mi consola, sin afirmar que estén bajo MIT/GPL ni conceder una licencia de terceros. Debes contar con los permisos aplicables para reutilizarlos o redistribuirlos. Puedes usar tu propio material o el ejemplo generado.

La licencia del código no se extiende a los clips, personajes, marcas o imágenes de terceros. No hay afiliación con Nintendo, Atmosphère, Hekate, devkitPro o TegraExplorer.

## Dependencias portátiles descargadas

No se redistribuyen dentro de este repositorio ni se incluyen en su licencia MIT. El instalador descarga versiones fijadas y verifica su SHA-256 antes de ejecutarlas.

- FFmpeg: [fuentes y licencias](https://github.com/FFmpeg/FFmpeg), distribuido dentro de las ruedas de [imageio-ffmpeg 0.8.1](https://pypi.org/project/imageio-ffmpeg/0.8.1/). Se conserva la licencia BSD-2-Clause del empaquetador, la salida `ffmpeg -L` de la compilación concreta y sus datos de origen junto al ejecutable. Las condiciones de FFmpeg dependen de las opciones de esa compilación y no se sustituyen por las de imageio. [Proceso de obtención de binarios](https://github.com/imageio/imageio-ffmpeg/tree/main/tools).
- Compilador ARM portátil: [xPack GNU Arm Embedded GCC](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v15.2.1-1.1). Se conservan los avisos y licencias de la distribución y sus componentes.
- Python portátil: [python-build-standalone, distribución 20261110](https://github.com/astral-sh/python-build-standalone/releases/tag/20261110), con los avisos y licencias incluidos en el paquete y los de [CPython](https://github.com/python/cpython).
