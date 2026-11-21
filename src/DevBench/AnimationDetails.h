#pragma once

#include <QJsonObject>

namespace Animation {
struct ShaderValues;
}
struct ObjectState;

namespace DevBench {

// The animated shader values by name, with colors as red, green and blue arrays.
QJsonObject ShaderValuesDetails(const Animation::ShaderValues& a_values);

// Where a pose puts an object: its translation, the rows of its rotation, its scale and whether it is hidden.
QJsonObject ObjectStateDetails(const ObjectState& a_state);

}
