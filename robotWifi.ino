#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "stdlib.h"

#define PORT 80

//Macros para el control del sensor de distancia
#define SALIDA D10
#define ENTRADA D15
#define TRIG SALIDA
#define ECHO ENTRADA
//Macros de las lineas de control de los motores (puente H)
#define  AIA  D7
#define  AIB  D6
#define  BIB  D4
#define  BIA  D5

//Macros de las velocidades
#define  STD_SPEED 600
#define  OBJ_D_SPEED 450
#define  ACCEL_SPEED 200

//Tiempo en mS que estara girando y retrocediendo cuando detecte un obstaculo y se encuentre la configuracion de cambiar de direccion
#define TURN_TIME 600
#define BACKW_TIME 600

//Declaracion de la variable que controla la velocidad de los motores
int speed;
//Declaracion de las variables para usar con el sensor de distancia
int distancia;
long tiempo;

//Parametros de Configuracion de la red Wifi a la que se va a conectar
const char* ssid = "mouse";
const char* password = "0043603905";
//const char* ssid = SSID; //(Reemplazar este valor por el SSID de la red wifi a utilizar)
//const char* password = PSW; //(Reemplazar este valor por la contraseña de la red wifi a utilizar)

//Variable para crear un servidor
WiFiServer server(PORT);

// Variable para verificar si el cliente estaba conectado previamente o no.
boolean alreadyConnected = false; 
// Variable para decidir si frenar o cambiar de direccion cuando detecta un obstaculo
boolean OPT_cambiar_direccion=false;
// Variable para controlar la lectura de distancias (para que no lea continuamente.
unsigned long puedoLeer=0;

void setup() {

  //Configuracion de sensor de distancia
  HC_SR04_setup();

  //Configuracion del puente H (motores)
  H_BRIDGE_setup();

  //Inicializo la terminal serie (para debug)
  Serial.begin(115200);
  delay(10);

  // Conexion a la red Wifi especificada
  Serial.println();
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado");

  //Levantar el servidor
  server.begin();
  Serial.println("Servidor iniciado");

  // Mostrar la direccion IP que fue asignada al robot y el puerto especificado
  Serial.print("Conectese mediante TCP a la siguiente IP: ");
  Serial.print(WiFi.localIP());
  Serial.print(" en el puerto: ");
  Serial.println(PORT);

}
//Variable para enviar y recibir datos al/desde el cliente(smartphone)
WiFiClient client;
//Variable para controlar el ultimo movimiento realizado (util para le comando "acelerar")
char last_mov='S';

void loop() {

  if (!client.connected()) {
    // Si no hay ningun cliente conectado intentar conectarse
    client = server.available();//Para mas detalle ver https://www.arduino.cc/en/Reference/ServerAvailable
    delay(100);
    if (client.connected()) {
      //Si se conecto con el cliente mostrarlo en la terminal(debug) junto con su IP
      Serial.write("Nuevo cliente: ");
      Serial.println(client.remoteIP());
    }
  } else {
    // Si el cliente SI ESTÁ conectado leer datos que envía
    if (client.available() > 0) {
      String request = client.readStringUntil('\n');
      Serial.println(request);
      //Se Evalua que comando se recibe y se ejecutan las acciones correspondientes
      //Despues del seteo de la velocidad se realiza un pequeño delay por seguridad
      if (request == "avanzar") {
        speed = STD_SPEED;
        delay(10);
        last_mov=R_forward();
        delay(10);
      } else if (request == "retroceder") {
        speed = STD_SPEED;
        delay(10);
        last_mov=R_backward();
        delay(10);
      } else if (request == "acelerar") {
        speed = speed + ACCEL_SPEED;
        if (speed>1023)
          speed=1023;
        delay(10);
        switch (last_mov) {
          case 'F':
            R_forward();
            break;
          case 'B':
            R_backward();
            break;
          case 'R':
            R_turn_right();
            break;
          case 'L':
            R_turn_left();
            break;
          default: 
          break;
        }
      } else if (request == "frenar") {
        speed = 0;
        delay(10);
        last_mov=R_stop();
      } else if (request == "derecha") {
        speed = STD_SPEED;
        delay(10);
        last_mov=R_turn_right();
      } else if (request == "izquierda") {
        speed = STD_SPEED;
        delay(10);
       last_mov= R_turn_left();
      }
      //Comandos para la respuesta frente un obstaculo
      else if (request == "CAMBIAR") {
        OPT_cambiar_direccion=true;
      }else if (request == "FRENAR") {
        OPT_cambiar_direccion=false;
      }
      else {
        last_mov=R_stop();
      }
    }
  }
  //Calcular distancia
    if (puedoLeer==0){
      //Calcula la distancia
      HC_SR04_getDistancia();
      //Envía los datos al cliente con el formato "$DISTA XXXX$" donde XXXX es el valor leido
      client.write("$DISTA ");
      client.print(distancia);
      client.write("$");
      //Si la distancia es menor a 20 y mayor a 5 [cm] ejecuta la accion configurada (por defecto es frenar "OPT_cambiar_direccion=false")
      if (distancia<20){
        if (distancia<5){
          //Si la distancia es menor a 5 cm realiza el frenado de emergencia y ejecuta un delay como filtro pasa bajos 
          //para que no lea por ese determinado tiempo en el cual el sensor puede leer erroneamente y activar el cambio de direccion
          Serial.write("Frenado de seguridad!");
          R_stop();
          delay(1200);//Delay porque las lecturas las hace muy seguidas
        }else{
          Serial.write("Objeto detectado. dist: ");
          Serial.println(distancia);
          if(OPT_cambiar_direccion){
            //Gira aleatoriamente a la derecha o izquierda por 500 milisegundos con velocidad de 100 + la velocidad standard y luego sigue avanzando con velocidad de deteccion de obstaculo
            speed = STD_SPEED+200;
            delay(10);
            R_backward();
            delay(BACKW_TIME);
            (random(0, 2)==1)?R_turn_left():R_turn_right();
            //Gira durante TURN_TIME mimlisegundos
            delay(TURN_TIME);
            speed = OBJ_D_SPEED;
            delay(10);
            R_forward();
          }else{
            R_stop();
          }
        }

      }
      //debug de la distancia medida
      Serial.write("Prueba sensor de distancia. dist: ");
      Serial.println(distancia);
      //seteo la variable puedo leer para generar un delay mientras sigue recibiendo comandos
      puedoLeer=50000;
    }else{
      puedoLeer--;
    }

}

void H_BRIDGE_setup(){
  //Todas los pines del puente H se deben configurar como salidas
  pinMode( AIA, OUTPUT );
  pinMode( AIB, OUTPUT );
  pinMode( BIA, OUTPUT );
  pinMode( BIB, OUTPUT );

}

void HC_SR04_setup() {
  pinMode(TRIG, OUTPUT); /*activación del pin TRIG como salida: para el pulso ultrasónico*/
  pinMode(ECHO, INPUT); /*activación del pin ECHO como entrada: tiempo del rebote del ultrasonido*/


}
int HC_SR04_getDistancia() {
  digitalWrite(TRIG, LOW); /* Por cuestión de estabilización del sensor*/
  delayMicroseconds(5);
  digitalWrite(TRIG, HIGH); /* envío del pulso ultrasónico*/
  delayMicroseconds(10);
  tiempo = pulseIn(ECHO, HIGH); /* Función para medir la longitud del pulso entrante. Mide el tiempo que transcurrido entre el envío
  del pulso ultrasónico y cuando el sensor recibe el rebote, es decir: desde que el pin 12 empieza a recibir el rebote, HIGH, hasta que
  deja de hacerlo, LOW, la longitud del pulso entrante*/
  distancia = long(0.017 * tiempo); /*fórmula para calcular la distancia obteniendo un valor entero [cm]*/
  return distancia;
}

//Retroceder
char R_backward()
{
  analogWrite(AIA, 0);
  analogWrite(AIB, speed);
  analogWrite(BIB, speed);
  analogWrite(BIA, 0);
  return 'B';
}

//Avanzar
char R_forward()
{
  analogWrite(AIA, speed);
  analogWrite(AIB, 0);
  analogWrite(BIA, speed);
  analogWrite(BIB, 0);
  return 'F';

}

//Girar a la derecha
char R_turn_right()
{
  analogWrite(AIA, 0);
  analogWrite(AIB, 0);
  analogWrite(BIB, 0);
  analogWrite(BIA, speed);
  return 'R';

}

//Girar a la izquierda
char R_turn_left()
{
  analogWrite(AIA, speed);
  analogWrite(AIB, 0);
  analogWrite(BIB, 0);
  analogWrite(BIA, 0);
  return 'L';

}
//Frenar, 
char R_stop()
{
  delay(10);
  analogWrite(AIA, 0);
  delay(10);
  analogWrite(AIB, 0);
  delay(10);
  analogWrite(BIB, 0);
  delay(10);
  analogWrite(BIA, 0);
  delay(10);
  return 'S';
}

