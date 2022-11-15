#include "../../src/NAU7802.h" // Click here to get the library: http://librarymanager/All#SparkFun_NAU8702

NAU7802 myScale; // Create instance of the NAU7802 class


bool settingsDetected = false; // Used to prompt user to calibrate their scale

// Create an array to take average of weights. This helps smooth out jitter.
#define AVG_SIZE 4
float avgWeights[AVG_SIZE];
uint8_t avgWeightSpot = 0;

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

            cout << "Reading: ";
            cout << currentReading;
            cout << "\tWeight: ";
            cout << (currentWeight, 2) << endl; // Print 2 decimal places

            myScale.calculateZeroOffset();
            cout << myScale.getZeroOffset() << endl;
            usleep(500E3);
        }
    }
    return 0;
}

