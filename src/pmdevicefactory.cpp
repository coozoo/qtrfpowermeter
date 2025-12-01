#include "pmdevicefactory.h"
#include "abstractpmdevice.h"
#include "rf8000device.h"
#include "rfpmv7device.h"
#include "rfpmv5device.h"

PMDeviceFactory::PMDeviceFactory(QObject *parent) : QObject(parent)
{
    registerDevices();
}

PMDeviceFactory::~PMDeviceFactory() {}

void PMDeviceFactory::registerDevices()
{
    // --- RF Power Meter 10GHz V7 ---
    // this one added just as example how to add custom serial attenuator
    // current implementation will not work and require device to implement it
    PMDeviceProperties rf10000_v7;
    rf10000_v7.id = "rfpm_v7_10ghz";
    rf10000_v7.name = "RF-PM V7 10GHz";
    rf10000_v7.alternativeNames = "";
    rf10000_v7.imagePath = ":/images/devices/rf_pm_v7_10.png";
    rf10000_v7.minFreqHz = 1000000;
    rf10000_v7.maxFreqHz = 10000000000;
    rf10000_v7.minPowerDbm = -50.0;
    rf10000_v7.maxPowerDbm = 0.0;
    rf10000_v7.hasOffset = true;
    rf10000_v7.baudRate = 115200;
    rf10000_v7.hasInternalAttenuator = true;
    rf10000_v7.internalAttMinDb = 0.0;
    rf10000_v7.internalAttMaxDb = 30.0;
    rf10000_v7.internalAttStepDb = 0.25;
    rf10000_v7.isEnabled = false;
    m_deviceRegistry.insert(rf10000_v7.id, rf10000_v7);

    // --- RF Power Meter V5.0 ---
    PMDeviceProperties rfpmv5;
    rfpmv5.id = "rfpmv5";
    rfpmv5.name = "RF-PM V5.0 10GHz";
    rfpmv5.alternativeNames = "RF-Power-Meter-V5.0";
    rfpmv5.imagePath = ":/images/devices/rf_pm_v5_10.png";
    rfpmv5.minFreqHz = 1000000;
    rfpmv5.maxFreqHz = 10000000000;
    rfpmv5.minPowerDbm = -60.0;
    rfpmv5.maxPowerDbm = 0.0;
    rfpmv5.hasOffset = true;
    rfpmv5.baudRate = 460800;
    rfpmv5.hasInternalAttenuator = false;
    rfpmv5.isEnabled = true;
    m_deviceRegistry.insert(rfpmv5.id, rfpmv5);

    // --- RF Power Meter RF8000 ---
    PMDeviceProperties rf8000;
    rf8000.id = "rf8000";
    rf8000.name = "RF8000 8GHZ";
    rf8000.alternativeNames = "RF-Power 8000,RFPower8000";
    rf8000.imagePath = ":/images/devices/rf8000.png";
    rf8000.minFreqHz = 1000000;      // 1 MHz
    rf8000.maxFreqHz = 7999000000;   // 8000 MHz - 7999 is the highest value
    rf8000.minPowerDbm = -55.0;
    rf8000.maxPowerDbm = -5.0;
    rf8000.hasOffset = true;
    rf8000.baudRate = 9600;
    rf8000.hasInternalAttenuator = false;
    rf8000.isEnabled = true;
    m_deviceRegistry.insert(rf8000.id, rf8000);

    // --- RF Power Meter RF3000 ---
    PMDeviceProperties rf3000;
    rf3000.id = "rf3000";
    rf3000.name = "RF3000 3GHz";
    rf3000.alternativeNames = "RF-Power 3000,RFPower3000";
    rf3000.imagePath = ":/images/devices/rf8000.png";
    rf3000.minFreqHz = 50000000;    // 50 MHz maybe require fix I don't know what is real lowest freq
    rf3000.maxFreqHz = 2999000000;  // 3000 MHz - assume that 2999 highest value as it is with rf8000
    rf3000.minPowerDbm = -45.0;
    rf3000.maxPowerDbm = 5.0;
    rf3000.hasOffset = true;
    rf3000.baudRate = 9600;
    rf3000.hasInternalAttenuator = false;
    rf3000.isEnabled = true;
    m_deviceRegistry.insert(rf3000.id, rf3000);

    // --- RF Power Meter RF500 ---
    PMDeviceProperties rf500;
    rf500.id = "rf500";
    rf500.name = "RF500 500MHz";
    rf500.alternativeNames = "RF-Power 500,RFPower500";
    rf500.imagePath = ":/images/devices/rf8000.png";
    rf500.minFreqHz = 1000000;      // 1 MHz
    rf500.maxFreqHz = 499000000;    // 500 MHz  - assume that 499 highest value as it is with rf8000
    rf500.minPowerDbm = -65.0;
    rf500.maxPowerDbm = 15.0;
    rf500.hasOffset = true;
    rf500.baudRate = 9600;
    rf500.hasInternalAttenuator = false;
    rf500.isEnabled = true;
    m_deviceRegistry.insert(rf500.id, rf500);

}

QList<PMDeviceProperties> PMDeviceFactory::availableDevices() const
{
    QList<PMDeviceProperties> enabledDevices;
    for (const auto &props : m_deviceRegistry.values()) {
        if (props.isEnabled) {
            enabledDevices.append(props);
        }
    }

    std::sort(enabledDevices.begin(), enabledDevices.end(), [](const PMDeviceProperties &a, const PMDeviceProperties &b) {
        return a.maxFreqHz > b.maxFreqHz;
    });
    return enabledDevices;
}

PMDeviceProperties PMDeviceFactory::propertiesForDevice(const QString &deviceId) const
{
    return m_deviceRegistry.value(deviceId, PMDeviceProperties());
}

AbstractPMDevice* PMDeviceFactory::createDevice(const QString &deviceId, QObject *parent)
{
    if (deviceId == "rfpmv5") {
        return new RfpmV5Device(propertiesForDevice(deviceId), parent);
    }

    if (deviceId == "rfpm_v7_10ghz") {
        return new RfpmV7Device(propertiesForDevice(deviceId), parent);
    }

    if (deviceId == "rf500" || deviceId == "rf3000" || deviceId == "rf8000") {
        return new Rf8000Device(propertiesForDevice(deviceId), parent);
    }
    return nullptr;
}
