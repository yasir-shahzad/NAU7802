#include "../../src/NAU7802.h" // Click here to get the library: http://librarymanager/All#SparkFun_NAU8702

NAU7802 myScale; // Create instance of the NAU7802 class

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
            // float currentWeight = myScale.getWeight();
            
            cout << "Reading: ";
            cout << currentReading << endl;
            //  cout << "\tWeight: ";
            //  cout << (currentWeight, 2) << endl; // Print 2 decimal placesy

            //  myScale.calculateZeroOffset();
            // cout << myScale.getZeroOffset() << endl;
            usleep(150E3);
        }
    }
    return 0;
}

