

#include "../../src/NAU7802.h" // Click here to get the library: http://librarymanager/All#SparkFun_NAU8702

NAU7802 myScale; // Create instance of the NAU7802 class

// EEPROM locations to store 4-byte variables
#define LOCATION_CALIBRATION_FACTOR 0 // Float, requires 4 bytes of EEPROM
#define LOCATION_ZERO_OFFSET 10       // Must be more than 4 away from previous spot. Long, requires 4 bytes of EEPROM

bool settingsDetected = false; // Used to prompt user to calibrate their scale

// Create an array to take average of weights. This helps smooth out jitter.
#define AVG_SIZE 4
float avgWeights[AVG_SIZE];
byte avgWeightSpot = 0;

int main()
{

    if (myScale.begin() == false)
    {
        cout << "Scale not detected. Please check wiring Freezing...";
        while (1)
            ;
    }
    cout << "Scale detected!" << endl;

    myScale.setSampleRate(NAU7802_SPS_320); // Increase to max sample rate
    myScale.calibrateAFE();                 // Re-cal analog front end when we change gain, sample rate, or channel

    cout << "Zero offset: ";
    cout << myScale.getZeroOffset() << endl;
    cout << "Calibration factor: ";
    cout << myScale.getCalibrationFactor() << endl;

    while (1)
    {
        if (myScale.available() == true)
        {
            long currentReading = myScale.getReading();
            float currentWeight = myScale.getWeight();

    cout<<"Reading: ");
    cout<<currentReading);
    cout<<"\tWeight: ");
    cout<<currentWeight, 2); //Print 2 decimal places

    avgWeights[avgWeightSpot++] = currentWeight;
    if (avgWeightSpot == AVG_SIZE)
        avgWeightSpot = 0;

    float avgWeight = 0;
    for (int x = 0; x < AVG_SIZE; x++)
        avgWeight += avgWeights[x];
    avgWeight /= AVG_SIZE;

    cout<<"\tAvgWeight: ");
    cout<<avgWeight, 2); //Print 2 decimal places

    if (settingsDetected == false)
    {
        cout << "\tScale not calibrated. Press 'c'.";
    }
    cout << endl;
        }

        if (Serial.available())
        {
            byte incoming = Serial.read();

            if (incoming == 't') // Tare the scale
                myScale.calculateZeroOffset();
            else if (incoming == 'c') // Calibrate
            {
                calibrateScale();
            }
        }
    }
}
// Gives user the ability to set a known weight on the scale and calculate a calibration factor
void calibrateScale(void)
{
    cout << endl;
    cout << endl;
    cout << "Scale calibration" << endl;

    cout << "Setup scale with no weight on it. Press a key when ready." << endl;

    myScale.calculateZeroOffset(64); // Zero or Tare the scale. Average over 64 readings.
    cout << "New zero offset: ";
    cout << myScale.getZeroOffset();

    Serial.println(F("Place known weight on scale. Press a key when weight is in place and stable."));

    cout << "Please enter the weight, without units, currently sitting on the scale (for example '4.25'): ";

    // Read user input
    float weightOnScale = Serial.parseFloat();
    Serial.println();

    myScale.calculateCalibrationFactor(weightOnScale, 64); // Tell the library how much weight is currently on it
  cout<<F("New cal factor: "));
  Serial.println(myScale.getCalibrationFactor(), 2);

  cout<<F("New Scale Reading: "));
  Serial.println(myScale.getWeight(), 2);
}
