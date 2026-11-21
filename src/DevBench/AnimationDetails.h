#pragma once

#include <QJsonObject>

namespace Animation {
struct ShaderValues;
}

namespace DevBench {

// The animated shader values by name, with colors as red, green and blue arrays.
QJsonObject ShaderValuesDetails(const Animation::ShaderValues& a_values);

}
