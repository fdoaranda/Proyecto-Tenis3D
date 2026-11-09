# Proyecto Final: Escenario Deportivo 3D Interactivo (Tenis)
Computacion Grafica e Interaccion Humano-Computadora, FI UNAM, semestre 2027-1.
Integrantes: Aranda Marron Fernando, Gutierrez Nolasco Emiliano, Chacon Jaral Hugo Emmanuel.

## Contenido
- main.cpp: programa principal (camaras, iluminacion, animaciones, interaccion).
- Model.h, Mesh.h, Shader.h, Camera.h: clases de las practicas.
- Shader/: lighting.vs, lighting.frag, lamp.vs, lamp.frag.
- Models/: 52 modelos obj con sus mtl y 42 texturas png.
- tools/generar_modelos.py: generador de modelos y texturas (Python 3, Pillow y numpy).
- CMakeLists.txt: compilacion alternativa en Linux.

## Compilar en Visual Studio
1. Proyecto de consola con main.cpp, Model.h, Mesh.h, Shader.h y Camera.h.
2. Vincular GLFW, GLEW, SOIL2 y Assimp como en las practicas, mas opengl32.lib.
3. El directorio de trabajo debe ser la carpeta que contiene Models y Shader.
4. Argumento --demo para ejecutar el recorrido automatico.

## Controles
1-4 camaras | WASD/flechas mover | Mouse mirar | ESPACIO saque | L reflectores | N dia/noche | P y O puntos | R reiniciar marcador | V recorrido automatico | ESC salir
