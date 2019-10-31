/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre
Description: Programme pour deplacer le ROBUS pour le defi du combattant.
Date: 25 octobre 2019
*/

/* ****************************************************************************
Inclure les librairies de functions que vous voulez utiliser
**************************************************************************** */

#include <Arduino.h>
#include <LibRobus.h> // Essentielle pour utiliser RobUS
#include <math.h>
#include <Adafruit_TCS34725.h>
#include <string.h>


/* ****************************************************************************
Variables globales et defines
**************************************************************************** */
// -> defines...
// L'ensemble des fonctions y ont acces
const float WHEEL_CIRCUMFERENCE = 23.94;
const int PULSES_PER_CYCLE = 3200;
const float BASE_SPEED = 0.8;
const String COULEUR1 = "Rouge";
const String COULEUR2 = "Bleu";

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

/* ****************************************************************************
Vos propres fonctions sont creees ici
**************************************************************************** */

///Converts a distance to the according number of pulses.
///distance : The distance in centimeters.
int DistanceToPulses(float distance)
{
    return (distance / WHEEL_CIRCUMFERENCE) * PULSES_PER_CYCLE;
}

///Function to move ROBUS in a straight line.
///speed : The base speed.
///pulses : The distance to travel in pulses. Should always be positive.
void PID(float speed, int pulses)
{
    int totalPulsesLeft = 0, totalPulsesRight = 0, 
        deltaPulsesLeft = 0, deltaPulsesRight = 0,
        errorDelta = 0, errorTotal = 0;
    const float kp = 0.001, ki = 0.001;
    
    //the number of pulses ROBUS will accelerate/decelerate
    float pulsesAcceleration = 1/10.0 * pulses;

    float masterSpeed = 0, slaveSpeed = 0, slowSpeed = 0.2;
    int speedSign = speed > 0 ? 1 : -1;
    speed *= speedSign; //we'll convert it back to the negative value (if that's the case) at the end
    ENCODER_Reset(LEFT);
    ENCODER_Reset(RIGHT);

    //we give it a slow speed to start
    MOTOR_SetSpeed(LEFT, slowSpeed);
    MOTOR_SetSpeed(RIGHT, slowSpeed);

    while(totalPulsesRight < pulses)
    {
        deltaPulsesLeft = abs(ENCODER_ReadReset(LEFT));
        deltaPulsesRight = abs(ENCODER_ReadReset(RIGHT));
        totalPulsesLeft += deltaPulsesLeft;
        totalPulsesRight += deltaPulsesRight;
        
        //change the 2 following lines if we change the master
        errorDelta = deltaPulsesRight - deltaPulsesLeft;
        errorTotal = totalPulsesRight - totalPulsesLeft;

        if(totalPulsesRight < pulsesAcceleration)
        {
            masterSpeed = slowSpeed + (totalPulsesRight / pulsesAcceleration * (speed - slowSpeed));
        }
        else if(totalPulsesRight > pulses - pulsesAcceleration)
        {
            masterSpeed = slowSpeed + ((pulses - totalPulsesRight) / pulsesAcceleration * (speed - slowSpeed));
        }
        else //not accelerating nor decelerating
        {
            masterSpeed = speed;
        }
        slaveSpeed = masterSpeed + (errorDelta * kp) + (errorTotal * ki);
        MOTOR_SetSpeed(LEFT, speedSign * slaveSpeed);
        MOTOR_SetSpeed(RIGHT, speedSign * masterSpeed);
        delay(40);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///Move ROBUS according to the distance specified.
///distance : The distance to cover in centimeters. Can be negative.
///speed : The base speed. Must be between 0.15 and 1 for it to work.
void Move(float distance, float speed)
{
    int distanceSign = distance > 0 ? 1 : -1;
    //distanceSign is used to reverse the speed if the distance is negative
    PID(distanceSign * speed, DistanceToPulses(abs(distance)));
}

///Turn ROBUS according to the angle specified.
///angle : the rotation angle in radians. Positive makes it go left,
///        while negative makes it go right.
void Turn(float angle)
{
    //only one wheel will move
    float radius = 19.33; //in centimeters
    int motor = angle > 0 ? RIGHT : LEFT;
    int totalPulses = 0;
    float pulsesRotation = DistanceToPulses(abs(radius * angle));

    ENCODER_Reset(motor);
    MOTOR_SetSpeed(motor, 0.4);
    while(totalPulses < pulsesRotation)
    {
        totalPulses = ENCODER_Read(motor);
        delay(20); //might not be necessary
    }
    MOTOR_SetSpeed(motor, 0);    
}

//----------------

void Test()
{
    //do some tests
    Serial.print("values : ");
    senseurcouleur();
    /*Serial.print(analogRead(A0)); Serial.print("\t");
    Serial.print(analogRead(A1)); Serial.print("\t");
    Serial.print(analogRead(A2)); Serial.print("\t");
    Serial.print(analogRead(A3)); Serial.print("\t");
    Serial.print(analogRead(A4)); Serial.print("\t");
    Serial.print(analogRead(A5)); Serial.print("\t");
    Serial.print(analogRead(A6)); Serial.print("\t");
    Serial.print(analogRead(A7)); Serial.print("\n");
    Serial.println();*/
    
}

/* ****************************************************************************
Fonctions d'initialisation (setup)
**************************************************************************** */
// -> Se fait appeler au debut du programme
// -> Se fait appeler seulement un fois
// -> Generalement on y initilise les varibbles globales

void setup()
{
    BoardInit();

    //color sensor setup
    Serial.println("Color View Test!");

    if (tcs.begin())
        Serial.println("Found sensor");
    else
    {
        Serial.println("No TCS34725 found ... check your connections");
        while (1); // halt!
    }
}


/* ****************************************************************************
Fonctions de boucle infini (loop())
**************************************************************************** */
// -> Se fait appeler perpetuellement suite au "setup"

void loop()
{
    if(ROBUS_IsBumper(REAR))
    {
        delay(1000);
        Test();
    }
    // SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}

















































































































































































































































































































































































































































































































































//Test function for color sensor. In test, it should print the color read in RGB, in temperature and in Lux.
void senseurcouleur()
{
    uint16_t r = 0, g = 0, b = 0, c = 0, colorTemp = 0, Lux = 0;
    tcs.setInterrupt(false); //turns on LED
    delay(60);
    tcs.getRawData(&r, &g, &b, &c);
    tcs.setInterrupt(true); //turns off LED
    Serial.print("C: "); Serial.print(c);
    Serial.print("\tR: "); Serial.print(r);
    Serial.print("\tG: "); Serial.print(g);
    Serial.print("\tB: "); Serial.print(b);
    colorTemp = tcs.calculateColorTemperature(r, g, b);
    Lux = tcs.calculateLux(r, g, b);
    Serial.print("\ncolorTemp: "); Serial.print(colorTemp);
    Serial.print("\nLux: "); Serial.print(Lux);
    Serial.print("\n");

}

//assigns color to the word defining the color 
/*String assigncolcapt()
{

}

//indicates with a boolean if the goal sensed is the good goal
bool bonbut(String couleurcapt)
{
    bool actioncoul;
    if (couleurcapt = COULEUR1)
    {
        actioncoul = true;
    }
    else
    {
        actioncoul = false;
    }
    return actioncoul;
}*/