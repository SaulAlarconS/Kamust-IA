# Kamust IA

Plugin de instrumento (VST3 y AU) que toma un one-shot (golpe, nota, sample corto) y lo reconstruye como un instrumento tocable por teclado, con controles de timbre, envolvente, filtros, glide y reverb. Puede mezclar dos samples (A y B) con un control de Morph.

Funciona en **Windows** (VST3) y **macOS** (VST3 y AU).

## Compatibilidad con DAW

| DAW | Formato | Sistema |
|---|---|---|
| Studio One | VST3 | Windows y macOS |
| Logic Pro | **AU** (Logic no carga VST3) | solo macOS |

Logic Pro sólo carga Audio Units, por eso el proyecto compila también la versión AU en macOS.

## Qué hace (y qué no)

Kamust IA reproduce la idea general de un resintetizador de samples. Tiene estas partes:

1. **Análisis**: STFT de 4096 puntos sobre la muestra (máximo 12 s, mezcla a mono). Se buscan los parciales más fuertes del espectro medio y se sigue su envolvente de amplitud en el tiempo.
2. **Detección de tono**: el parcial más grave y fuerte se toma como raíz si la serie parece armónica. Si no, el sample se reproduce a su altura original.
3. **Reproducción**: síntesis aditiva de hasta 48 parciales por muestra, con una capa de "click" (los primeros 60 ms diferenciados) para el ataque.
4. **Morph**: mezcla los parciales y el click de A y B.
5. **Controles**: Harmonics, Click, Tilt, Morph, Tune, Glide, ADSR, filtros HP/LP, Reverb y Room.
6. **Proyecto**: las muestras se guardan dentro del proyecto, así que se abre igual en otra máquina.

**Limitaciones honestas:**
- No usa un motor de IA. Resynthesizer de Samplab separa componentes armónicos y percusivos con un modelo local; aquí el análisis es DSP clásico (picos espectrales). El resultado será parecido en la idea, no igual en calidad.
- Los sonidos muy ruidosos o inarmónicos se reconstruyen peor.
- No hay separación armónico/percusivo real: la capa de ataque es una aproximación.
- No está probado en Logic Pro ni en Studio One, ni compilado en esta sesión (no hay red para descargar JUCE). Primero compila con el workflow de GitHub y pruébalo con tus propios samples.

## Compilar en GitHub (sin instalar nada)

1. Sube este repositorio a tu cuenta de GitHub.
2. Ve a la pestaña **Actions** y ejecuta **Build Kamust IA** (o haz un push).
3. Al terminar, descarga los artefactos `KamustIA-windows-latest` y `KamustIA-macos-14`.

## Compilar en local

Requisitos: CMake 3.22 o superior, Git, y en Windows Visual Studio 2022 (C++); en macOS Xcode.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

CMake descarga JUCE 7.0.12 automáticamente. Los archivos quedan en `build/Kamust_IA_artefacts/Release/`.

## Instalación con instalador (recomendado)

- **Windows**: ejecuta `KamustIA-Setup-Windows.exe`. Instala el plugin en `C:\Program Files\Common Files\VST3\` y pide permisos de administrador.
- **macOS**: ejecuta `KamustIA-macOS.pkg`. Instala el VST3 en `/Library/Audio/Plug-Ins/VST3/` y la AU en `/Library/Audio/Plug-Ins/Components/`.

Los instaladores se generan en GitHub Actions y aparecen como artefactos de cada ejecución. Como no están firmados, Windows (SmartScreen) y macOS (Gatekeeper) mostrarán una advertencia. En Windows pulsa "Más información" y "Ejecutar de todas formas". En macOS haz clic derecho sobre el `.pkg`, elige **Abrir** y confirma.

## Instalación manual


- **Windows**: copia `VST3/Kamust IA.vst3` en `C:\Program Files\Common Files\VST3\`.
- **macOS VST3**: copia `VST3/Kamust IA.vst3` en `~/Library/Audio/Plug-Ins/VST3/`.
- **macOS AU (Logic)**: copia `AU/Kamust IA.component` en `~/Library/Audio/Plug-Ins/Components/`. Si macOS bloquea el plugin por no estar firmado, ejecuta `xattr -dr com.apple.quarantine "~/Library/Audio/Plug-Ins/Components/Kamust IA.component"`.

Después reinicia el DAW y, en Logic, busca el plugin en Instrumentos > Audio Units > Kamust.

## Uso rápido

1. Arrastra un WAV/AIFF/FLAC sobre la mitad izquierda de la ventana (A) o usa **LOAD A**.
2. Toca notas en MIDI: la muestra se afina automáticamente sobre el teclado.
3. Opcional: carga una segunda muestra en B y mueve **MORPH** para mezclar ambas.
4. Ajusta **HARMONICS** (nivel de parciales) y **CLICK** (ataque) para equilibrar el sonido.

## Estructura del código

```
Source/
  ResynthModel.*   análisis de la muestra (STFT, picos, envolventes, raíz)
  ResynthVoice.*   voz de síntesis aditiva con morph, glide y ADSR
  PluginProcessor.* parámetros, carga/guardado de muestras, bucle de audio
  PluginEditor.*   interfaz
  RetroLookAndFeel.h  estilo visual (bronce, óxido, verde azulado)
```

## Aviso

Proyecto independiente, sin relación con Samplab. No usa código, samples ni recursos de Samplab. Tampoco usa la ilustración de tu referencia: solo toma una paleta de colores inspirada en ella.
