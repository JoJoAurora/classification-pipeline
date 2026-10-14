import numpy as np
from pipeline_common import (
    GRID_N,
    REJECT_TAU,
    load_split_scaled,
    quantize,
)

# Load surfaces and scaling bounds from training
surfaces = np.load("surfaces.npz")
data = load_split_scaled()
bounds = data["bounds"]  # bounds is (lo, hi) where lo=[xmin, ymin], hi=[xmax, ymax]

classes = sorted(int(k[1:]) for k in surfaces.files)
NUM_CLASSES = len(classes)

# Quantize each class surface to uint8 (0-255)
tables = []
for c in classes:
  q_surface = quantize(surfaces[f"c{c}"])
  tables.append(q_surface)

# Build the C header file contents matching the specification
header_content = f"""#ifndef MODEL_H
#define MODEL_H

#include <avr/pgmspace.h>

#define GRID_N {GRID_N}
#define NUM_CLASSES {NUM_CLASSES}
#define REJECT_TAU {REJECT_TAU}f

// Quantized class density tables stored in Flash memory
const uint8_t CLASS_TABLE[NUM_CLASSES][GRID_N][GRID_N] PROGMEM = {{
"""

for c_idx, table in enumerate(tables):
  header_content += f"    // Class {classes[c_idx]}\n    {{\n"
  for row in table:
    row_str = ", ".join(f"0x{val:02X}" for val in row)
    header_content += f"        {{{row_str}}},\n"
  header_content += "    },\n"

header_content += f"""}};

// Min-max scaling bounds: {{xmin, xmax, ymin, ymax}}
const float SCALE_BOUNDS[4] = {{
    {bounds[0][0]}f, {bounds[1][0]}f, {bounds[0][1]}f, {bounds[1][1]}f
}};

// Class identifier names
const char* const CLASS_NAMES[NUM_CLASSES] = {{
    """
header_content += ", ".join(f'"class {c}"' for c in classes)
header_content += """
};

#endif // MODEL_H
"""

# Write out to model.h in the src directory (or root as required)
with open("model.h", "w") as f:
  f.write(header_content)

total_bytes = NUM_CLASSES * GRID_N * GRID_N
print(f"Successfully generated model.h!")
print(f" > Grid Size (G): {GRID_N}x{GRID_N}")
print(f" > Number of Classes (K): {NUM_CLASSES}")
print(f" > Table Flash Footprint: {total_bytes}")
print(f" > Scaling Bounds Footprint: 16 bytes (4 floats)")