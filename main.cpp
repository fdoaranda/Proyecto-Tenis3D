/*
 * Proyecto Final - Escenario Deportivo 3D Interactivo: TENIS
 * Computacion Grafica e Interaccion Humano-Computadora - FI UNAM
 * Integrantes: Aranda Marron Fernando, Gutierrez Nolasco Emiliano, Chacon Jaral Hugo Emmanuel
 *
 * OpenGL 3.3 (GLFW, GLEW, GLM, SOIL2, Assimp). Todo lo usado aqui se vio en las practicas:
 * carga de modelos, camara, modelado jerarquico, texturizado, iluminacion (direccional, puntual y
 * reflector), transparencia, animacion basica, maquina de estados y animacion por keyframes.
 *
 * CONTROLES (tambien se imprimen en la consola al iniciar)
 *   1 Camara espectador (primera persona)   2 Camara orbital del jugador (tercera persona)
 *   3 Camara de transmision TV              4 Camara del jugador (primera persona)
 *   W A S D / flechas  mover camara o jugador        Mouse  mirar / orbitar
 *   ESPACIO  saque (animacion por keyframes)         L  encender/apagar reflectores
 *   N  dia / noche                                   P y O  punto para el jugador 1 / 2
 *   R  reiniciar marcador                            V  recorrido automatico (demo)
 *   ESC  salir
 */

#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

// GLEW
#include <GL/glew.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM Mathematics
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Carga de texturas y modelos
#include "SOIL2/SOIL2.h"

// Otros
#include "Shader.h"
#include "Camera.h"
#include "Model.h"

// ---------------------------------------------------------------------------------------------
// Prototipos
// ---------------------------------------------------------------------------------------------
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void DoMovement(float dt);
void Animation(float dt);

// ---------------------------------------------------------------------------------------------
// Ventana, camaras y teclado
// ---------------------------------------------------------------------------------------------
const GLuint WIDTH = 1280, HEIGHT = 720;
int SCREEN_WIDTH, SCREEN_HEIGHT;

enum CamMode { CAM_ESPECTADOR = 0, CAM_TERCERA = 1, CAM_TV = 2, CAM_JUGADOR = 3 };
CamMode camMode = CAM_ESPECTADOR;
const char* camNames[] = { "Espectador (1a persona)", "Orbital del jugador (3a persona)", "Transmision TV", "Jugador (1a persona)" };

Camera camera(glm::vec3(16.3f, 4.9f, 7.0f), glm::vec3(0.0f, 1.0f, 0.0f), 188.0f, -16.0f);   // camara libre del espectador
GLfloat lastX = WIDTH / 2.0, lastY = HEIGHT / 2.0;
bool keys[1024];
bool firstMouse = true;

float orbitYaw = 0.0f, orbitPitch = 22.0f, orbitDist = 7.0f;   // camara de tercera persona
float viewPitch = 0.0f;                                          // cabeceo en la camara del jugador

// ---------------------------------------------------------------------------------------------
// Iluminacion e interaccion con el entorno
// ---------------------------------------------------------------------------------------------
const int NUM_LUCES = 4;
glm::vec3 pointLightPositions[NUM_LUCES] = {
	glm::vec3(22.0f, 19.0f, 20.0f), glm::vec3(-22.0f, 19.0f, 20.0f),
	glm::vec3(22.0f, 19.0f, -20.0f), glm::vec3(-22.0f, 19.0f, -20.0f) };
glm::vec3 towerPositions[NUM_LUCES] = {
	glm::vec3(24.0f, 0.0f, 22.0f), glm::vec3(-24.0f, 0.0f, 22.0f),
	glm::vec3(24.0f, 0.0f, -22.0f), glm::vec3(-24.0f, 0.0f, -22.0f) };

bool reflectoresOn = false;     // L
float reflectoresLevel = 0.0f;  // 0..1 (transicion suave)
bool nocheOn = false;           // N
float dayFactor = 1.0f;         // 1 = dia, 0 = noche
float cheerTimer = 0.0f;        // la grada celebra un punto
float scorePop[2] = { 0.0f, 0.0f };   // pulso de escala de los digitos del marcador (0..1)

// ---------------------------------------------------------------------------------------------
// Jugadores (modelado jerarquico)
// ---------------------------------------------------------------------------------------------
// Pose articulada de un jugador. Los angulos estan en grados.
//   lean/twist: inclinacion y giro del torso     shRp/shRr/elR: hombro (cabeceo, abduccion) y codo derecho
//   shLp/shLr/elL: igual para el brazo izquierdo crouch: flexion de piernas (0..1)   jump: salto en metros
//   wrist: giro de la raqueta sobre su eje
struct Pose
{
	float lean, twist, shRp, shRr, elR, shLp, shLr, elL, crouch, jump, wrist;
};

struct Player
{
	glm::vec3 pos;
	float yaw;          // grados, el modelo mira hacia +Z cuando yaw = 0
	Pose pose;
	float hipSwing;     // oscilacion de las piernas al caminar (maquina de estados)
	bool step;          // estado de la caminata: false sube, true baja
	bool walking;
	glm::vec3 ball;     // posicion de la pelota en el marco local del jugador
};

struct PlayerModels
{
	Model *torso, *head, *arm, *forearm, *thigh, *shin, *racket;
};

const float PELVIS_H = 1.05f;   // altura de la cadera con el jugador de pie

Pose PoseIdle = { 3, 0, 25, 10, 75, 35, 0, 60, 0.05f, 0, 90 };

Player playerA;   // jugador 1 (controlado)
Player playerB;   // jugador 2 (contrincante)
float umpireHeadYaw = 0.0f;

// ---------------------------------------------------------------------------------------------
// Animacion por keyframes del saque
// ---------------------------------------------------------------------------------------------
#define MAX_FRAMES 10
typedef struct _frame
{
	Pose pose;          // pose del jugador en este keyframe
	glm::vec3 ball;     // posicion de la pelota (marco local del jugador)
	float dur;          // duracion en segundos del tramo hacia el siguiente keyframe
} FRAME;

FRAME KeyFrame[MAX_FRAMES];
int FrameIndex = MAX_FRAMES;   // keyframes definidos
bool play = false;             // reproduciendo el saque
int playIndex = 0;             // keyframe de partida del tramo actual
float tSeg = 0.0f;             // tiempo transcurrido en el tramo actual
const int RELEASE_FRAME = 2;   // antes de este keyframe la pelota va en la mano izquierda

// ---------------------------------------------------------------------------------------------
// Marcador
// ---------------------------------------------------------------------------------------------
int pts[2] = { 0, 0 };     // 0,1,2,3 = 0,15,30,40
int games[2] = { 0, 0 };
int adv = -1;              // jugador con ventaja en deuce, -1 ninguno

// ---------------------------------------------------------------------------------------------
// Recorrido automatico (demo)
// ---------------------------------------------------------------------------------------------
bool demo = false;
float demoTime = 0.0f;
int demoEvent = 0;
glm::vec3 demoWalk(0.0f);
float demoWalkT = 0.0f;
struct CamKey { float t; glm::vec3 pos; float yaw, pitch; };
std::vector<CamKey> demoPath;

// ---------------------------------------------------------------------------------------------
// Tiempo
// ---------------------------------------------------------------------------------------------
GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

// ---------------------------------------------------------------------------------------------
// Funciones auxiliares de pose y cinematica
// ---------------------------------------------------------------------------------------------
Pose LerpPose(const Pose& a, const Pose& b, float s)
{
	Pose r;
	float* ra = (float*)&r;
	const float* pa = (const float*)&a;
	const float* pb = (const float*)&b;
	for (int i = 0; i < 11; i++)
		ra[i] = pa[i] + (pb[i] - pa[i]) * s;
	return r;
}

// Descenso de la cadera cuando se flexionan las piernas, para que los pies sigan en el piso
float CrouchDrop(float crouch)
{
	float a = glm::radians(55.0f * crouch);
	float k = glm::radians(110.0f * crouch);
	return 0.45f * (1.0f - cosf(a)) + 0.42f * (1.0f - cosf(k - a));
}

float PelvisHeight(const Pose& p)
{
	return PELVIS_H - CrouchDrop(p.crouch) + p.jump;
}

glm::mat4 TorsoMatrix(const Pose& p)
{
	glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, PelvisHeight(p), 0.0f));
	m = glm::rotate(m, glm::radians(p.twist), glm::vec3(0.0f, 1.0f, 0.0f));
	m = glm::rotate(m, glm::radians(p.lean), glm::vec3(1.0f, 0.0f, 0.0f));
	return m;
}

// Cinematica directa: posicion de la mano izquierda (donde va la pelota) en el marco local del jugador
glm::vec3 LeftHand(const Pose& p)
{
	glm::mat4 m = TorsoMatrix(p);
	m = glm::translate(m, glm::vec3(0.23f, 0.52f, 0.0f));
	m = glm::rotate(m, glm::radians(p.shLr), glm::vec3(0.0f, 0.0f, 1.0f));
	m = glm::rotate(m, glm::radians(-p.shLp), glm::vec3(1.0f, 0.0f, 0.0f));
	m = glm::translate(m, glm::vec3(0.0f, -0.30f, 0.0f));
	m = glm::rotate(m, glm::radians(-p.elL), glm::vec3(1.0f, 0.0f, 0.0f));
	m = glm::translate(m, glm::vec3(0.0f, -0.29f, 0.0f));
	return glm::vec3(m * glm::vec4(0.0f, 0.075f, 0.0f, 1.0f));
}

// Cinematica directa: centro de la raqueta, para colocar ahi la pelota en el momento del golpe
glm::vec3 RacketCenter(const Pose& p)
{
	glm::mat4 m = TorsoMatrix(p);
	m = glm::translate(m, glm::vec3(-0.23f, 0.52f, 0.0f));
	m = glm::rotate(m, glm::radians(-p.shRr), glm::vec3(0.0f, 0.0f, 1.0f));
	m = glm::rotate(m, glm::radians(-p.shRp), glm::vec3(1.0f, 0.0f, 0.0f));
	m = glm::translate(m, glm::vec3(0.0f, -0.30f, 0.0f));
	m = glm::rotate(m, glm::radians(-p.elR), glm::vec3(1.0f, 0.0f, 0.0f));
	m = glm::translate(m, glm::vec3(0.0f, -0.30f, 0.0f));
	m = glm::rotate(m, glm::radians(p.wrist), glm::vec3(0.0f, 1.0f, 0.0f));
	return glm::vec3(m * glm::vec4(0.0f, -0.46f, 0.0f, 1.0f));
}

// Define los keyframes del saque
void InitServeKeyFrames()
{
	//                lean twist shRp shRr elR  shLp shLr elL  crouch jump wrist
	KeyFrame[0].pose = { 3,    0,   25,  10,  75,  35,  0,   60,  0.05f, 0.0f,  90 };    // listo
	KeyFrame[1].pose = { 8,  -15,  -20,  30,  80,  80,  5,   30,  0.20f, 0.0f,  90 };    // preparacion
	KeyFrame[2].pose = { 4,  -20,  -10,  60,  95, 165,  5,    5,  0.28f, 0.0f,  90 };    // lanzamiento de la pelota
	KeyFrame[3].pose = { -12, -25,   5,  85, 120, 170,  5,    5,  0.40f, 0.0f,  90 };    // posicion de trofeo
	KeyFrame[4].pose = { 15,  10,  170,  15,  10,  70,  0,   90,  0.00f, 0.28f, 20 };    // contacto
	KeyFrame[5].pose = { 30,  35,   50, -20,  40,  20,  0,   30,  0.20f, 0.05f, 80 };    // seguimiento
	KeyFrame[6].pose = { 18,  20,   35,  -5,  60,  28,  0,   45,  0.12f, 0.0f,  85 };
	KeyFrame[7].pose = { 8,    8,   28,   5,  70,  33,  0,   55,  0.07f, 0.0f,  90 };
	KeyFrame[8].pose = PoseIdle;
	KeyFrame[9].pose = PoseIdle;

	// duracion de cada tramo en segundos
	float dur[MAX_FRAMES] = { 0.7f, 0.5f, 0.55f, 0.32f, 0.25f, 0.45f, 0.30f, 0.45f, 0.60f, 0.0f };
	for (int i = 0; i < MAX_FRAMES; i++)
		KeyFrame[i].dur = dur[i];

	// pelota: en la mano izquierda hasta el lanzamiento (cinematica directa)
	KeyFrame[0].ball = LeftHand(KeyFrame[0].pose);
	KeyFrame[1].ball = LeftHand(KeyFrame[1].pose);
	KeyFrame[2].ball = LeftHand(KeyFrame[2].pose);
	KeyFrame[3].ball = glm::vec3(-0.05f, 3.45f, 0.30f);          // punto mas alto del lanzamiento
	KeyFrame[4].ball = RacketCenter(KeyFrame[4].pose);           // el golpe ocurre en el centro de la raqueta
	KeyFrame[5].ball = glm::vec3(-0.30f, 2.45f, 2.20f);          // sale disparada
	KeyFrame[6].ball = glm::vec3(1.80f, 1.15f, 11.40f);          // pasa sobre la red
	KeyFrame[7].ball = glm::vec3(4.00f, 0.10f, 15.90f);          // bota en el cuadro de saque
	KeyFrame[8].ball = glm::vec3(4.90f, 1.10f, 19.50f);
	KeyFrame[9].ball = glm::vec3(5.60f, 0.10f, 23.00f);
}

void StartServe()
{
	if (play)
		return;
	play = true;
	playIndex = 0;
	tSeg = 0.0f;
	playerA.walking = false;
	printf("Saque! (animacion por keyframes)\n");
}

// ---------------------------------------------------------------------------------------------
// Marcador
// ---------------------------------------------------------------------------------------------
void UpdateTitle(GLFWwindow* window)
{
	static const char* nombres[] = { "0", "15", "30", "40" };
	char a[8], b[8];
	if (pts[0] == 3 && pts[1] == 3)
	{
		snprintf(a, sizeof(a), "%s", adv == 0 ? "AD" : "40");
		snprintf(b, sizeof(b), "%s", adv == 1 ? "AD" : "40");
	}
	else
	{
		snprintf(a, sizeof(a), "%s", nombres[pts[0]]);
		snprintf(b, sizeof(b), "%s", nombres[pts[1]]);
	}
	char title[256];
	snprintf(title, sizeof(title), "Tenis 3D | Camara: %s | Juegos %d-%d | Puntos %s-%s | %s | Reflectores: %s",
		camNames[camMode], games[0], games[1], a, b, nocheOn ? "Noche" : "Dia", reflectoresOn ? "ON" : "OFF");
	glfwSetWindowTitle(window, title);
}

void AddPoint(int who)
{
	int o = 1 - who;
	scorePop[0] = scorePop[1] = 1.0f;   // los digitos del marcador crecen y vuelven a su tamano
	if (pts[who] < 3)
		pts[who]++;
	else if (pts[o] < 3)
	{
		games[who] = (games[who] + 1) % 10;
		pts[0] = pts[1] = 0;
		adv = -1;
	}
	else if (adv == who)
	{
		games[who] = (games[who] + 1) % 10;
		pts[0] = pts[1] = 0;
		adv = -1;
	}
	else if (adv == o)
		adv = -1;
	else
		adv = who;
	cheerTimer = 2.5f;
}

void ResetScore()
{
	pts[0] = pts[1] = 0;
	games[0] = games[1] = 0;
	adv = -1;
}

// ---------------------------------------------------------------------------------------------
// Dibujo de un jugador con modelado jerarquico
// ---------------------------------------------------------------------------------------------
void DrawPlayer(const PlayerModels& M, const Player& P, Shader& shader, GLint modelLoc, bool drawHead = true)
{
	const Pose& p = P.pose;
	float hipL = P.hipSwing, hipR = -P.hipSwing;
	float kneeL = glm::max(0.0f, -hipL) * 1.3f, kneeR = glm::max(0.0f, -hipR) * 1.3f;
	// la pierna de apoyo (la menos flexionada) define la altura de la cadera
	float aC = glm::radians(55.0f * p.crouch), kC = glm::radians(110.0f * p.crouch);
	auto drop = [&](float hip, float knee) {
		float a = glm::radians(hip) + aC, k = glm::radians(knee) + kC;
		return 0.45f * (1.0f - cosf(a)) + 0.42f * (1.0f - cosf(k - a));
	};
	float dL = drop(hipL, kneeL), dR = drop(hipR, kneeR);
	float pelvisY = PELVIS_H - glm::min(dL, dR) + p.jump;

	glm::mat4 base = glm::translate(glm::mat4(1.0f), P.pos + glm::vec3(0.0f, pelvisY, 0.0f));
	base = glm::rotate(base, glm::radians(P.yaw), glm::vec3(0.0f, 1.0f, 0.0f));

	// Torso: raiz de la jerarquia
	glm::mat4 torso = glm::rotate(base, glm::radians(p.twist), glm::vec3(0.0f, 1.0f, 0.0f));
	torso = glm::rotate(torso, glm::radians(p.lean), glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(torso));
	M.torso->Draw(shader);

	// Cabeza
	glm::mat4 m = glm::translate(torso, glm::vec3(0.0f, 0.58f, 0.0f));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(m));
	if (drawHead)       // en la camara en primera persona no se dibuja la cabeza del propio jugador
		M.head->Draw(shader);

	// Brazo derecho (con raqueta): hombro -> codo -> muneca
	glm::mat4 sh = glm::translate(torso, glm::vec3(-0.23f, 0.52f, 0.0f));
	sh = glm::rotate(sh, glm::radians(-p.shRr), glm::vec3(0.0f, 0.0f, 1.0f));
	sh = glm::rotate(sh, glm::radians(-p.shRp), glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sh));
	M.arm->Draw(shader);
	glm::mat4 el = glm::translate(sh, glm::vec3(0.0f, -0.30f, 0.0f));
	el = glm::rotate(el, glm::radians(-p.elR), glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(el));
	M.forearm->Draw(shader);
	glm::mat4 rk = glm::translate(el, glm::vec3(0.0f, -0.30f, 0.0f));
	rk = glm::rotate(rk, glm::radians(p.wrist), glm::vec3(0.0f, 1.0f, 0.0f));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(rk));
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUniform1i(glGetUniformLocation(shader.Program, "transparency"), 1);   // cuerdas con canal alfa
	M.racket->Draw(shader);
	glUniform1i(glGetUniformLocation(shader.Program, "transparency"), 0);
	glDisable(GL_BLEND);

	// Brazo izquierdo: hombro -> codo
	sh = glm::translate(torso, glm::vec3(0.23f, 0.52f, 0.0f));
	sh = glm::rotate(sh, glm::radians(p.shLr), glm::vec3(0.0f, 0.0f, 1.0f));
	sh = glm::rotate(sh, glm::radians(-p.shLp), glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sh));
	M.arm->Draw(shader);
	el = glm::translate(sh, glm::vec3(0.0f, -0.30f, 0.0f));
	el = glm::rotate(el, glm::radians(-p.elL), glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(el));
	M.forearm->Draw(shader);

	// Piernas: cadera -> rodilla (se cuelgan de la cintura, sin la inclinacion del torso)
	float hips[2] = { hipL, hipR }, knees[2] = { kneeL, kneeR }, sx[2] = { 0.10f, -0.10f };
	for (int i = 0; i < 2; i++)
	{
		glm::mat4 th = glm::translate(base, glm::vec3(sx[i], -0.12f, 0.0f));
		th = glm::rotate(th, glm::radians(-(hips[i]) - 55.0f * p.crouch), glm::vec3(1.0f, 0.0f, 0.0f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(th));
		M.thigh->Draw(shader);
		glm::mat4 sn = glm::translate(th, glm::vec3(0.0f, -0.45f, 0.0f));
		sn = glm::rotate(sn, glm::radians(knees[i] + 110.0f * p.crouch), glm::vec3(1.0f, 0.0f, 0.0f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sn));
		M.shin->Draw(shader);
	}
}

// ---------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------
int main(int argc, char** argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	for (int i = 1; i < argc; i++)
		if (strcmp(argv[i], "--demo") == 0)
			demo = true;

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Tenis 3D", nullptr, nullptr);
	if (nullptr == window)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return EXIT_FAILURE;
	}
	glfwMakeContextCurrent(window);
	glfwGetFramebufferSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);

	glfwSetKeyCallback(window, KeyCallback);
	glfwSetCursorPosCallback(window, MouseCallback);
	if (!demo)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glewExperimental = GL_TRUE;
	if (GLEW_OK != glewInit())
	{
		std::cout << "Failed to initialize GLEW" << std::endl;
		return EXIT_FAILURE;
	}
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	Shader lightingShader("Shader/lighting.vs", "Shader/lighting.frag");
	Shader lampShader("Shader/lamp.vs", "Shader/lamp.frag");

	// ---------------- Modelos: zona 1, terreno de juego ----------------
	Model Suelo("Models/Suelo.obj");
	Model Cancha("Models/Cancha.obj");
	Model Red("Models/Red.obj");
	Model PosteRed("Models/PosteRed.obj");
	Model Pelota("Models/Pelota.obj");
	Model AnuncioA("Models/AnuncioA.obj");
	Model AnuncioB("Models/AnuncioB.obj");
	Model AnuncioC("Models/AnuncioC.obj");
	// ---------------- Zona 2: bancas de jugadores ----------------
	Model Banca("Models/Banca.obj");
	Model Hielera("Models/Hielera.obj");
	Model Sombrilla("Models/Sombrilla.obj");
	// ---------------- Zona 3: espectadores, juez de silla, marcador, torres de luz ----------------
	Model Gradas("Models/Gradas.obj");
	Model SillaJuez("Models/SillaJuez.obj");
	Model JuezTorso("Models/Juez_Torso.obj");
	Model JuezCabeza("Models/Juez_Cabeza.obj");
	Model Marcador("Models/Marcador.obj");
	Model TorreLuz("Models/TorreLuz.obj");
	std::vector<Model*> Espectadores;
	for (int i = 0; i < 6; i++)
	{
		char path[64];
		snprintf(path, sizeof(path), "Models/Espectador%d.obj", i);
		Espectadores.push_back(new Model(path));
	}
	std::vector<Model*> DigPts, DigGames;
	const char* ptsNames[] = { "0", "15", "30", "40", "AD" };
	for (int i = 0; i < 5; i++)
	{
		char path[64];
		snprintf(path, sizeof(path), "Models/Pt_%s.obj", ptsNames[i]);
		DigPts.push_back(new Model(path));
	}
	for (int i = 0; i < 10; i++)
	{
		char path[64];
		snprintf(path, sizeof(path), "Models/G_%d.obj", i);
		DigGames.push_back(new Model(path));
	}
	// ---------------- Jugadores (partes del modelo jerarquico) ----------------
	PlayerModels MA = { new Model("Models/JugA_Torso.obj"), new Model("Models/JugA_Cabeza.obj"), new Model("Models/JugA_Brazo.obj"),
		new Model("Models/JugA_Antebrazo.obj"), new Model("Models/JugA_Muslo.obj"), new Model("Models/JugA_Pierna.obj"), new Model("Models/JugA_Raqueta.obj") };
	PlayerModels MB = { new Model("Models/JugB_Torso.obj"), new Model("Models/JugB_Cabeza.obj"), new Model("Models/JugB_Brazo.obj"),
		new Model("Models/JugB_Antebrazo.obj"), new Model("Models/JugB_Muslo.obj"), new Model("Models/JugB_Pierna.obj"), new Model("Models/JugB_Raqueta.obj") };

	// ---------------- Cubo de las lamparas (como en la practica de iluminacion) ----------------
	float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,   0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,   0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,   0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,   0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,   0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,   0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,   0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,   0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,   0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,   0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f };
	GLuint VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	// ---------------- Espectadores: posiciones en las gradas ----------------
	struct Spectator { glm::vec3 pos; float yaw; int model; float phase; };
	std::vector<Spectator> crowd;
	{
		srand(12345);
		const int rows = 10;
		const float depth = 0.85f, rise = 0.45f, x0 = 11.0f;
		for (int side = 0; side < 2; side++)
			for (int r = 0; r < rows; r++)
				for (float z = -17.4f; z <= 17.4f; z += 0.9f)
				{
					if (rand() % 100 > 62)
						continue;
					if (fabsf(fabsf(z) - 7.0f) < 0.8f)      // pasillos de las gradas en z = +-7
						continue;
					float x = x0 + r * depth + depth * 0.45f;
					Spectator s;
					s.pos = glm::vec3(side == 0 ? x : -x, (r + 1) * rise - 0.05f, z);
					s.yaw = side == 0 ? -90.0f : 90.0f;
					s.model = rand() % 6;
					s.phase = (rand() % 628) / 100.0f;
					crowd.push_back(s);
				}
	}

	// ---------------- Estado inicial de los jugadores y los keyframes ----------------
	playerA.pos = glm::vec3(2.0f, 0.0f, 11.4f);
	playerA.yaw = 180.0f;
	playerA.pose = PoseIdle;
	playerA.hipSwing = 0.0f;
	playerA.step = false;
	playerA.walking = false;
	playerB.pos = glm::vec3(-1.0f, 0.0f, -11.5f);
	playerB.yaw = 0.0f;
	playerB.pose = PoseIdle;
	playerB.pose.crouch = 0.15f;
	playerB.hipSwing = 0.0f;
	playerB.step = false;
	playerB.walking = false;
	InitServeKeyFrames();
	playerA.ball = LeftHand(playerA.pose);

	// camino de la camara libre en el recorrido automatico (primera persona del espectador)
	demoPath.push_back({ 0.0f, glm::vec3(16.3f, 4.9f, 7.0f), 188.0f, -16.0f });
	demoPath.push_back({ 5.0f, glm::vec3(16.3f, 4.9f, -7.0f), 172.0f, -16.0f });
	demoPath.push_back({ 9.0f, glm::vec3(3.0f, 1.7f, 2.0f), 195.0f, -4.0f });
	demoPath.push_back({ 13.0f, glm::vec3(-3.5f, 1.7f, 3.0f), 200.0f, -3.0f });
	demoPath.push_back({ 18.0f, glm::vec3(7.0f, 2.6f, 27.0f), 258.0f, -9.0f });

	glm::mat4 projection;

	printf("=== Tenis 3D ===\n1-4 camaras | WASD mover | Mouse mirar | ESPACIO saque | L reflectores | N dia/noche | P/O puntos | R reiniciar | V demo | ESC salir\n");
	UpdateTitle(window);

	// ================================ Ciclo principal ================================
	while (!glfwWindowShouldClose(window))
	{
		GLfloat currentFrame = (GLfloat)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		if (deltaTime > 0.1f)
			deltaTime = 0.1f;

		glfwPollEvents();
		DoMovement(deltaTime);
		Animation(deltaTime);

		// ---- Cielo segun la hora del dia ----
		glm::vec3 skyDay(0.55f, 0.76f, 0.95f), skyNight(0.015f, 0.025f, 0.07f);
		glm::vec3 sky = glm::mix(skyNight, skyDay, dayFactor);
		glClearColor(sky.r, sky.g, sky.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);

		// ---- Camara activa: matriz de vista y proyeccion ----
		glm::mat4 view;
		glm::vec3 viewPos;
		float fov = camera.GetZoom();
		glm::vec3 ballWorld = glm::vec3(glm::translate(glm::mat4(1.0f), playerA.pos) *
			glm::rotate(glm::mat4(1.0f), glm::radians(playerA.yaw), glm::vec3(0, 1, 0)) * glm::vec4(playerA.ball, 1.0f));
		glm::vec3 spotDir(0.0f, -1.0f, 0.0f);
		if (camMode == CAM_ESPECTADOR)
		{
			view = camera.GetViewMatrix();
			viewPos = camera.GetPosition();
		}
		else if (camMode == CAM_TERCERA)
		{
			float py = glm::radians(orbitPitch), yw = glm::radians(orbitYaw);
			glm::vec3 target = playerA.pos + glm::vec3(0.0f, 1.2f, 0.0f);
			viewPos = target + glm::vec3(orbitDist * cosf(py) * sinf(yw), orbitDist * sinf(py), orbitDist * cosf(py) * cosf(yw));
			view = glm::lookAt(viewPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
		}
		else if (camMode == CAM_TV)
		{
			viewPos = glm::vec3(0.0f, 11.0f, 31.0f);
			glm::vec3 mid = (playerA.pos + playerB.pos) * 0.5f;
			glm::vec3 target(mid.x * 0.5f + ballWorld.x * 0.2f, 1.0f, mid.z * 0.25f);
			viewPos.x += mid.x * 0.4f;
			view = glm::lookAt(viewPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
			fov = glm::radians(38.0f);
		}
		else
		{
			float yw = glm::radians(playerA.yaw), pt = glm::radians(viewPitch);
			glm::mat4 torso = TorsoMatrix(playerA.pose);
			viewPos = playerA.pos + glm::vec3(0.0f, PelvisHeight(playerA.pose) + 0.76f, 0.0f) + glm::vec3(sinf(yw), 0.0f, cosf(yw)) * 0.16f;
			glm::vec3 front(cosf(pt) * sinf(yw), sinf(pt), cosf(pt) * cosf(yw));
			view = glm::lookAt(viewPos, viewPos + front, glm::vec3(0.0f, 1.0f, 0.0f));
			(void)torso;
		}
		projection = glm::perspective(fov, (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT, 0.1f, 300.0f);

		// ---- Iluminacion ----
		lightingShader.Use();
		glUniform3f(glGetUniformLocation(lightingShader.Program, "viewPos"), viewPos.x, viewPos.y, viewPos.z);

		// Luz direccional: sol de dia, luna de noche
		glm::vec3 sunDir = glm::normalize(glm::mix(glm::vec3(0.25f, -1.0f, 0.2f), glm::vec3(-0.35f, -1.0f, -0.45f), dayFactor));
		glm::vec3 dAmb = glm::mix(glm::vec3(0.07f, 0.08f, 0.16f), glm::vec3(0.55f, 0.55f, 0.55f), dayFactor);
		glm::vec3 dDif = glm::mix(glm::vec3(0.10f, 0.12f, 0.25f), glm::vec3(0.65f, 0.63f, 0.58f), dayFactor);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.direction"), sunDir.x, sunDir.y, sunDir.z);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.ambient"), dAmb.r, dAmb.g, dAmb.b);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.diffuse"), dDif.r, dDif.g, dDif.b);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.specular"), 0.15f, 0.15f, 0.15f);

		// Reflectores de las torres: cuatro luces puntuales
		glm::vec3 lampCol = glm::vec3(1.0f, 0.95f, 0.82f) * reflectoresLevel;
		for (int i = 0; i < NUM_LUCES; i++)
		{
			char name[64];
			snprintf(name, sizeof(name), "pointLights[%d].position", i);
			glUniform3f(glGetUniformLocation(lightingShader.Program, name), pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);
			snprintf(name, sizeof(name), "pointLights[%d].ambient", i);
			glUniform3f(glGetUniformLocation(lightingShader.Program, name), lampCol.r * 0.06f, lampCol.g * 0.06f, lampCol.b * 0.06f);
			snprintf(name, sizeof(name), "pointLights[%d].diffuse", i);
			glUniform3f(glGetUniformLocation(lightingShader.Program, name), lampCol.r * 0.55f, lampCol.g * 0.55f, lampCol.b * 0.55f);
			snprintf(name, sizeof(name), "pointLights[%d].specular", i);
			glUniform3f(glGetUniformLocation(lightingShader.Program, name), lampCol.r * 0.2f, lampCol.g * 0.2f, lampCol.b * 0.2f);
			snprintf(name, sizeof(name), "pointLights[%d].constant", i);
			glUniform1f(glGetUniformLocation(lightingShader.Program, name), 1.0f);
			snprintf(name, sizeof(name), "pointLights[%d].linear", i);
			glUniform1f(glGetUniformLocation(lightingShader.Program, name), 0.014f);
			snprintf(name, sizeof(name), "pointLights[%d].quadratic", i);
			glUniform1f(glGetUniformLocation(lightingShader.Program, name), 0.0007f);
		}

		// Reflector central sobre la cancha
		glUniform3f(glGetUniformLocation(lightingShader.Program, "spotLight.position"), 0.0f, 24.0f, 0.0f);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "spotLight.direction"), spotDir.x, spotDir.y, spotDir.z);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "spotLight.ambient"), 0.0f, 0.0f, 0.0f);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "spotLight.diffuse"), lampCol.r * 0.50f, lampCol.g * 0.50f, lampCol.b * 0.50f);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "spotLight.specular"), 0.0f, 0.0f, 0.0f);
		glUniform1f(glGetUniformLocation(lightingShader.Program, "spotLight.constant"), 1.0f);
		glUniform1f(glGetUniformLocation(lightingShader.Program, "spotLight.linear"), 0.012f);
		glUniform1f(glGetUniformLocation(lightingShader.Program, "spotLight.quadratic"), 0.0006f);
		glUniform1f(glGetUniformLocation(lightingShader.Program, "spotLight.cutOff"), glm::cos(glm::radians(34.0f)));
		glUniform1f(glGetUniformLocation(lightingShader.Program, "spotLight.outerCutOff"), glm::cos(glm::radians(50.0f)));

		glUniform1f(glGetUniformLocation(lightingShader.Program, "material.shininess"), 8.0f);
		glUniform1i(glGetUniformLocation(lightingShader.Program, "transparency"), 0);

		GLint modelLoc = glGetUniformLocation(lightingShader.Program, "model");
		GLint viewLoc = glGetUniformLocation(lightingShader.Program, "view");
		GLint projLoc = glGetUniformLocation(lightingShader.Program, "projection");
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

		auto draw = [&](Model& m, const glm::mat4& mat) {
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mat));
			m.Draw(lightingShader);
		};
		const glm::vec3 Y(0.0f, 1.0f, 0.0f);
		float t = (float)glfwGetTime();
		glm::mat4 I(1.0f);

		// ===================== Zona 1: terreno de juego =====================
		draw(Suelo, I);
		draw(Cancha, I);
		glEnable(GL_BLEND);                                   // red con canal alfa, como la esfera de la practica
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUniform1i(glGetUniformLocation(lightingShader.Program, "transparency"), 1);
		draw(Red, I);
		glUniform1i(glGetUniformLocation(lightingShader.Program, "transparency"), 0);
		glDisable(GL_BLEND);
		draw(PosteRed, glm::translate(I, glm::vec3(-6.4f, 0.0f, 0.0f)));
		draw(PosteRed, glm::translate(I, glm::vec3(6.4f, 0.0f, 0.0f)));

		// Anuncios perimetrales (4 m cada uno)
		Model* ads[3] = { &AnuncioA, &AnuncioB, &AnuncioC };
		for (int i = 0; i < 9; i++)
		{
			float z = -16.0f + i * 4.0f;
			draw(*ads[i % 3], glm::rotate(glm::translate(I, glm::vec3(9.9f, 0.0f, z)), glm::radians(-90.0f), Y));
			draw(*ads[(i + 1) % 3], glm::rotate(glm::translate(I, glm::vec3(-9.9f, 0.0f, z)), glm::radians(90.0f), Y));
		}
		for (int i = 0; i < 5; i++)
		{
			float x = -7.84f + i * 3.92f;
			glm::mat4 sc = glm::scale(I, glm::vec3(0.98f, 1.0f, 1.0f));
			draw(*ads[(i + 2) % 3], glm::scale(glm::translate(I, glm::vec3(x, 0.0f, -18.9f)), glm::vec3(0.98f, 1.0f, 1.0f)));
			draw(*ads[i % 3], glm::scale(glm::rotate(glm::translate(I, glm::vec3(x, 0.0f, 18.9f)), glm::radians(180.0f), Y), glm::vec3(0.98f, 1.0f, 1.0f)));
			(void)sc;
		}

		// Jugadores
		DrawPlayer(MA, playerA, lightingShader, modelLoc, camMode != CAM_JUGADOR);
		DrawPlayer(MB, playerB, lightingShader, modelLoc);

		// Pelota (marco local del jugador 1)
		draw(Pelota, glm::translate(I, ballWorld));

		// ===================== Zona 2: bancas de los jugadores =====================
		for (int i = 0; i < 2; i++)
		{
			float z = i == 0 ? -5.5f : 5.5f;
			glm::mat4 b = glm::rotate(glm::translate(I, glm::vec3(-8.6f, 0.0f, z)), glm::radians(90.0f), Y);
			draw(Banca, b);
			draw(Hielera, glm::translate(I, glm::vec3(-8.2f, 0.0f, z + 1.6f)));
			draw(Sombrilla, glm::translate(I, glm::vec3(-8.9f, 0.0f, z - 1.4f)));
		}

		// ===================== Zona 3: espectadores y juez de silla =====================
		draw(Gradas, I);
		draw(Gradas, glm::rotate(I, glm::radians(180.0f), Y));
		draw(SillaJuez, glm::translate(I, glm::vec3(7.3f, 0.0f, 0.0f)));
		{
			// el juez de silla esta sentado y su cabeza (jerarquia) sigue la pelota
			glm::mat4 torso = glm::rotate(glm::translate(I, glm::vec3(7.3f, 2.55f, 0.0f)), glm::radians(-90.0f), Y);
			draw(JuezTorso, torso);
			draw(JuezCabeza, glm::rotate(glm::translate(torso, glm::vec3(0.0f, 0.58f, 0.0f)), glm::radians(umpireHeadYaw), Y));
		}
		for (size_t i = 0; i < crowd.size(); i++)
		{
			float bob = 0.02f * sinf(t * 2.0f + crowd[i].phase);
			if (cheerTimer > 0.0f)
				bob += 0.20f * fabsf(sinf(t * 7.0f + crowd[i].phase)) * glm::min(1.0f, cheerTimer);
			glm::mat4 m = glm::translate(I, crowd[i].pos + glm::vec3(0.0f, bob, 0.0f));
			m = glm::rotate(m, glm::radians(crowd[i].yaw), Y);
			draw(*Espectadores[crowd[i].model], m);
		}

		// Marcador: tablero y digitos
		glm::mat4 board = glm::translate(I, glm::vec3(0.0f, 0.0f, -21.5f));
		draw(Marcador, board);
		for (int p = 0; p < 2; p++)
		{
			float y = p == 0 ? 4.62f : 3.70f;
			int shown = pts[p];
			if (pts[0] == 3 && pts[1] == 3)
				shown = (adv == p) ? 4 : 3;
			glm::mat4 g = glm::scale(glm::translate(board, glm::vec3(1.45f, y, 0.17f)), glm::vec3(0.8f * (1.0f + 0.35f * scorePop[p])));
			draw(*DigGames[games[p]], g);
			glm::mat4 q = glm::scale(glm::translate(board, glm::vec3(3.0f, y, 0.17f)), glm::vec3(0.8f * (1.0f + 0.35f * scorePop[p])));
			draw(*DigPts[shown], q);
		}

		// Torres de luz
		for (int i = 0; i < NUM_LUCES; i++)
		{
			float yaw = atan2f(-towerPositions[i].x, -towerPositions[i].z);
			draw(TorreLuz, glm::rotate(glm::translate(I, towerPositions[i]), yaw, Y));
		}

		// ---- Lamparas emisivas (cubos con el shader lamp) ----
		lampShader.Use();
		GLint lmLoc = glGetUniformLocation(lampShader.Program, "model");
		glUniformMatrix4fv(glGetUniformLocation(lampShader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(lampShader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		glm::vec3 lampDraw = glm::mix(glm::vec3(0.18f, 0.18f, 0.2f), glm::vec3(1.0f, 0.96f, 0.85f), reflectoresLevel);
		glUniform3f(glGetUniformLocation(lampShader.Program, "lampColor"), lampDraw.r, lampDraw.g, lampDraw.b);
		glBindVertexArray(VAO);
		for (int i = 0; i < NUM_LUCES; i++)
		{
			float yaw = atan2f(-towerPositions[i].x, -towerPositions[i].z);
			for (int cx = -1; cx <= 1; cx++)
				for (int cy = 0; cy < 2; cy++)
				{
					glm::mat4 m = glm::translate(I, towerPositions[i]);
					m = glm::rotate(m, yaw, Y);
					m = glm::translate(m, glm::vec3(cx * 1.3f, 19.0f + cy * 0.8f, 0.3f));
					m = glm::scale(m, glm::vec3(0.9f, 0.6f, 0.2f));
					glUniformMatrix4fv(lmLoc, 1, GL_FALSE, glm::value_ptr(m));
					glDrawArrays(GL_TRIANGLES, 0, 36);
				}
		}
		glBindVertexArray(0);

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}

// ---------------------------------------------------------------------------------------------
// Movimiento: camaras y jugador
// ---------------------------------------------------------------------------------------------
void MovePlayer(float dt, const glm::vec3& dir)
{
	if (play)
	{
		playerA.walking = false;
		return;
	}
	if (glm::length(dir) < 0.01f)
	{
		playerA.walking = false;
		return;
	}
	glm::vec3 d = glm::normalize(dir);
	playerA.pos += d * 3.6f * dt;
	// limites: no cruza la red ni sale de la zona de juego
	playerA.pos.x = glm::clamp(playerA.pos.x, -8.8f, 8.8f);
	playerA.pos.z = glm::clamp(playerA.pos.z, 0.9f, 17.8f);
	if (camMode == CAM_TERCERA)
		playerA.yaw = glm::degrees(atan2f(d.x, d.z));
	playerA.walking = true;
}

void DoMovement(float dt)
{
	bool fwd = keys[GLFW_KEY_W] || keys[GLFW_KEY_UP];
	bool back = keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN];
	bool left = keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT];
	bool right = keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT];

	if (camMode == CAM_ESPECTADOR)
	{
		if (fwd) camera.ProcessKeyboard(FORWARD, dt);
		if (back) camera.ProcessKeyboard(BACKWARD, dt);
		if (left) camera.ProcessKeyboard(LEFT, dt);
		if (right) camera.ProcessKeyboard(RIGHT, dt);
		playerA.walking = demoWalkT > 0.0f;
	}
	else if (camMode == CAM_TV)
	{
		playerA.walking = false;
	}
	else
	{
		glm::vec3 f, r;
		if (camMode == CAM_TERCERA)
		{
			float yw = glm::radians(orbitYaw);
			f = glm::vec3(-sinf(yw), 0.0f, -cosf(yw));      // de la camara hacia el jugador
			if (left && !right && !fwd && !back) {}
			// las flechas izquierda/derecha giran la camara orbital
			if (keys[GLFW_KEY_LEFT]) orbitYaw -= 90.0f * dt;
			if (keys[GLFW_KEY_RIGHT]) orbitYaw += 90.0f * dt;
			left = keys[GLFW_KEY_A];
			right = keys[GLFW_KEY_D];
		}
		else
		{
			float yw = glm::radians(playerA.yaw);
			f = glm::vec3(sinf(yw), 0.0f, cosf(yw));
		}
		r = glm::vec3(-f.z, 0.0f, f.x);
		glm::vec3 dir(0.0f);
		if (fwd) dir += f;
		if (back) dir -= f;
		if (right) dir += r;
		if (left) dir -= r;
		if (demoWalkT > 0.0f)
			dir = demoWalk;
		MovePlayer(dt, dir);
	}
	if (demoWalkT > 0.0f && camMode == CAM_ESPECTADOR)
		MovePlayer(dt, demoWalk);
	if (demoWalkT > 0.0f)
		demoWalkT -= dt;
}

// ---------------------------------------------------------------------------------------------
// Animaciones: transiciones de luz, caminata (maquina de estados), keyframes y juez
// ---------------------------------------------------------------------------------------------
void DemoUpdate(float dt, GLFWwindow* window);

void Animation(float dt)
{
	// Transicion suave dia/noche y de los reflectores
	float target = nocheOn ? 0.0f : 1.0f;
	if (dayFactor < target) dayFactor = glm::min(target, dayFactor + 0.4f * dt);
	if (dayFactor > target) dayFactor = glm::max(target, dayFactor - 0.4f * dt);
	float rt = reflectoresOn ? 1.0f : 0.0f;
	if (reflectoresLevel < rt) reflectoresLevel = glm::min(rt, reflectoresLevel + 1.2f * dt);
	if (reflectoresLevel > rt) reflectoresLevel = glm::max(rt, reflectoresLevel - 1.2f * dt);
	if (cheerTimer > 0.0f) cheerTimer -= dt;
	for (int i = 0; i < 2; i++)
		if (scorePop[i] > 0.0f) scorePop[i] = glm::max(0.0f, scorePop[i] - 1.6f * dt);

	if (demo)
		DemoUpdate(dt, glfwGetCurrentContext());

	// ---- Caminata: maquina de estados (como en la practica) ----
	if (playerA.walking)
	{
		if (playerA.step == false)
		{
			playerA.hipSwing += 130.0f * dt;
			if (playerA.hipSwing > 25.0f) playerA.step = true;
		}
		else
		{
			playerA.hipSwing -= 130.0f * dt;
			if (playerA.hipSwing < -25.0f) playerA.step = false;
		}
	}
	else
	{
		playerA.hipSwing *= (1.0f - glm::min(1.0f, 8.0f * dt));
	}

	// ---- Jugador 2: se desplaza lateralmente sobre la linea de fondo ----
	{
		float tt = (float)glfwGetTime();
		float nx = -1.0f + 3.0f * sinf(tt * 0.5f);
		float vx = 3.0f * 0.5f * cosf(tt * 0.5f);
		playerB.pos.x = nx;
		playerB.walking = fabsf(vx) > 0.6f;
		if (playerB.step == false) { playerB.hipSwing += 70.0f * dt; if (playerB.hipSwing > 14.0f) playerB.step = true; }
		else { playerB.hipSwing -= 70.0f * dt; if (playerB.hipSwing < -14.0f) playerB.step = false; }
		playerB.pose.twist = 6.0f * sinf(tt * 1.3f);
		playerB.pose.crouch = 0.15f + 0.03f * sinf(tt * 2.1f);
	}

	// ---- Saque: animacion por keyframes con interpolacion lineal ----
	if (play)
	{
		tSeg += dt;
		while (play && tSeg >= KeyFrame[playIndex].dur)
		{
			tSeg -= KeyFrame[playIndex].dur;
			playIndex++;
			if (playIndex >= FrameIndex - 1)
			{
				play = false;
				playIndex = 0;
				tSeg = 0.0f;
				printf("termina anim\n");
			}
		}
		if (play)
		{
			float s = tSeg / KeyFrame[playIndex].dur;
			const FRAME& a = KeyFrame[playIndex];
			const FRAME& b = KeyFrame[playIndex + 1];
			playerA.pose = LerpPose(a.pose, b.pose, s);
			if (playIndex < RELEASE_FRAME)
				playerA.ball = LeftHand(playerA.pose);               // la pelota va en la mano
			else
				playerA.ball = a.ball + (b.ball - a.ball) * s;       // trayectoria por keyframes
		}
	}
	if (!play)
	{
		// en reposo el brazo derecho sostiene la raqueta y el izquierdo la pelota
		playerA.pose = PoseIdle;
		playerA.pose.crouch = 0.05f;
		playerA.ball = LeftHand(playerA.pose);
	}

	// ---- Juez de silla: la cabeza gira siguiendo la pelota ----
	glm::vec3 ballW = glm::vec3(glm::translate(glm::mat4(1.0f), playerA.pos) *
		glm::rotate(glm::mat4(1.0f), glm::radians(playerA.yaw), glm::vec3(0, 1, 0)) * glm::vec4(playerA.ball, 1.0f));
	// el juez mira hacia -X; el angulo se mide respecto a esa direccion
	float ang = glm::degrees(atan2f(-(ballW.z - 0.0f), (7.3f - ballW.x)));
	umpireHeadYaw = glm::clamp(ang, -75.0f, 75.0f);
}

// ---------------------------------------------------------------------------------------------
// Recorrido automatico (para el video demostrativo)
// ---------------------------------------------------------------------------------------------
struct DemoEventDef { float t; int action; };
// 1..4 camara | 5 coloca al jugador en la linea de fondo y activa su camara | 10 saque | 11 reflectores | 12 noche | 13 punto J1 | 14 punto J2 | 15 camina hacia la red | 16 camina a la derecha
// 20 fin
DemoEventDef demoEvents[] = {
	{ 18.0f, 3 }, { 19.0f, 10 }, { 28.0f, 12 }, { 29.5f, 11 }, { 34.0f, 13 },
	{ 35.5f, 2 }, { 36.0f, 15 }, { 39.5f, 16 }, { 43.0f, 10 }, { 50.0f, 5 }, { 51.0f, 10 },
	{ 58.0f, 14 }, { 59.5f, 3 }, { 64.0f, 20 } };

void DemoUpdate(float dt, GLFWwindow* window)
{
	demoTime += dt;
	if (camMode == CAM_ESPECTADOR && demoTime < demoPath.back().t)
	{
		size_t i = 0;
		while (i + 1 < demoPath.size() && demoTime > demoPath[i + 1].t) i++;
		const CamKey& a = demoPath[i];
		const CamKey& b = demoPath[glm::min(i + 1, demoPath.size() - 1)];
		float s = (b.t > a.t) ? (demoTime - a.t) / (b.t - a.t) : 0.0f;
		s = glm::clamp(s, 0.0f, 1.0f);
		camera = Camera(a.pos + (b.pos - a.pos) * s, glm::vec3(0, 1, 0), a.yaw + (b.yaw - a.yaw) * s, a.pitch + (b.pitch - a.pitch) * s);
	}
	int n = (int)(sizeof(demoEvents) / sizeof(demoEvents[0]));
	while (demoEvent < n && demoTime >= demoEvents[demoEvent].t)
	{
		switch (demoEvents[demoEvent].action)
		{
		case 1: camMode = CAM_ESPECTADOR; break;
		case 2: camMode = CAM_TERCERA; orbitYaw = 0.0f; break;
		case 3: camMode = CAM_TV; break;
		case 4: camMode = CAM_JUGADOR; playerA.yaw = 180.0f; viewPitch = -4.0f; break;
		case 10: StartServe(); break;
		case 11: reflectoresOn = !reflectoresOn; break;
		case 12: nocheOn = !nocheOn; break;
		case 13: AddPoint(0); break;
		case 14: AddPoint(1); AddPoint(0); break;
		case 15: demoWalk = glm::vec3(0.0f, 0.0f, -1.0f); demoWalkT = 1.6f; break;
		case 16: demoWalk = glm::vec3(1.0f, 0.0f, 0.0f); demoWalkT = 1.6f; break;
		case 5: playerA.pos = glm::vec3(2.0f, 0.0f, 11.4f); playerA.yaw = 180.0f; camMode = CAM_JUGADOR; viewPitch = -4.0f; break;
		case 20: glfwSetWindowShouldClose(window, GL_TRUE); break;
		}
		UpdateTitle(window);
		demoEvent++;
	}
}

// ---------------------------------------------------------------------------------------------
// Teclado y mouse
// ---------------------------------------------------------------------------------------------
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (GLFW_KEY_ESCAPE == key && GLFW_PRESS == action)
		glfwSetWindowShouldClose(window, GL_TRUE);

	if (key >= 0 && key < 1024)
	{
		if (action == GLFW_PRESS)
			keys[key] = true;
		else if (action == GLFW_RELEASE)
			keys[key] = false;
	}
	if (action != GLFW_PRESS)
		return;

	switch (key)
	{
	case GLFW_KEY_1: camMode = CAM_ESPECTADOR; break;
	case GLFW_KEY_2: camMode = CAM_TERCERA; break;
	case GLFW_KEY_3: camMode = CAM_TV; break;
	case GLFW_KEY_4: camMode = CAM_JUGADOR; viewPitch = -4.0f; break;
	case GLFW_KEY_SPACE: StartServe(); break;
	case GLFW_KEY_L: reflectoresOn = !reflectoresOn; break;
	case GLFW_KEY_N: nocheOn = !nocheOn; break;
	case GLFW_KEY_P: AddPoint(0); break;
	case GLFW_KEY_O: AddPoint(1); break;
	case GLFW_KEY_R: ResetScore(); break;
	case GLFW_KEY_V: demo = !demo; demoTime = 0.0f; demoEvent = 0; break;
	}
	UpdateTitle(window);
}

void MouseCallback(GLFWwindow* window, double xPos, double yPos)
{
	if (firstMouse)
	{
		lastX = (GLfloat)xPos;
		lastY = (GLfloat)yPos;
		firstMouse = false;
	}
	GLfloat xOffset = (GLfloat)xPos - lastX;
	GLfloat yOffset = lastY - (GLfloat)yPos;
	lastX = (GLfloat)xPos;
	lastY = (GLfloat)yPos;

	if (camMode == CAM_ESPECTADOR)
		camera.ProcessMouseMovement(xOffset, yOffset);
	else if (camMode == CAM_TERCERA)
	{
		orbitYaw -= xOffset * 0.25f;
		orbitPitch = glm::clamp(orbitPitch - yOffset * 0.2f, 3.0f, 70.0f);
	}
	else if (camMode == CAM_JUGADOR)
	{
		if (!play)
			playerA.yaw -= xOffset * 0.25f;
		viewPitch = glm::clamp(viewPitch + yOffset * 0.2f, -60.0f, 60.0f);
	}
}
