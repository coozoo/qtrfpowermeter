#include "cablemodel.h"
#include "spline.h"
#include <QJsonArray>
#include <QDebug>
#include <algorithm>
#include <vector>

CableModel::CableModel(const QJsonObject &jsonData, QObject *parent)
    : QObject{parent}
    , m_spline(new tk::spline())
{
    m_name = jsonData["name"].toString("Unknown");
    m_manufacturer = jsonData["manufacturer"].toString("N/A");
    m_type = jsonData["type"].toString("N/A");
    m_dataSource = jsonData["datasheet"].toString();
    m_additionalInfo = jsonData["extrainfo"].toObject();

    if (jsonData.contains("attenuation") && jsonData["attenuation"].isObject())
    {
        setupSpline(jsonData["attenuation"].toObject());
    }
}

CableModel::~CableModel()
{
    delete static_cast<tk::spline*>(m_spline);
}

double CableModel::getAttenuationPer100m(double frequencyMHz) const
{
    if (!m_splineValid)
    {
        return 0.0;
    }
    return (*static_cast<tk::spline*>(m_spline))(frequencyMHz);
}

void CableModel::setupSpline(const QJsonObject &attenuationData)
{
    std::vector<double> freqs;
    std::vector<double> losses;

    freqs.push_back(0.0);
    losses.push_back(0.0);

    for (auto it = attenuationData.begin(); it != attenuationData.end(); ++it)
    {
        bool ok;
        double freq = it.key().toDouble(&ok);
        if (ok && !it.value().isNull())
        {
            freqs.push_back(freq);
            losses.push_back(it.value().toDouble());
        }
    }

    if (freqs.size() < 2)
    {
        qWarning()<<"Not enough data points to create a spline for"<<m_name;
        m_splineValid = false;
        return;
    }

    std::vector<std::pair<double, double>> sorted_points;
    for (size_t i = 0; i < freqs.size(); ++i)
    {
        sorted_points.push_back({freqs[i], losses[i]});
    }
    std::sort(sorted_points.begin(), sorted_points.end(), [](const auto & a, const auto & b)
              {
                  return a.first < b.first;
              });

    freqs.clear();
    losses.clear();
    for (const auto &p : sorted_points)
    {
        freqs.push_back(p.first);
        losses.push_back(p.second);
    }

    m_minFrequency = freqs.at(1);
    m_maxFrequency = freqs.back();

    static_cast<tk::spline*>(m_spline)->set_points(freqs, losses, tk::spline::cspline);
    m_splineValid = true;
}
