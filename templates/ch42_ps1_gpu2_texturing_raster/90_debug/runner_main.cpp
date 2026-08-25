#include "../shared/runner_cli.hpp"
#include "debug_stages.hpp"

int main(int argc, char** argv) {
    return psx::gpu::labs_runner_main<
        psx::gpu::GpuDevice<psx::gpu::DebugStages>>(argc, argv);
}
