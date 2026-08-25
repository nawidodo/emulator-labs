#include "../shared/runner_cli.hpp"
#include "blend_stages.hpp"

int main(int argc, char** argv) {
    return psx::gpu::labs_runner_main<
        psx::gpu::GpuDevice<psx::gpu::BlendClipStages>>(argc, argv);
}
