/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre, Vincent-Xavier Voyer, Pierre-Olivier Lajoie, Robin Mailhot
Description: Programme qui gere le ROBUS pour le concours PIRUS.
Date: 30 novembre 2019
*/

//--------------- Libraries ---------------

#include <Arduino.h>
#include <LibRobus.h>
#include <math.h>
#include <LiquidCrystal.h>

//--------------- Defines ---------------

#define HOUR 0
#define MIN 1
#define SEC 2
#define X 0
#define Y 1
#define FORWARD 0
#define BACKWARDS 1
#define TURN_LEFT 2
#define TURN_RIGHT 3

//--------------- Function declarations ---------------

int DistanceToPulses(float);
void reset();

//--------------- Enumerations ---------------

enum class Mode { Sleep, Alarm, Simon};
enum class Orientation { North, West, South, East};

//--------------- Constants ---------------

//----- General -----
const float WHEEL_CIRCUMFERENCE = 23.94;
const float RADIUS_ROTATION = 40000 / 2; //19.33 is the distance between the wheels
const int PULSES_PER_CYCLE = 3200;
const float BASE_SPEED = 0.4;

//----- Music -----
const float BASE_VOLUME = 1;
const unsigned long MUSIC_FADE_TIME = 10000; //in milliseconds

//----- Movement -----
const int DISTANCE = 20000;//DistanceToPulses(20); 
const int DISTANCE_ROTATION = RADIUS_ROTATION * PI/2;//DistanceToPulses(RADIUS_ROTATION * PI/2); //the rotations will always be of 90 degrees
//there are 4 movements (in order) : forward, backwards, turn left, turn right
const int MOVEMENTS[4][2] = { {DISTANCE, DISTANCE}, {-DISTANCE, -DISTANCE}, {-DISTANCE_ROTATION, DISTANCE_ROTATION}, {DISTANCE_ROTATION, -DISTANCE_ROTATION}};
const int INI_X = 1;
const int INI_Y = 1;
const int MAX_X = 2;
const int MAX_Y = 2;

const int SPEED = 40; // cm/s
const unsigned long MOVE_TIME = DISTANCE / SPEED;
const unsigned long MOVE_TIME_ROTATION = (RADIUS_ROTATION * PI/2) / SPEED;
const unsigned long MOVEMENTS_TIME[4] = { MOVE_TIME, MOVE_TIME, MOVE_TIME_ROTATION, MOVE_TIME_ROTATION};
const int SPEED_SIGN[4][2] = { {1, 1}, {-1, -1}, {-1, 1}, {1, -1}};

//----- Clock -----
const unsigned long COUNTDOWN_TIME = 10000;
const int TIME_DEFAULT_HOUR = 6;
const int TIME_DEFAULT_MIN = 59;
const int TIME_DEFAULT_SEC = 55;
const int TIME_ALARM_HOUR = 7;
const int TIME_ALARM_MIN = 0;
const int TIME_ALARM_SEC = 0;

//----- Bells -----
const int TIME_START_BELLS = 5000;
const int PIN_BELLS = 53;

//----- Simon -----
const int PIN_LED_01 = 23;
const int PIN_LED_02 = 25;
const int PIN_LED_03 = 27;
const int PIN_LED_04 = 29;

const int PIN_BUTTON_01 = 22;
const int PIN_BUTTON_02 = 24;
const int PIN_BUTTON_03 = 26;
const int PIN_BUTTON_04 = 28;

const int PIN_SWITCH = 52;

const int LED_COUNT = 4;
///Sequence length
const int SEQ_LEN = 5;

const unsigned long DELAY_LED = 300;
const unsigned long DELAY_INPUT = 150;

//--------------- Variables ---------------

//----- General -----
Mode currentMode;
bool runProgram;
bool firstTimeInLoop;
unsigned long timeIni;
bool resetSeed;

//----- Music -----
bool musicPlaying;
bool musicFadeComplete;
float musicNewVolume;
unsigned long musicTimeIni;
unsigned long musicTimeCurrent;

//----- Movements -----
int totalPulsesLeft, totalPulsesRight, 
    deltaPulsesLeft, deltaPulsesRight,
    errorDelta, errorTotal;
const float kp = 0.001, ki = 0.001;

Orientation currentOrientation;
int currentMove;
int position[2]; //X and Y
int pulsesToTravel[2]; //LEFT and RIGHT
unsigned long timeToTravel;
unsigned long timeBeginningMove;
unsigned long timeCurrentMove;
bool moveCompleted;

//----- Clock and LCD -----
LiquidCrystal lcd(40,2,3,42,4,38);
int timeCurrent[3];
unsigned long timePrevious;
bool countdownOver;
bool timeHasChanged;

//----- Bells -----
bool bellsAreActive;


//---------------------------------------------------------------------------
//------------------------------   Functions   ------------------------------
//---------------------------------------------------------------------------

//---------------- General functions ----------------

///Converts a distance to the according number of pulses.
///distance : The distance in centimeters.
int DistanceToPulses(float distance)
{
    return (distance / WHEEL_CIRCUMFERENCE) * PULSES_PER_CYCLE;
}

///Uses the linetracker to return some random value.
int GetRandomData()
{
    return abs(analogRead(A1) + analogRead(A2) - analogRead(A3) - analogRead(A4) * analogRead(A5) + analogRead(A6));// * analogRead(A7) + analogRead(A8)); 
}

///Returns a random number in the range provided.
///min : the included minimum value.
///max : the included maximum value.
int Random(int min, int max)
{
    if(resetSeed) 
    {
        srand(GetRandomData());
        resetSeed = false;
    }
    return min + (rand() % (max + 1 - min));
}

///Checks if the user has showed a sign of life.
void CheckForInteraction()
{
    if(digitalRead(PIN_SWITCH) == LOW && currentMode == Mode::Alarm)
    {
        currentMode = Mode::Simon;
    }
    else if (digitalRead(PIN_SWITCH) == LOW && currentMode == Mode::Sleep)
    {
        delay(500);
        ReactivateSwitch();
    }
}

///Checks if it has to start the demo.
void CheckForStart()
{
    if(ROBUS_IsBumper(REAR))
    {
        runProgram = true;
        timePrevious = millis();
    }
}

//---------------- Music functions ----------------

///Raises the volume of the music over time.
void RaiseVolume()
{
    musicTimeCurrent = millis() - musicTimeIni;
    musicNewVolume = BASE_VOLUME + (musicTimeCurrent / MUSIC_FADE_TIME * (1 - BASE_VOLUME));
    if(musicNewVolume >= 1.0)
    {
        musicNewVolume = 1;
        musicFadeComplete = true;
    }
    AUDIO_SetVolume(musicNewVolume);
}

///Starts playing music at the indicated volume.
void StartMusic(float volume)
{
    musicPlaying = true;
    musicTimeIni = millis();
    AUDIO_SetVolume(volume);
    AUDIO_Next();
}

///Starts the music at a low volume - meant to be called if the volume will raise over time.
void StartMusic()
{
    musicFadeComplete = false;
    StartMusic(BASE_VOLUME);
}

///Stops music.
void StopMusic()
{
    musicPlaying = false;
    AUDIO_Stop();
}

///The main function to call to play music.
void PlayMusic()
{
    if(!musicPlaying)
    {
        StartMusic();
    }
    else if(!musicFadeComplete)
    {
        RaiseVolume();
    }
}

//---------------- Movement functions ----------------

///Updates the position of the ROBUS according to the move done.
///variation : 1 if it moves forward, -1 if it moves backwards
void UpdatePosition(int variation)
{
    switch (currentOrientation)
    {
        case Orientation::North:
            position[Y] -= variation;
            break;
        case Orientation::West:
            position[X] -= variation;
            break;
        case Orientation::South:
            position[Y] += variation;
            break;
        case Orientation::East:
            position[X] += variation;
            break;
    }
}

///Uses the current position and orientation to determine if a move is invalid.
///Returns the index of the invalid move, or -1 if anything is valid.
int DetermineInvalidMove()
{
    //it can always turn, so only Forward or Backwards can be invalid
    //the ifs (conditions) are not pretty, but are easily understandable
    int invalidMove = -1;
    if( (currentOrientation == Orientation::North && position[Y] == 0) ||
        (currentOrientation == Orientation::West && position[X] == 0) ||
        (currentOrientation == Orientation::South && position[Y] == MAX_Y) ||
        (currentOrientation == Orientation::East && position[X] == MAX_X))
    {
        invalidMove = FORWARD;
    }
    else if((currentOrientation == Orientation::North && position[Y] == MAX_Y) ||
            (currentOrientation == Orientation::West && position[X] == MAX_X) ||
            (currentOrientation == Orientation::South && position[Y] == 0) ||
            (currentOrientation == Orientation::East && position[X] == 0))
    {
        invalidMove = BACKWARDS;
    }
    return invalidMove;
}

///Generates a random move for the ROBUS. Makes sure it doesn't go out of the allowed area.
void GenerateRandomMove()
{
    int newMove = -1;
    //check orientation and position, so the next move is chosen accordingly
    int invalidMove = DetermineInvalidMove();

    //pick a random, valid move
    do
    {
        newMove = Random(0,3);
    } 
    while (newMove == invalidMove);
    
    //set the new distance to travel
    pulsesToTravel[LEFT] = MOVEMENTS[newMove][LEFT];
    pulsesToTravel[RIGHT] = MOVEMENTS[newMove][RIGHT];

    //set the new position or orientation
    switch (newMove)
    {
        case FORWARD:
            UpdatePosition(1);
            break;
        case BACKWARDS:
            UpdatePosition(-1);
            break;
        case TURN_LEFT:
            currentOrientation = (Orientation)(((int)currentOrientation + 1) % 4);
            break;
        case TURN_RIGHT:
            currentOrientation = (Orientation)(((int)currentOrientation - 1 + 4) % 4);
            break;
    }

    //make sure the next loop starts the move
    moveCompleted = false;
}

///Moves the ROBUS according to the values in pulsesToTravel.
void MoveUpdate()
{
    float correctedSpeed = 0;
    int speedSignLeft = pulsesToTravel[LEFT] / abs(pulsesToTravel[LEFT]);
    int speedSignRight = pulsesToTravel[RIGHT] / abs(pulsesToTravel[RIGHT]);

    if(totalPulsesLeft > abs(pulsesToTravel[LEFT]))
    {
        //the current movement is complete, so we reset the values
        MOTOR_SetSpeed(LEFT, 0);
        MOTOR_SetSpeed(RIGHT, 0);
        totalPulsesLeft = 0;
        totalPulsesRight = 0;
        moveCompleted = true;
    }
    else if(totalPulsesLeft == 0)
    {
        //it starts the movement, so we initialize correctly some stuff
        MOTOR_SetSpeed(LEFT, speedSignLeft * BASE_SPEED);
        MOTOR_SetSpeed(RIGHT, speedSignRight * BASE_SPEED);
        totalPulsesLeft = 1;
        totalPulsesRight = 1;
    }
    else
    {
        //it's in the middle of a movement
        deltaPulsesLeft = abs(ENCODER_ReadReset(LEFT));
        deltaPulsesRight = abs(ENCODER_ReadReset(RIGHT));
        totalPulsesLeft += deltaPulsesLeft;
        totalPulsesRight += deltaPulsesRight;

        Serial.print("totalPulsesLeft  : "); Serial.println(totalPulsesLeft);
        Serial.print("totalPulsesRight : "); Serial.println(totalPulsesRight);

        //the right motor is the master
        errorDelta = deltaPulsesRight - deltaPulsesLeft;
        errorTotal = totalPulsesRight - totalPulsesLeft;

        correctedSpeed = BASE_SPEED + (errorDelta * kp) + (errorTotal * ki);
        MOTOR_SetSpeed(LEFT, speedSignLeft * correctedSpeed);
    }
}

///Stops a movement, even if it's not over.
void StopMovement()
{
    moveCompleted = true;
    timeBeginningMove = 0;
    timeCurrentMove = 0;
    currentMove = 0;
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///The main function to call to move the robot.
void Move()
{
    if(moveCompleted)
    {
        GenerateRandomMove();
    }
    else
    {
        MoveUpdate();
    }
}

//---------------- Movement functions (time) ----------------

///Generates a random move for the ROBUS. Makes sure it doesn't go out of the allowed area. Relies on time.
void GenerateRandomMoveTime()
{
    int newMove = -1;
    //check orientation and position, so the next move is chosen accordingly
    int invalidMove = DetermineInvalidMove();

    //pick a random, valid move
    do
    {
        newMove = Random(0,3);
    } 
    while (newMove == invalidMove);
    
    currentMove = newMove;

    //set the new distance to travel
    timeToTravel = MOVEMENTS_TIME[currentMove];

    //set the new position or orientation
    switch (currentMove)
    {
        case FORWARD:
            UpdatePosition(1);
            break;
        case BACKWARDS:
            UpdatePosition(-1);
            break;
        case TURN_LEFT:
            currentOrientation = (Orientation)(((int)currentOrientation + 1) % 4);
            break;
        case TURN_RIGHT:
            currentOrientation = (Orientation)(((int)currentOrientation - 1 + 4) % 4);
            break;
    }

    //make sure the next loop starts the move
    moveCompleted = false;
}

///Moves the robot for a certain amount of time - doesn't rely on the encoders.
void MoveUpdateTime()
{
    if(timeCurrentMove > timeToTravel)
    {
        //the current movement is complete, so we reset the values
        MOTOR_SetSpeed(LEFT, 0);
        MOTOR_SetSpeed(RIGHT, 0);
        moveCompleted = true;
        timeBeginningMove = 0;
        timeCurrentMove = 0;
    }
    else if(timeCurrentMove == 0)
    {
        //it starts the movement, so we initialize correctly some stuff
        MOTOR_SetSpeed(LEFT, SPEED_SIGN[currentMove][LEFT] * BASE_SPEED);
        MOTOR_SetSpeed(RIGHT, SPEED_SIGN[currentMove][RIGHT] * BASE_SPEED);
        timeBeginningMove = millis();
        timeCurrentMove = 1;
    }
    else
    {
        //it's in the middle of a movement
        timeCurrentMove = millis() - timeBeginningMove;
    }
}

///The main function to call to move the robot, with time as a reference.
void MoveTime()
{
    if(moveCompleted)
    {
        GenerateRandomMoveTime();
    }
    else
    {
        MoveUpdateTime();
    }
}

//---------------- Clock and LCD functions ----------------

///Updates the false clock of the ROBUS everytime it is called.
void Clock()
{
    int sec = (millis() - timePrevious) / 1000;
    if(sec >= 1)
    {
        timePrevious = millis();
        timeCurrent[SEC] += sec;
        timeHasChanged = true;

        if(timeCurrent[SEC] >= 60)
        {
            timeCurrent[MIN] += timeCurrent[SEC] / 60;
            timeCurrent[SEC] %= 60;

            if(timeCurrent[MIN] >= 60)
            {
                timeCurrent[HOUR] += timeCurrent[MIN] / 60;
                timeCurrent[MIN] %= 60;

                if(timeCurrent[HOUR] >= 24)
                {
                    timeCurrent[HOUR] %= 24;
                }
            }
        }
    }
}

///Prints the current time, plus some info about the current status.
void PrintTime()
{
    if(timeHasChanged)
    {
        lcd.clear();
        lcd.setCursor(6,0);
        if(timeCurrent[HOUR] < 10)
            lcd.print('0');
        lcd.print(timeCurrent[HOUR]);

        if(timeCurrent[SEC] % 2 == 0)
        {
            lcd.print(':');
        }
        else
        {
            lcd.print(' ');
        }
        
        if(timeCurrent[MIN] < 10)
            lcd.print('0');
        lcd.print(timeCurrent[MIN]);

        timeHasChanged = false;

        PrintInfoLine();

        //to print in the console
        if(timeCurrent[HOUR] < 10)
            Serial.print('0');
        Serial.print(timeCurrent[HOUR]);
        Serial.print(':');
        if(timeCurrent[MIN] < 10)
            Serial.print('0');
        Serial.print(timeCurrent[MIN]);
        Serial.print('.');
        if(timeCurrent[SEC] < 10)
            Serial.print('0');
        Serial.println(timeCurrent[SEC]);
    }
}

///Prints the second line that displays the current status.
void PrintInfoLine()
{
    if(currentMode == Mode::Sleep)
    {
        lcd.setCursor(0,1);
        lcd.print("Alarme : 0");
        lcd.print(TIME_ALARM_HOUR);
        lcd.print(":0");
        lcd.print(TIME_ALARM_MIN);
    }
    else if(currentMode == Mode::Alarm)
    {
        lcd.setCursor(1,1);
        lcd.print("Salut bonjour!");
    }
    else if(currentMode == Mode::Simon)
    {
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Complete le");
        lcd.setCursor(0,1);
        lcd.print("Simon en 15 sec!");
    }
}

///Checks if the alarm is supposed to start.
void CheckForAlarm()
{
    if (timeCurrent[HOUR] == TIME_ALARM_HOUR && 
        timeCurrent[MIN] == TIME_ALARM_MIN && 
        timeCurrent[SEC] == TIME_ALARM_SEC)
    {
        currentMode = Mode::Alarm;
    }
}

//---------------- Bells functions ----------------

void StartBells()
{
    digitalWrite(PIN_BELLS, HIGH);
    bellsAreActive = true;
}

void StopBells()
{
    digitalWrite(PIN_BELLS, LOW);
    bellsAreActive = false;
}

void Bells()
{
    if(!bellsAreActive)
        StartBells();
}

//---------------- Simon functions ----------------

void ReactivateSwitch()
{
    SERVO_SetAngle(0, 110);
    delay(600);
    SERVO_SetAngle(0, 170);
}

///Executes the whole sequence for the Simon game.
///It's the only function that isn't called repeatedly in a loop - 
///it will exit the function only once it's done.
void Simon()
{
    PrintInfoLine();
    StopMovement();
    StopBells();

    delay(1000);

    int button1IsPressed = 0;
    int button2IsPressed = 0;
    int button3IsPressed = 0;
    int button4IsPressed = 0;
    int n = 0;
    int userOrder[SEQ_LEN];
    int simonOrder[SEQ_LEN];
    for(int i = 0; i < SEQ_LEN; i++)
    {
        userOrder[i] = 0;
        simonOrder[i] = Random(1, LED_COUNT);
    }

    //----- Part 1 : Showing the sequence -----
    for(int i = 0; i < SEQ_LEN; i++)
    {
        if(simonOrder[i] == 1)
        {
            digitalWrite(PIN_LED_01,HIGH);
            delay(1000);
            digitalWrite(PIN_LED_01,LOW);
            delay(500);
        }
        else if(simonOrder[i] == 2)
        {
            digitalWrite(PIN_LED_02,HIGH);
            delay(1000);
            digitalWrite(PIN_LED_02,LOW);
            delay(500);
        }
        else if(simonOrder[i] == 3)
        {
            digitalWrite(PIN_LED_03,HIGH);
            delay(1000);
            digitalWrite(PIN_LED_03,LOW);
            delay(500);
        }
        else if(simonOrder[i] == 4)
        {
            digitalWrite(PIN_LED_04,HIGH);
            delay(1000);
            digitalWrite(PIN_LED_04,LOW);
            delay(500);
        }
    }

    //----- Part 2 : Reading user input -----

    unsigned long timeSimonIni = millis();

    while(n < SEQ_LEN && (millis() - timeSimonIni) < 15000)
    {   
        button1IsPressed = digitalRead(PIN_BUTTON_01);
        button2IsPressed = digitalRead(PIN_BUTTON_02);
        button3IsPressed = digitalRead(PIN_BUTTON_03);
        button4IsPressed = digitalRead(PIN_BUTTON_04);

        if(button1IsPressed)
        {
            digitalWrite(PIN_LED_01, HIGH);
            delay(DELAY_LED);
            digitalWrite(PIN_LED_01, LOW);
            userOrder[n] = 1;
            n++;
        }
        if(button2IsPressed)
        {
            digitalWrite(PIN_LED_02, HIGH);
            delay(DELAY_LED);
            digitalWrite(PIN_LED_02,LOW);
            userOrder[n] = 2;
            n++;
        }
        if(button3IsPressed)
        {
            digitalWrite(PIN_LED_03, HIGH);
            delay(DELAY_LED);
            digitalWrite(PIN_LED_03,LOW);
            userOrder[n] = 3;
            n++;
        }
        if(button4IsPressed)
        {
            digitalWrite(PIN_LED_04, HIGH);
            delay(DELAY_LED);
            digitalWrite(PIN_LED_04,LOW);
            userOrder[n] = 4;
            n++;
        }
        delay(DELAY_INPUT);
    }

    //----- Part 3 : Determining the result -----

    bool seqIsCorrect = true;
    for(int i = 0; i < SEQ_LEN && seqIsCorrect; i++)
    {
        seqIsCorrect = userOrder[i] == simonOrder[i];
    }

    if (seqIsCorrect)
    {
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("Defi complete");
        lcd.setCursor(1,1);
        lcd.print("Bonne journee!");

        //flash the LEDs in case the LCD doesn't work
        for(int i = 0; i < 3; i++)
        {
            digitalWrite(PIN_LED_01, HIGH);
            digitalWrite(PIN_LED_02, HIGH);
            digitalWrite(PIN_LED_03, HIGH);
            digitalWrite(PIN_LED_04, HIGH);
            delay(250);
            digitalWrite(PIN_LED_01, LOW);
            digitalWrite(PIN_LED_02, LOW);
            digitalWrite(PIN_LED_03, LOW);
            digitalWrite(PIN_LED_04, LOW);
            delay(250);
        }

        //delay(5000);
        
        reset(); //this will reset the robot, ready for a next demo
    }
    else
    {
        //Sequence is incorrect, or time is over
        lcd.clear();
        lcd.print("Oh non, reessaie");

        //flash the red LED in case the LCD doesn't work
        for(int i = 0; i < 3; i++)
        {
            digitalWrite(PIN_LED_01, HIGH);
            delay(250);
            digitalWrite(PIN_LED_01, LOW);
            delay(250);
        }

        //delay(2500);
        ReactivateSwitch();
        currentMode = Mode::Alarm;
    }
    //firstTimeInLoop = true;
}

//---------------- Init and loop functions ----------------

void setup()
{
    //----- Board initialization -----

    //--- General ---
    BoardInit();
    AudioInit();
    lcd.begin(16,2);

    //--- Simon ---
    pinMode(PIN_LED_01, OUTPUT);
    pinMode(PIN_LED_02, OUTPUT);
    pinMode(PIN_LED_03, OUTPUT);
    pinMode(PIN_LED_04, OUTPUT);
    pinMode(PIN_BUTTON_01, INPUT);
    pinMode(PIN_BUTTON_02, INPUT);
    pinMode(PIN_BUTTON_03, INPUT);
    pinMode(PIN_BUTTON_04, INPUT);

    //--- Bells ---
    pinMode(PIN_BELLS, OUTPUT);
    digitalWrite(PIN_BELLS, LOW);

    //--- Switch ---
    SERVO_Enable(0);
    ReactivateSwitch();
    pinMode(PIN_SWITCH, INPUT);

    //----- Variables initialization -----

    //--- General ---
    runProgram = false;
    currentMode = Mode::Sleep;
    firstTimeInLoop = true;
    timeIni = 0;
    resetSeed = true;

    //--- Music ---
    musicPlaying = false;
    musicFadeComplete = false;
    musicNewVolume = 0;
    musicTimeIni = 0;
    musicTimeCurrent = 0;

    //--- Movement ---
    totalPulsesLeft = 0;
    totalPulsesRight = 0;
    deltaPulsesLeft = 0;
    deltaPulsesRight = 0;
    errorDelta = 0;
    errorTotal = 0;

    currentOrientation = Orientation::North;
    position[X] = INI_X;
    position[Y] = INI_Y;
    pulsesToTravel[LEFT] = 0;
    pulsesToTravel[RIGHT] = 0;
    timeToTravel = 0;
    timeBeginningMove = 0;
    timeCurrentMove = 0;
    moveCompleted = true;

    //--- Clock ---
    timeCurrent[HOUR] = TIME_DEFAULT_HOUR;
    timeCurrent[MIN] = TIME_DEFAULT_MIN;
    timeCurrent[SEC] = TIME_DEFAULT_SEC;
    timePrevious = 0;
    timeHasChanged = true;

    //--- Bells ---
    bellsAreActive = false;

    PrintTime();
}

///Function to make the robot stop and re-initialize everything.
void reset()
{
    StopMusic();
    StopMovement();
    StopBells();
    setup();
}

///The main program, called in the loop() function.
void MainProgram()
{
    Clock();
    PrintTime();
    if(currentMode == Mode::Sleep)
    {
        CheckForAlarm();
    }
    else if (currentMode == Mode::Alarm)
    {
        if(firstTimeInLoop)
        {
            timeIni = millis();
            firstTimeInLoop = false;
        }
        MoveTime();
        PlayMusic();
        if(millis() - timeIni > TIME_START_BELLS)
        {
            Bells();
        }
        CheckForInteraction();
    }
    else if (currentMode == Mode::Simon)
    {
        Simon();
    }
}

void loop()
{
    CheckForStart();
    if(runProgram)
    {
        MainProgram();
    }
    //SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}