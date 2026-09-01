#include "Theme.h"

namespace frostyeq::theme
{

// One scheme for both modules. The design gives a single palette, and which
// module is loaded is said by the chooser rather than by the colour.
const Palette& palette() noexcept { return kLight; }

void setModel (Model) noexcept {}

} // namespace frostyeq::theme
