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
#include <SharpIR.h>


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
SharpIR sharp(0, 1080);

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
    //both wheels will move
    int totalPulsesLeft = 0, totalPulsesRight = 0,
        deltaPulsesLeft = 0, deltaPulsesRight = 0,
        errorDelta = 0, errorTotal = 0;
    
    float radius = 19.33 / 2; //in centimeters
    float baseSpeed = 0.2, correctedSpeed = 0;
    const float kp = 0.0001, ki = 0.0005;

    int motorLeftSign = angle > 0 ? -1 : 1;
    float pulsesRotation = DistanceToPulses(fabs(radius * angle));
    
    ENCODER_Reset(LEFT);
    ENCODER_Reset(RIGHT);
    MOTOR_SetSpeed(LEFT, motorLeftSign * baseSpeed);
    MOTOR_SetSpeed(RIGHT, -1 * motorLeftSign * baseSpeed);
    while(totalPulsesLeft < pulsesRotation)
    {
        //deltaPulsesLeft = abs(ENCODER_ReadReset(LEFT));
        //deltaPulsesRight = abs(ENCODER_ReadReset(RIGHT));
        totalPulsesLeft = abs(ENCODER_Read(LEFT)); //+= deltaPulsesLeft;
        totalPulsesRight = abs(ENCODER_Read(RIGHT)); //+= deltaPulsesRight;

        //errorDelta = deltaPulsesRight - deltaPulsesLeft;
        errorTotal = totalPulsesRight - totalPulsesLeft;

        correctedSpeed = baseSpeed /*+ (errorDelta * kp)*/ + (errorTotal * ki);

        MOTOR_SetSpeed(LEFT, motorLeftSign * correctedSpeed);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

//----------------

void Test()
{
    //do some tests
    //Serial.print("values : ");
    //IRBall();
    AlignAlec();
    /*int dist = sharp.distance();
    Serial.print(dist);
    Serial.print("\n");*/
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
    /*
    if (tcs.begin())
        Serial.println("Found sensor");
    else
    {
        Serial.println("No TCS34725 found ... check your connections");
    }*/
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















































































































































































































































































































































































































































































































































/* NOT USED AT THE MOMENT
//assigns color to the word defining the color 
String AssignWordColor(uint16_t tabrgb[3])
{
    String couleurCapt;
    if((tabrgb[1] > tabrgb[2]) && (tabrgb[1] > tabrgb[3]))
    {
        couleurCapt = "Rouge";
    }
    else if((tabrgb[2] > tabrgb[1]) && (tabrgb[2] > tabrgb[3]))
    {
        couleurCapt = "Vert";
    }
    else if((tabrgb[3] > tabrgb[1]) && (tabrgb[3] > tabrgb[2]))
    {
        couleurCapt = "Bleu";
    }
    else
    {
        couleurCapt = "Jaune";
    }
    return couleurCapt;
}

//Reads color from sensor and returns it in RGB values.
String ReadSensorColor()
{
    uint16_t r = 0, g = 0, b = 0, c = 0, colorTemp = 0, Lux = 0;
    uint16_t tabrgb[3] = {0,0,0};
    tcs.setInterrupt(false); //turns on LED
    delay(60);
    tcs.getRawData(&r, &g, &b, &c);
    tcs.setInterrupt(true); //turns off LED
    tabrgb[1] = r;
    tabrgb[2] = g;
    tabrgb[3] = b;
    //Serial.print("C: "); Serial.print(c);
    //Serial.print("\tR: "); Serial.print(r);
    //Serial.print("\tG: "); Serial.print(g);
    //Serial.print("\tB: "); Serial.print(b);
    //colorTemp = tcs.calculateColorTemperature(r, g, b);
    //Lux = tcs.calculateLux(r, g, b);
    //Serial.print("\ncolorTemp: "); Serial.print(colorTemp);
    //Serial.print("\nLux: "); Serial.print(Lux);
    //Serial.print("\n");
    return AssignWordColor(tabrgb);
}

//indicates with a boolean if the goal sensed is the good goal
bool BonBut()//String couleurCapt)
{
    return ReadSensorColor() = COULEUR1;
}

//Uses the sonar to find the ball, align the middle of the robot with the middle of the ball and get the ball
void SonarBall()
{
    float distance1 = 0, distance2 = 0, angleBalai1 = 0, angleBalai2 = 0, angleMilieu = 0, distanceBallon = 0;
    while(abs(distance2-distance1) <= 5)
    {
        distance1 = SONAR_GetRange(1);
        delay(100);
        Turn(PI/64);
        distance2 = SONAR_GetRange(1);
        angleBalai1 += PI/64;
    }
    Turn(-angleBalai1);
    distance1 = 0;
    distance2 = 0;
    while(abs(distance2-distance1) <= 5)
    {
        distance1 = SONAR_GetRange(1);
        delay(100);
        Turn(-PI/64);
        distance2 = SONAR_GetRange(1);
        angleBalai2 += PI/64;
    }
    angleMilieu = (angleBalai1 + angleBalai2)/2;
    Turn(angleMilieu);
    distanceBallon = SONAR_GetRange(1);
    Move(distanceBallon, 0.3);
}

void IRBallAlign()
{
   float distance1 = 10, distance2 = 10, angleBalai1 = 0, angleBalai2 = 0, angleMilieu = 0, distanceBallon = 10;
    do //while(abs(distance2-distance1) <= 5) //&& distance1 <= 80 && distance2 <= 80 && distance1 >= 10 && distance2 >= 10)
    {
        distance1 = sharp.distance(); //SONAR_GetRange(1);
        //delay(100);
        Turn(PI/16);
        distance2 = sharp.distance(); //SONAR_GetRange(1);
        angleBalai1 += PI/16;
    }while(abs(distance2-distance1) <= 5);
    Turn(-angleBalai1);
    distance1 = 10;
    distance2 = 10;
    do //while(abs(distance2-distance1) <= 5) //&& distance1 <= 80 && distance2 <= 80 && distance1 >= 10 && distance2 >= 10)
    {
        distance1 = sharp.distance(); //SONAR_GetRange(1);
        //delay(100);
        Turn(-PI/16);
        distance2 = sharp.distance(); //SONAR_GetRange(1);
        angleBalai2 += PI/16;
    }while(abs(distance2-distance1) <= 5);
    angleMilieu = (angleBalai1 + angleBalai2)/2;
    Turn(angleMilieu);
    distanceBallon = sharp.distance(); //SONAR_GetRange(1);
    Move(distanceBallon, 0.3); 
}*/

///Scans from PI/4 to -PI/4 rads to find the ball, then turns to face it.
///Returns the angle from the center.
float AlignAlec()
{
    int i;
    float tabDistance[16] = {200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200};
    float tabAngle[16] = {PI/2, 15*PI/32, 14*PI/32, 13*PI/32, 12*PI/32, 11*PI/32, 10*PI/32, 9*PI/32, 8*PI/32, 7*PI/32, 6*PI/32, 5*PI/32, 4*PI/32, 3*PI/32, 2*PI/32, PI/32};
    float smallestDistance = tabDistance[0];
    int smallestDistancePos = 0;
    Turn(PI/4);
    for(i = 0; i < 16; i++)
    {
        tabDistance[i] = sharp.distance();
        delay(300);
        if(tabDistance[i] == 8) //nothing gets reflected, i.e. the surface is too far away
        {
            tabDistance[i] = 200;
        }
        else if(tabDistance[i] < smallestDistance)
        {
            smallestDistance = tabDistance[i];
            smallestDistancePos = i;
        }
        Turn(-PI/32);
    }
    Turn(tabAngle[smallestDistancePos]);
    return (-PI/4) + tabAngle[smallestDistancePos];
    //Move(40, 0.4);
}
//print durant fct pour voir ce qu'il voit

/*float IRBallPO()
{
   float distance1, angleBalai1 = 0, angleMilieu = 0, distanceBallon;
   bool foundBall = false;
   Turn (-PI/4);
    while (foundBall==false) //while(abs(distance2-distance1) <= 5) //&& distance1 <= 80 && distance2 <= 80 && distance1 >= 10 && distance2 >= 10)
    {
        distance1 = sharp.distance(); //sharp.distance();
        while (distance1 <=60)
        {
            Turn(PI/32);
            distance1 = sharp.distance();
            angleBalai1 += PI/32;
            foundBall = true;
            delay(300);
        }
        Turn(PI/32);
        delay(300);
    }
    angleMilieu = (-angleBalai1)/2;
    Turn(-PI/32 + angleMilieu);
    return angleMilieu;
}*/