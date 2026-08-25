// Published PSX ADPCM predictor coefficients (PSX-SPX "CD-ROM ADPCM").
#include "adpcm.hpp"

namespace spu {

const int8_t FILTER_C[5][2] = {
    {0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60},
};

}  // namespace spu
