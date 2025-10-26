#pragma once

#include <QObject>
#include <QJsonObject>
#include <QString>


class CableModel : public QObject
{
    Q_OBJECT
public:
    explicit CableModel(const QJsonObject &jsonData, QObject *parent = nullptr);
    ~CableModel();

    QString getName() const { return m_name; }
    QString getManufacturer() const { return m_manufacturer; }
    QString getType() const { return m_type; }
    QString getDataSource() const { return m_dataSource; }
    QJsonObject getAdditionalInfo() const { return m_additionalInfo; }
    double getMaxFrequency() const { return m_maxFrequency; }
    double getMinFrequency() const { return m_minFrequency; }

    double getAttenuationPer100m(double frequencyMHz) const;

private:
    void setupSpline(const QJsonObject &attenuationData);

    QString m_name;
    QString m_manufacturer;
    QString m_type;
    QString m_dataSource;
    QJsonObject m_additionalInfo;

    void* m_spline;
    bool m_splineValid = false;
    double m_maxFrequency = 0.0;
    double m_minFrequency = 0.0;
};
