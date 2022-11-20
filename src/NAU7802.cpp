/*
  This is an Arduino library written for the NAU7802 24-bit wheatstone
  bridge and load cell amplifier.
  By Nathan Seidle @ SparkFun Electronics, March 3nd, 2019

  The NAU7802 is an I2C device that converts analog signals to a 24-bit
  digital signal. This makes it possible to create your own digital scale
  either by hacking an off-the-shelf bathroom scale or by creating your
  own scale using a load cell.

  The NAU7802 is a better version of the popular HX711 load cell amplifier.
  It uses a true I2C interface so that it can share the bus with other
  I2C devices while still taking very accurate 24-bit load cell measurements
  up to 320Hz.

  https://github.com/sparkfun/SparkFun_Qwiic_Scale_NAU7802_Arduino_Library

  SparkFun labored with love to create this code. Feel like supporting open
  source? Buy a board from SparkFun!
  https://www.sparkfun.com/products/15242
*/

#include "NAU7802.h"

//Constructor
NAU7802::NAU7802(uint8_t i2c_bus, uint8_t i2c_addr)
{
    this->i2c_addr = i2c_addr;
    this-> i2c_bus = i2c_bus;
    refTime = std::chrono::steady_clock::now();
}

//Sets up the NAU7802 for basic function
//If initialize is true (or not specified), default init and calibration is performed
//If initialize is false, then it's up to the caller to initalize and calibrate
//Returns true upon completion
bool NAU7802::begin(bool initialize)
{
    char device[32];
    snprintf(device, sizeof(device), "/dev/i2c-%u", i2c_bus); // creating device address buffer
    if ((fd = open(device, O_RDWR)) < 0)
    {
        printf("File descriptor opening error %s\n", strerror(errno));
        return 0;
    }
    else {
        if (ioctl(fd, I2C_SLAVE, i2c_addr) < 0)
        {
            std::cout << "Open fd error" << errno << std::endl;
            return 0;
        }
        printf("I2C connection established\n");
    }

    // Check if the device ack's over I2C
    if (isConnected() == false)
    {
        // There are rare times when the sensor is occupied and doesn't ack. A 2nd try resolves this.
        if (isConnected() == false)
            return (false);
    }

    bool result = true; // Accumulate a result as we do the setup

    if (initialize)
    {
        result &= reset(); // Reset all registers

        result &= powerUp(); // Power on analog and digital sections of the scale

        result &= setLDO(NAU7802_LDO_3V3); // Set LDO to 3.3V

        result &= setGain(NAU7802_GAIN_128); // Set gain to 128

        result &= setSampleRate(NAU7802_SPS_80); // Set samples per second to 10

        result &= setRegister(NAU7802_ADC, 0x30); // Turn off CLK_CHP. From 9.1 power on sequencing.

        result &= setBit(NAU7802_PGA_PWR_PGA_CAP_EN,
                         NAU7802_PGA_PWR); // Enable 330pF decoupling cap on chan 2. From 9.14 application circuit note.

        result &= calibrateAFE(); // Re-cal analog front end when we change gain, sample rate, or channel
    }

    return (result);
}

//Returns true if device is present
//Tests for device ack to I2C address
bool NAU7802::isConnected()
{
   if (ioctl(fd, I2C_SLAVE, i2c_addr) < 0) {
        printf("Error While Opening I2C connection : 3, Error Number: %d", errno);
        return 0; // Sensor did not ACK
   }
   return 1;  // All good
}

//Returns true if Cycle Ready bit is set (conversion is complete)
bool NAU7802::available()
{
  return (getBit(NAU7802_PU_CTRL_CR, NAU7802_PU_CTRL));
}

//Calibrate analog front end of system. Returns true if CAL_ERR bit is 0 (no error)
//Takes approximately 344ms to calibrate; wait up to 1000ms.
//It is recommended that the AFE be re-calibrated any time the gain, SPS, or channel number is changed.
bool NAU7802::calibrateAFE()
{
  beginCalibrateAFE();
  return waitForCalibrateAFE(1000);
}

//Begin asynchronous calibration of the analog front end.
// Poll for completion with calAFEStatus() or wait with waitForCalibrateAFE()
void NAU7802::beginCalibrateAFE()
{
  setBit(NAU7802_CTRL2_CALS, NAU7802_CTRL2);
}

//Check calibration status.
NAU7802_Cal_Status NAU7802::calAFEStatus()
{
  if (getBit(NAU7802_CTRL2_CALS, NAU7802_CTRL2))
  {
    return NAU7802_CAL_IN_PROGRESS;
  }

  if (getBit(NAU7802_CTRL2_CAL_ERROR, NAU7802_CTRL2))
  {
    return NAU7802_CAL_FAILURE;
  }

  // Calibration passed
  return NAU7802_CAL_SUCCESS;
}

//Wait for asynchronous AFE calibration to complete with optional timeout.
//If timeout is not specified (or set to 0), then wait indefinitely.
//Returns true if calibration completes succsfully, otherwise returns false.
bool NAU7802::waitForCalibrateAFE(uint32_t timeout_ms)
{
  uint32_t begin = millis();
  NAU7802_Cal_Status cal_ready;

  while ((cal_ready = calAFEStatus()) == NAU7802_CAL_IN_PROGRESS)
  {
    if ((timeout_ms > 0) && ((millis() - begin) > timeout_ms))
    {
      break;
    }
    usleep(1E3);
  }

  if (cal_ready == NAU7802_CAL_SUCCESS)
  {
    return (true);
  }
  return (false);
}

//Set the readings per second
//10, 20, 40, 80, and 320 samples per second is available
bool NAU7802::setSampleRate(uint8_t rate)
{
  if (rate > 0b111)
    rate = 0b111; //Error check

  uint8_t value = getRegister(NAU7802_CTRL2);
  value &= 0b10001111; //Clear CRS bits
  value |= rate << 4;  //Mask in new CRS bits

  return (setRegister(NAU7802_CTRL2, value));
}

//Select between 1 and 2
bool NAU7802::setChannel(uint8_t channelNumber)
{
  if (channelNumber == NAU7802_CHANNEL_1)
    return (clearBit(NAU7802_CTRL2_CHS, NAU7802_CTRL2)); //Channel 1 (default)
  else
    return (setBit(NAU7802_CTRL2_CHS, NAU7802_CTRL2)); //Channel 2
}

//Power up digital and analog sections of scale
bool NAU7802::powerUp()
{
  setBit(NAU7802_PU_CTRL_PUD, NAU7802_PU_CTRL);
  setBit(NAU7802_PU_CTRL_PUA, NAU7802_PU_CTRL);

  //Wait for Power Up bit to be set - takes approximately 200us
  uint8_t counter = 0;
  while (1)
  {
    if (getBit(NAU7802_PU_CTRL_PUR, NAU7802_PU_CTRL) == true) {
      break; //Good to go
    }
      usleep(1E3);
    if (counter++ > 100) {
      return (false); //Error
    }
  }
  return (true);
}

//Puts scale into low-power mode
bool NAU7802::powerDown()
{
  clearBit(NAU7802_PU_CTRL_PUD, NAU7802_PU_CTRL);
  return (clearBit(NAU7802_PU_CTRL_PUA, NAU7802_PU_CTRL));
}

//Resets all registers to Power Of Defaults
bool NAU7802::reset()
{
  setBit(NAU7802_PU_CTRL_RR, NAU7802_PU_CTRL); //Set RR
  usleep(1E3);
  return (clearBit(NAU7802_PU_CTRL_RR, NAU7802_PU_CTRL)); //Clear RR to leave reset state
}

//Set the onboard Low-Drop-Out voltage regulator to a given value
//2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.2, 4.5V are available
bool NAU7802::setLDO(uint8_t ldoValue)
{
  if (ldoValue > 0b111)
    ldoValue = 0b111; //Error check

  //Set the value of the LDO
  uint8_t value = getRegister(NAU7802_CTRL1);
  value &= 0b11000111;    //Clear LDO bits
  value |= ldoValue << 3; //Mask in new LDO bits
  setRegister(NAU7802_CTRL1, value);

  return (setBit(NAU7802_PU_CTRL_AVDDS, NAU7802_PU_CTRL)); //Enable the internal LDO
}

//Set the gain
//x1, 2, 4, 8, 16, 32, 64, 128 are avaialable
bool NAU7802::setGain(uint8_t gainValue)
{
  if (gainValue > 0b111)
    gainValue = 0b111; //Error check

  uint8_t value = getRegister(NAU7802_CTRL1);
  value &= 0b11111000; //Clear gain bits
  value |= gainValue;  //Mask in new bits

  return (setRegister(NAU7802_CTRL1, value));
}

//Get the revision code of this IC
uint8_t NAU7802::getRevisionCode()
{
  uint8_t revisionCode = getRegister(NAU7802_DEVICE_REV);
  return (revisionCode & 0x0F);
}

//Returns 24-bit reading
//Assumes CR Cycle Ready bit (ADC conversion complete) has been checked to be 1
int32_t NAU7802::getReading()
{
    uint8_t data[3];
    uint8_t ret;
    // read Current from register
    ret = i2c_smbus_read_i2c_block_data(fd, NAU7802_ADCO_B2, sizeof(data), data);
    // data[0] contains the length of the data
    if (ret > 1) // number of bytes that were read
    {
        uint32_t valueRaw = (uint32_t)data[0] << 16; // MSB
        valueRaw |= (uint32_t)data[1] << 8;          // MidSB
        valueRaw |= (uint32_t)data[2];               // LSB
        // the raw value coming from the ADC is a 24-bit number, so the sign bit now
        // resides on bit 23 (0 is LSB) of the uint32_t container. By shifting the
        // value to the left, I move the sign bit to the MSB of the uint32_t container.
        // By casting to a signed int32_t container I now have properly recovered
        // the sign of the original value
        int32_t valueShifted = (int32_t)(valueRaw << 8);
        // shift the number back right to recover its intended magnitude
        int32_t value = (valueShifted >> 8);

        return (value);
    }

    return (0); // Error
}

//Return the average of a given number of readings
//Gives up after 1000ms so don't call this function to average 8 samples setup at 1Hz output (requires 8s)
int32_t NAU7802::getAverage(uint8_t averageAmount)
{
  long total = 0;
  uint8_t samplesAquired = 0;

  unsigned long startTime = millis();
  while (1)
  {
    if (available() == true)
    {
      total += getReading();
      if (++samplesAquired == averageAmount)
        break; //All done
    }
    if (millis() - startTime > 1000) {
      return (0); //Timeout - Bail with error
    }
      usleep(1E3);
  }
  total /= averageAmount;

  return (total);
}

//Call when scale is setup, level, at running temperature, with nothing on it
void NAU7802::calculateZeroOffset(uint8_t averageAmount)
{
  setZeroOffset(getAverage(averageAmount));
}

//Sets the internal variable. Useful for users who are loading values from NVM.
void NAU7802::setZeroOffset(int32_t newZeroOffset)
{
  _zeroOffset = newZeroOffset;
}

int32_t NAU7802::getZeroOffset()
{
  return (_zeroOffset);
}

//Call after zeroing. Provide the float weight sitting on scale. Units do not matter.
void NAU7802::calculateCalibrationFactor(float weightOnScale, uint8_t averageAmount)
{
  int32_t onScale = getAverage(averageAmount);
  float newCalFactor = (onScale - _zeroOffset) / (float)weightOnScale;
  setCalibrationFactor(newCalFactor);
}

//Pass a known calibration factor into library. Helpful if users is loading settings from NVM.
//If you don't know your cal factor, call setZeroOffset(), then calculateCalibrationFactor() with a known weight
void NAU7802::setCalibrationFactor(float newCalFactor)
{
  _calibrationFactor = newCalFactor;
}

float NAU7802::getCalibrationFactor()
{
  return (_calibrationFactor);
}

//Returns the y of y = mx + b using the current weight on scale, the cal factor, and the offset.
float NAU7802::getWeight(bool allowNegativeWeights, uint8_t samplesToTake)
{
  int32_t onScale = getAverage(samplesToTake);

  //Prevent the current reading from being less than zero offset
  //This happens when the scale is zero'd, unloaded, and the load cell reports a value slightly less than zero value
  //causing the weight to be negative or jump to millions of pounds
  if (allowNegativeWeights == false)
  {
    if (onScale < _zeroOffset)
      onScale = _zeroOffset; //Force reading to zero
  }

  float weight = (onScale - _zeroOffset) / _calibrationFactor;
  return (weight);
}

//Set Int pin to be high when data is ready (default)
bool NAU7802::setIntPolarityHigh()
{
  return (clearBit(NAU7802_CTRL1_CRP, NAU7802_CTRL1)); //0 = CRDY pin is high active (ready when 1)
}

//Set Int pin to be low when data is ready
bool NAU7802::setIntPolarityLow()
{
  return (setBit(NAU7802_CTRL1_CRP, NAU7802_CTRL1)); //1 = CRDY pin is low active (ready when 0)
}

//Mask & set a given bit within a register
bool NAU7802::setBit(uint8_t bitNumber, uint8_t registerAddress)
{
  uint8_t value = getRegister(registerAddress);
  value |= (1 << bitNumber); //Set this bit
  return (setRegister(registerAddress, value));
}

//Mask & clear a given bit within a register
bool NAU7802::clearBit(uint8_t bitNumber, uint8_t registerAddress)
{
  uint8_t value = getRegister(registerAddress);
  value &= ~(1 << bitNumber); //Set this bit
  return (setRegister(registerAddress, value));
}

//Return a given bit within a register
bool NAU7802::getBit(uint8_t bitNumber, uint8_t registerAddress)
{
  uint8_t value = getRegister(registerAddress);
  value &= (1 << bitNumber); //Clear all but this bit
  return (value);
}

//Get contents of a register
uint8_t NAU7802::getRegister(uint8_t registerAddress)
{
    int32_t retVal;
    retVal = i2c_smbus_read_byte_data(fd, registerAddress);
    if (retVal < 0) {
        printf("Error While reading Nau7802 I2C register, Error: %d", errno);
        return 0;
    }
    else {
        return (uint8_t)retVal;
    }
}

//Send a given value to be written to given address
//Return true if successful
bool NAU7802::setRegister(uint8_t registerAddress, uint8_t value)
{
    int32_t retVal;
    retVal = i2c_smbus_write_byte_data(fd, registerAddress, value);
    if (retVal < 0) {
        printf("Error While setting Nau7802 I2C register, Error#: %d", errno);
        return 0;
    }

    return 1;
}

unsigned long NAU7802::millis() {
    unsigned long value_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - refTime).count();
    return value_ms;
}

unsigned long NAU7802::micros() {
    unsigned long value_us =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - refTime).count();
    return value_us;
}

NAU7802::~NAU7802(){
    close(fd);
}
