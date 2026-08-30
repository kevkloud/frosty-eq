#include "Theme.h"

namespace frostyeq::theme
{

namespace { const Palette* current = &k1073; }

const Palette& palette() noexcept { return *current; }

void setModel (Model m) noexcept
{
    current = (m == Model::m1084) ? &k1084 : &k1073;
}

} // namespace frostyeq::theme
